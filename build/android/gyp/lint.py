#!/usr/bin/env python3
#
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Runs Android's lint tool."""

from __future__ import print_function

import argparse
import logging
import os
import shutil
import sys
import time
from xml.dom import minidom
from xml.etree import ElementTree

from util import build_utils
from util import manifest_utils
from util import server_utils

_LINT_MD_URL = 'https://chromium.googlesource.com/chromium/src/+/main/build/android/docs/lint.md'  # pylint: disable=line-too-long

# These checks are not useful for chromium.
_DISABLED_ALWAYS = [
    "AppCompatResource",  # Lint does not correctly detect our appcompat lib.
    "Assert",  # R8 --force-enable-assertions is used to enable java asserts.
    "InflateParams",  # Null is ok when inflating views for dialogs.
    "InlinedApi",  # Constants are copied so they are always available.
    "LintBaseline",  # Don't warn about using baseline.xml files.
    "MissingInflatedId",  # False positives https://crbug.com/1394222
    "MissingApplicationIcon",  # False positive for non-production targets.
    "ObsoleteLintCustomCheck",  # We have no control over custom lint checks.
    "SwitchIntDef",  # Many C++ enums are not used at all in java.
    "Typos",  # Strings are committed in English first and later translated.
    "UniqueConstants",  # Chromium enums allow aliases.
    "UnusedAttribute",  # Chromium apks have various minSdkVersion values.
]

# These checks are not useful for test targets and adds an unnecessary burden
# to suppress them.
_DISABLED_FOR_TESTS = [
    # We should not require test strings.xml files to explicitly add
    # translatable=false since they are not translated and not used in
    # production.
    "MissingTranslation",
    # Test strings.xml files often have simple names and are not translatable,
    # so it may conflict with a production string and cause this error.
    "Untranslatable",
    # Test targets often use the same strings target and resources target as the
    # production targets but may not use all of them.
    "UnusedResources",
    # TODO(wnwen): Turn this back on since to crash it would require running on
    #     a device with all the various minSdkVersions.
    # Real NewApi violations crash the app, so the only ones that lint catches
    # but tests still succeed are false positives.
    "NewApi",
    # Tests should be allowed to access these methods/classes.
    "VisibleForTests",
]

_RES_ZIP_DIR = 'RESZIPS'
_SRCJAR_DIR = 'SRCJARS'
_AAR_DIR = 'AARS'


def _SrcRelative(path):
  """Returns relative path to top-level src dir."""
  return os.path.relpath(path, build_utils.DIR_SOURCE_ROOT)


def _GenerateProjectFile(android_manifest,
                         android_sdk_root,
                         cache_dir,
                         sources=None,
                         classpath=None,
                         srcjar_sources=None,
                         resource_sources=None,
                         custom_lint_jars=None,
                         custom_annotation_zips=None,
                         android_sdk_version=None,
                         baseline_path=None):
  project = ElementTree.Element('project')
  root = ElementTree.SubElement(project, 'root')
  # Run lint from output directory: crbug.com/1115594
  root.set('dir', os.getcwd())
  sdk = ElementTree.SubElement(project, 'sdk')
  # Lint requires that the sdk path be an absolute path.
  sdk.set('dir', os.path.abspath(android_sdk_root))
  if baseline_path is not None:
    baseline = ElementTree.SubElement(project, 'baseline')
    baseline.set('file', baseline_path)
  cache = ElementTree.SubElement(project, 'cache')
  cache.set('dir', cache_dir)
  main_module = ElementTree.SubElement(project, 'module')
  main_module.set('name', 'main')
  main_module.set('android', 'true')
  main_module.set('library', 'false')
  if android_sdk_version:
    main_module.set('compile_sdk_version', android_sdk_version)
  manifest = ElementTree.SubElement(main_module, 'manifest')
  manifest.set('file', android_manifest)
  if srcjar_sources:
    for srcjar_file in srcjar_sources:
      src = ElementTree.SubElement(main_module, 'src')
      src.set('file', srcjar_file)
  if sources:
    for source in sources:
      src = ElementTree.SubElement(main_module, 'src')
      src.set('file', source)
  if classpath:
    for file_path in classpath:
      classpath_element = ElementTree.SubElement(main_module, 'classpath')
      classpath_element.set('file', file_path)
  if resource_sources:
    for resource_file in resource_sources:
      resource = ElementTree.SubElement(main_module, 'resource')
      resource.set('file', resource_file)
  if custom_lint_jars:
    for lint_jar in custom_lint_jars:
      lint = ElementTree.SubElement(main_module, 'lint-checks')
      lint.set('file', lint_jar)
  if custom_annotation_zips:
    for annotation_zip in custom_annotation_zips:
      annotation = ElementTree.SubElement(main_module, 'annotations')
      annotation.set('file', annotation_zip)
  return project


def _RetrieveBackportedMethods(backported_methods_path):
  with open(backported_methods_path) as f:
    methods = f.read().splitlines()
  # Methods look like:
  #   java/util/Set#of(Ljava/lang/Object;)Ljava/util/Set;
  # But error message looks like:
  #   Call requires API level R (current min is 21): java.util.Set#of [NewApi]
  methods = (m.replace('/', '\\.') for m in methods)
  methods = (m[:m.index('(')] for m in methods)
  return sorted(set(methods))


def _GenerateConfigXmlTree(orig_config_path, backported_methods):
  if orig_config_path:
    root_node = ElementTree.parse(orig_config_path).getroot()
  else:
    root_node = ElementTree.fromstring('<lint/>')

  issue_node = ElementTree.SubElement(root_node, 'issue')
  issue_node.attrib['id'] = 'NewApi'
  ignore_node = ElementTree.SubElement(issue_node, 'ignore')
  ignore_node.attrib['regexp'] = '|'.join(backported_methods)
  return root_node


def _GenerateAndroidManifest(original_manifest_path, extra_manifest_paths,
                             min_sdk_version, android_sdk_version):
  # Set minSdkVersion in the manifest to the correct value.
  doc, manifest, app_node = manifest_utils.ParseManifest(original_manifest_path)

  # TODO(crbug.com/1126301): Should this be done using manifest merging?
  # Add anything in the application node of the extra manifests to the main
  # manifest to prevent unused resource errors.
  for path in extra_manifest_paths:
    _, _, extra_app_node = manifest_utils.ParseManifest(path)
    for node in extra_app_node:
      app_node.append(node)

  uses_sdk = manifest.find('./uses-sdk')
  if uses_sdk is None:
    uses_sdk = ElementTree.Element('uses-sdk')
    manifest.insert(0, uses_sdk)
  uses_sdk.set('{%s}minSdkVersion' % manifest_utils.ANDROID_NAMESPACE,
               min_sdk_version)
  uses_sdk.set('{%s}targetSdkVersion' % manifest_utils.ANDROID_NAMESPACE,
               android_sdk_version)
  return doc


def _WriteXmlFile(root, path):
  logging.info('Writing xml file %s', path)
  build_utils.MakeDirectory(os.path.dirname(path))
  with build_utils.AtomicOutput(path) as f:
    # Although we can write it just with ElementTree.tostring, using minidom
    # makes it a lot easier to read as a human (also on code search).
    f.write(
        minidom.parseString(ElementTree.tostring(
            root, encoding='utf-8')).toprettyxml(indent='  ').encode('utf-8'))


def _RunLint(create_cache,
             custom_lint_jar_path,
             lint_jar_path,
             backported_methods_path,
             config_path,
             manifest_path,
             extra_manifest_paths,
             sources,
             classpath,
             cache_dir,
             android_sdk_version,
             aars,
             srcjars,
             min_sdk_version,
             resource_sources,
             resource_zips,
             android_sdk_root,
             lint_gen_dir,
             baseline,
             testonly_target=False,
             warnings_as_errors=False):
  logging.info('Lint starting')

  if create_cache:
    # Occasionally lint may crash due to re-using intermediate files from older
    # lint runs. See https://crbug.com/1258178 for context.
    logging.info('Clearing cache dir %s before creating cache.', cache_dir)
    shutil.rmtree(cache_dir, ignore_errors=True)
    os.makedirs(cache_dir)

  if baseline and not os.path.exists(baseline):
    # Generating new baselines is only done locally, and requires more memory to
    # avoid OOMs.
    creating_baseline = True
    lint_xmx = '4G'
  else:
    creating_baseline = False
    lint_xmx = '2G'

  # All paths in lint are based off of relative paths from root with root as the
  # prefix. Path variable substitution is based off of prefix matching so custom
  # path variables need to match exactly in order to show up in baseline files.
  # e.g. lint_path=path/to/output/dir/../../file/in/src
  root_path = os.getcwd()  # This is usually the output directory.
  pathvar_src = os.path.join(
      root_path, os.path.relpath(build_utils.DIR_SOURCE_ROOT, start=root_path))

  cmd = build_utils.JavaCmd(xmx=lint_xmx) + [
      '-cp',
      '{}:{}'.format(lint_jar_path, custom_lint_jar_path),
      'org.chromium.build.CustomLint',
      '--sdk-home',
      android_sdk_root,
      '--jdk-home',
      build_utils.JAVA_HOME,
      '--path-variables',
      f'SRC={pathvar_src}',
      '--quiet',  # Silences lint's "." progress updates.
      '--stacktrace',  # Prints full stacktraces for internal lint errors.
      '--disable',
      ','.join(_DISABLED_ALWAYS),
  ]

  if testonly_target:
    cmd.extend(['--disable', ','.join(_DISABLED_FOR_TESTS)])

  if not manifest_path:
    manifest_path = os.path.join(build_utils.DIR_SOURCE_ROOT, 'build',
                                 'android', 'AndroidManifest.xml')

  logging.info('Generating config.xml')
  backported_methods = _RetrieveBackportedMethods(backported_methods_path)
  config_xml_node = _GenerateConfigXmlTree(config_path, backported_methods)
  generated_config_path = os.path.join(lint_gen_dir, 'config.xml')
  _WriteXmlFile(config_xml_node, generated_config_path)
  cmd.extend(['--config', generated_config_path])

  logging.info('Generating Android manifest file')
  android_manifest_tree = _GenerateAndroidManifest(manifest_path,
                                                   extra_manifest_paths,
                                                   min_sdk_version,
                                                   android_sdk_version)
  # Include the rebased manifest_path in the lint generated path so that it is
  # clear in error messages where the original AndroidManifest.xml came from.
  lint_android_manifest_path = os.path.join(lint_gen_dir, manifest_path)
  _WriteXmlFile(android_manifest_tree.getroot(), lint_android_manifest_path)

  resource_root_dir = os.path.join(lint_gen_dir, _RES_ZIP_DIR)
  # These are zip files with generated resources (e. g. strings from GRD).
  logging.info('Extracting resource zips')
  for resource_zip in resource_zips:
    # Use a consistent root and name rather than a temporary file so that
    # suppressions can be local to the lint target and the resource target.
    resource_dir = os.path.join(resource_root_dir, resource_zip)
    shutil.rmtree(resource_dir, True)
    os.makedirs(resource_dir)
    resource_sources.extend(
        build_utils.ExtractAll(resource_zip, path=resource_dir))

  logging.info('Extracting aars')
  aar_root_dir = os.path.join(lint_gen_dir, _AAR_DIR)
  custom_lint_jars = []
  custom_annotation_zips = []
  if aars:
    for aar in aars:
      # Use relative source for aar files since they are not generated.
      aar_dir = os.path.join(aar_root_dir,
                             os.path.splitext(_SrcRelative(aar))[0])
      shutil.rmtree(aar_dir, True)
      os.makedirs(aar_dir)
      aar_files = build_utils.ExtractAll(aar, path=aar_dir)
      for f in aar_files:
        if f.endswith('lint.jar'):
          custom_lint_jars.append(f)
        elif f.endswith('annotations.zip'):
          custom_annotation_zips.append(f)

  logging.info('Extracting srcjars')
  srcjar_root_dir = os.path.join(lint_gen_dir, _SRCJAR_DIR)
  srcjar_sources = []
  if srcjars:
    for srcjar in srcjars:
      # Use path without extensions since otherwise the file name includes
      # .srcjar and lint treats it as a srcjar.
      srcjar_dir = os.path.join(srcjar_root_dir, os.path.splitext(srcjar)[0])
      shutil.rmtree(srcjar_dir, True)
      os.makedirs(srcjar_dir)
      # Sadly lint's srcjar support is broken since it only considers the first
      # srcjar. Until we roll a lint version with that fixed, we need to extract
      # it ourselves.
      srcjar_sources.extend(build_utils.ExtractAll(srcjar, path=srcjar_dir))

  logging.info('Generating project file')
  project_file_root = _GenerateProjectFile(lint_android_manifest_path,
                                           android_sdk_root, cache_dir, sources,
                                           classpath, srcjar_sources,
                                           resource_sources, custom_lint_jars,
                                           custom_annotation_zips,
                                           android_sdk_version, baseline)

  project_xml_path = os.path.join(lint_gen_dir, 'project.xml')
  _WriteXmlFile(project_file_root, project_xml_path)
  cmd += ['--project', project_xml_path]

  # This filter is necessary for JDK11.
  stderr_filter = build_utils.FilterReflectiveAccessJavaWarnings
  stdout_filter = lambda x: build_utils.FilterLines(x, 'No issues found')

  start = time.time()
  logging.debug('Lint command %s', ' '.join(cmd))
  failed = True

  if creating_baseline and not warnings_as_errors:
    # Allow error code 6 when creating a baseline: ERRNO_CREATED_BASELINE
    fail_func = lambda returncode, _: returncode not in (0, 6)
  else:
    fail_func = lambda returncode, _: returncode != 0

  try:
    failed = bool(
        build_utils.CheckOutput(cmd,
                                print_stdout=True,
                                stdout_filter=stdout_filter,
                                stderr_filter=stderr_filter,
                                fail_on_output=warnings_as_errors,
                                fail_func=fail_func))
  finally:
    # When not treating warnings as errors, display the extra footer.
    is_debug = os.environ.get('LINT_DEBUG', '0') != '0'

    if failed:
      print('- For more help with lint in Chrome:', _LINT_MD_URL)
      if is_debug:
        print('- DEBUG MODE: Here is the project.xml: {}'.format(
            _SrcRelative(project_xml_path)))
      else:
        print('- Run with LINT_DEBUG=1 to enable lint configuration debugging')

    end = time.time() - start
    logging.info('Lint command took %ss', end)
    if not is_debug:
      shutil.rmtree(aar_root_dir, ignore_errors=True)
      shutil.rmtree(resource_root_dir, ignore_errors=True)
      shutil.rmtree(srcjar_root_dir, ignore_errors=True)
      os.unlink(project_xml_path)

  logging.info('Lint completed')


def _ParseArgs(argv):
  parser = argparse.ArgumentParser()
  build_utils.AddDepfileOption(parser)
  parser.add_argument('--target-name', help='Fully qualified GN target name.')
  parser.add_argument('--skip-build-server',
                      action='store_true',
                      help='Avoid using the build server.')
  parser.add_argument('--use-build-server',
                      action='store_true',
                      help='Always use the build server.')
  parser.add_argument('--lint-jar-path',
                      required=True,
                      help='Path to the lint jar.')
  parser.add_argument('--custom-lint-jar-path',
                      required=True,
                      help='Path to our custom lint jar.')
  parser.add_argument('--backported-methods',
                      help='Path to backported methods file created by R8.')
  parser.add_argument('--cache-dir',
                      required=True,
                      help='Path to the directory in which the android cache '
                      'directory tree should be stored.')
  parser.add_argument('--config-path', help='Path to lint suppressions file.')
  parser.add_argument('--lint-gen-dir',
                      required=True,
                      help='Path to store generated xml files.')
  parser.add_argument('--stamp', help='Path to stamp upon success.')
  parser.add_argument('--android-sdk-version',
                      help='Version (API level) of the Android SDK used for '
                      'building.')
  parser.add_argument('--min-sdk-version',
                      required=True,
                      help='Minimal SDK version to lint against.')
  parser.add_argument('--android-sdk-root',
                      required=True,
                      help='Lint needs an explicit path to the android sdk.')
  parser.add_argument('--testonly',
                      action='store_true',
                      help='If set, some checks like UnusedResources will be '
                      'disabled since they are not helpful for test '
                      'targets.')
  parser.add_argument('--create-cache',
                      action='store_true',
                      help='Whether this invocation is just warming the cache.')
  parser.add_argument('--warnings-as-errors',
                      action='store_true',
                      help='Treat all warnings as errors.')
  parser.add_argument('--java-sources',
                      help='File containing a list of java sources files.')
  parser.add_argument('--aars', help='GN list of included aars.')
  parser.add_argument('--srcjars', help='GN list of included srcjars.')
  parser.add_argument('--manifest-path',
                      help='Path to original AndroidManifest.xml')
  parser.add_argument('--extra-manifest-paths',
                      action='append',
                      help='GYP-list of manifest paths to merge into the '
                      'original AndroidManifest.xml')
  parser.add_argument('--resource-sources',
                      default=[],
                      action='append',
                      help='GYP-list of resource sources files, similar to '
                      'java sources files, but for resource files.')
  parser.add_argument('--resource-zips',
                      default=[],
                      action='append',
                      help='GYP-list of resource zips, zip files of generated '
                      'resource files.')
  parser.add_argument('--classpath',
                      help='List of jars to add to the classpath.')
  parser.add_argument('--baseline',
                      help='Baseline file to ignore existing errors and fail '
                      'on new errors.')

  args = parser.parse_args(build_utils.ExpandFileArgs(argv))
  args.java_sources = build_utils.ParseGnList(args.java_sources)
  args.aars = build_utils.ParseGnList(args.aars)
  args.srcjars = build_utils.ParseGnList(args.srcjars)
  args.resource_sources = build_utils.ParseGnList(args.resource_sources)
  args.extra_manifest_paths = build_utils.ParseGnList(args.extra_manifest_paths)
  args.resource_zips = build_utils.ParseGnList(args.resource_zips)
  args.classpath = build_utils.ParseGnList(args.classpath)

  if args.baseline:
    assert os.path.basename(args.baseline) == 'lint-baseline.xml', (
        'The baseline file needs to be named "lint-baseline.xml" in order for '
        'the autoroller to find and update it whenever lint is rolled to a new '
        'version.')

  return args


def main():
  build_utils.InitLogging('LINT_DEBUG')
  args = _ParseArgs(sys.argv[1:])

  # TODO(wnwen): Consider removing lint cache now that there are only two lint
  #              invocations.
  # Avoid parallelizing cache creation since lint runs without the cache defeat
  # the purpose of creating the cache in the first place.
  if (not args.create_cache and not args.skip_build_server
      and server_utils.MaybeRunCommand(name=args.target_name,
                                       argv=sys.argv,
                                       stamp_file=args.stamp,
                                       force=args.use_build_server)):
    return

  sources = []
  for java_sources_file in args.java_sources:
    sources.extend(build_utils.ReadSourcesList(java_sources_file))
  resource_sources = []
  for resource_sources_file in args.resource_sources:
    resource_sources.extend(build_utils.ReadSourcesList(resource_sources_file))

  possible_depfile_deps = (args.srcjars + args.resource_zips + sources +
                           resource_sources + [
                               args.baseline,
                               args.manifest_path,
                           ])
  depfile_deps = [p for p in possible_depfile_deps if p]

  _RunLint(args.create_cache,
           args.custom_lint_jar_path,
           args.lint_jar_path,
           args.backported_methods,
           args.config_path,
           args.manifest_path,
           args.extra_manifest_paths,
           sources,
           args.classpath,
           args.cache_dir,
           args.android_sdk_version,
           args.aars,
           args.srcjars,
           args.min_sdk_version,
           resource_sources,
           args.resource_zips,
           args.android_sdk_root,
           args.lint_gen_dir,
           args.baseline,
           testonly_target=args.testonly,
           warnings_as_errors=args.warnings_as_errors)
  logging.info('Creating stamp file')
  build_utils.Touch(args.stamp)

  if args.depfile:
    build_utils.WriteDepfile(args.depfile, args.stamp, depfile_deps)


if __name__ == '__main__':
  sys.exit(main())
