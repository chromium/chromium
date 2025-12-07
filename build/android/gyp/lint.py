#!/usr/bin/env python3
#
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Runs Android's lint tool."""

import argparse
import logging
import os
import shutil
import sys
import time
from xml.dom import minidom
from xml.etree import ElementTree

from util import build_utils
from util import server_utils
import action_helpers  # build_utils adds //build to sys.path.

_LINT_MD_URL = 'https://chromium.googlesource.com/chromium/src/+/main/build/android/docs/lint.md'

# These checks are not useful for chromium.
_DISABLED_ALWAYS = [
    "AppCompatResource",  # Lint does not correctly detect our appcompat lib.
    "AppLinkUrlError",  # As a browser, we have intent filters without a host.
    "Assert",  # R8 --force-enable-assertions is used to enable java asserts.
    "InflateParams",  # Null is ok when inflating views for dialogs.
    # Android apps are associated with domains of the same owner. Chrome uses
    # the Credential Manager API to support filling *any* site with a third
    # party password manager. Therefore, the list of sign-in domains would be
    # infinite and this warning must be suppressed.
    "CredentialManagerMisuse",
    "CredManMissingDal",  # Has false-positives, TODO(crbug.com/420855219).
    "InlinedApi",  # Constants are copied so they are always available.
    "LintBaseline",  # Don't warn about using baseline.xml files.
    "LintBaselineFixed",  # We dont care if baseline.xml has unused entries.
    "MissingInflatedId",  # False positives https://crbug.com/1394222
    "MissingApplicationIcon",  # False positive for non-production targets.
    "MissingVersion",  # We set versions via aapt2, which runs after lint.
    "NetworkSecurityConfig",  # Breaks on library certificates b/269783280.
    "ObsoleteLintCustomCheck",  # We have no control over custom lint checks.
    "OldTargetApi",  # We sometimes need targetSdkVersion to not be latest.
    "PrivateResource",  # Triggers on our own R.java files.
    "StringFormatCount",  # Has false-positives.
    "SwitchIntDef",  # Many C++ enums are not used at all in java.
    "Typos",  # Strings are committed in English first and later translated.
    "VisibleForTests",  # Does not recognize "ForTesting" methods.
    "UniqueConstants",  # Chromium enums allow aliases.
    "UnusedAttribute",  # Chromium apks have various minSdkVersion values.
    "UnusedTranslation",  # Triggers from .aar files with extra translations.
]

_RES_ZIP_DIR = 'RESZIPS'
_SRCJAR_DIR = 'SRCJARS'


def _SrcRelative(path):
  """Returns relative path to top-level src dir."""
  return os.path.relpath(path, build_utils.DIR_SOURCE_ROOT)


def _GenerateProjectFile(android_manifest,
                         android_sdk_root,
                         cache_dir,
                         partials_dir,
                         sources=None,
                         classpath=None,
                         srcjar_sources=None,
                         resource_sources=None,
                         aars=None,
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
  # Required to make lint-resources.xml be written to a per-target path.
  # https://crbug.com/1515070 and b/324598620
  main_module.set('partial-results-dir', partials_dir)
  if android_sdk_version:
    main_module.set('compile_sdk_version', android_sdk_version)
  # Cache task has no manifest.
  if android_manifest:
    manifest = ElementTree.SubElement(main_module, 'merged-manifest')
    manifest.set('file', android_manifest)
  if srcjar_sources:
    srcjar_sources = sorted(set(srcjar_sources))  # Ensure these are unique.
    for srcjar_file in srcjar_sources:
      src = ElementTree.SubElement(main_module, 'src')
      src.set('file', srcjar_file)
      # Cannot add generated="true" since then lint does not scan them, and
      # we get "UnusedResources" lint errors when resources are used only by
      # generated files.
  if sources:
    sources = sorted(set(sources))  # Ensure these are unique.
    for source in sources:
      src = ElementTree.SubElement(main_module, 'src')
      src.set('file', source)
      # Cannot set test="true" since we sometimes put Test.java files beside
      # non-test files, which lint does not allow:
      # "Test sources cannot be in the same source root as production files"
  if classpath:
    # The classpath entries should already be unique and order matters.
    for file_path in classpath:
      classpath_element = ElementTree.SubElement(main_module, 'classpath')
      classpath_element.set('file', file_path)
  if resource_sources:
    resource_sources = sorted(set(resource_sources))  # Ensure these are unique.
    for resource_file in resource_sources:
      resource = ElementTree.SubElement(main_module, 'resource')
      resource.set('file', resource_file)
  if aars:
    aars = sorted(set(aars))  # Ensure these are unique.
    for aar in aars:
      lint = ElementTree.SubElement(main_module, 'aar')
      lint.set('file', aar)
  return project


def _RetrieveBackportedMethods(backported_methods_path):
  with open(backported_methods_path, encoding='utf-8') as f:
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


def _WriteXmlFile(root, path):
  logging.info('Writing xml file %s', path)
  build_utils.MakeDirectory(os.path.dirname(path))
  with action_helpers.atomic_output(path, encoding='utf-8') as f:
    # Although we can write it just with ElementTree.tostring, using minidom
    # makes it a lot easier to read as a human (also on code search).
    f.write(
        minidom.parseString(ElementTree.tostring(
            root, encoding='utf-8')).toprettyxml(indent='  '))


def _RunLint(lint_jar_path,
             backported_methods_path,
             config_path,
             sources,
             classpath,
             cache_dir,
             android_sdk_version,
             aars,
             srcjars,
             resource_sources,
             resource_zips,
             android_sdk_root,
             lint_gen_dir,
             baseline,
             create_cache,
             manifest_path,
             warnings_as_errors=False):
  logging.info('Lint starting')
  if not cache_dir:
    # Use per-target cache directory when --cache-dir is not used.
    cache_dir = os.path.join(lint_gen_dir, 'cache')
    # Lint complains if the directory does not exist.
    # When --create-cache is used, ninja will create this directory because the
    # stamp file is created within it.
    os.makedirs(cache_dir, exist_ok=True)

  if baseline and not os.path.exists(baseline):
    # Generating new baselines is only done locally, and requires more memory to
    # avoid OOMs.
    creating_baseline = True
    lint_xmx = '6G'
  else:
    creating_baseline = False
    lint_xmx = '4G'

  # Lint requires this directory to exist and be cleared. See b/324598620.
  partials_dir = os.path.join(lint_gen_dir, 'partials')
  shutil.rmtree(partials_dir, ignore_errors=True)
  os.makedirs(partials_dir)

  # All paths in lint are based off of relative paths from root with root as the
  # prefix. Path variable substitution is based off of prefix matching so custom
  # path variables need to match exactly in order to show up in baseline files.
  # e.g. lint_path=path/to/output/dir/../../file/in/src
  root_path = os.getcwd()  # This is usually the output directory.
  pathvar_src = os.path.join(
      root_path, os.path.relpath(build_utils.DIR_SOURCE_ROOT, start=root_path))

  cmd = build_utils.JavaCmd(xmx=lint_xmx) + [
      '-cp',
      lint_jar_path,
      'com.android.tools.lint.Main',
      '--sdk-home',
      android_sdk_root,
      '--jdk-home',
      build_utils.JAVA_HOME,
      '--path-variables',
      f'SRC={pathvar_src}',
      '--offline',
      '--quiet',  # Silences lint's "." progress updates.
      '--stacktrace',  # Prints full stacktraces for internal lint errors.
  ]

  # Only disable for real runs since otherwise you get UnknownIssueId warnings
  # when disabling custom lint checks since they are not passed during cache
  # creation.
  if not create_cache:
    cmd += [
        '--disable',
        ','.join(_DISABLED_ALWAYS),
    ]

  logging.info('Generating config.xml')
  backported_methods = _RetrieveBackportedMethods(backported_methods_path)
  config_xml_node = _GenerateConfigXmlTree(config_path, backported_methods)
  generated_config_path = os.path.join(lint_gen_dir, 'config.xml')
  _WriteXmlFile(config_xml_node, generated_config_path)
  cmd.extend(['--config', generated_config_path])

  resource_root_dir = os.path.join(lint_gen_dir, _RES_ZIP_DIR)
  shutil.rmtree(resource_root_dir, True)
  # These are zip files with generated resources (e. g. strings from GRD).
  logging.info('Extracting resource zips')
  for resource_zip in resource_zips:
    # Use a consistent root and name rather than a temporary file so that
    # suppressions can be local to the lint target and the resource target.
    resource_dir = os.path.join(resource_root_dir, resource_zip)
    os.makedirs(resource_dir)
    resource_sources.extend(
        build_utils.ExtractAll(resource_zip, path=resource_dir))

  logging.info('Extracting srcjars')
  srcjar_root_dir = os.path.join(lint_gen_dir, _SRCJAR_DIR)
  shutil.rmtree(srcjar_root_dir, True)
  srcjar_sources = []
  if srcjars:
    for srcjar in srcjars:
      # Use path without extensions since otherwise the file name includes
      # .srcjar and lint treats it as a srcjar.
      srcjar_dir = os.path.join(srcjar_root_dir, os.path.splitext(srcjar)[0])
      os.makedirs(srcjar_dir)
      # Sadly lint's srcjar support is broken since it only considers the first
      # srcjar. Until we roll a lint version with that fixed, we need to extract
      # it ourselves.
      srcjar_sources.extend(build_utils.ExtractAll(srcjar, path=srcjar_dir))

  logging.info('Generating project file')
  project_file_root = _GenerateProjectFile(manifest_path, android_sdk_root,
                                           cache_dir, partials_dir, sources,
                                           classpath, srcjar_sources,
                                           resource_sources, aars,
                                           android_sdk_version, baseline)

  project_xml_path = os.path.join(lint_gen_dir, 'project.xml')
  _WriteXmlFile(project_file_root, project_xml_path)
  cmd += ['--project', project_xml_path]

  def stdout_filter(output):
    filter_patterns = [
        # This filter is necessary for JDK11.
        'No issues found',
        # Custom checks are not always available in every lint run so an
        # UnknownIssueId warning is sometimes printed for custom checks in the
        # _DISABLED_ALWAYS list.
        r'\[UnknownIssueId\]',
        # If all the warnings are filtered, we should not fail on the final
        # summary line.
        r'\d+ errors?, \d+ warnings?',
    ]
    return build_utils.FilterLines(output, '|'.join(filter_patterns))

  start = time.time()
  failed = False

  if creating_baseline and not warnings_as_errors:
    # Allow error code 6 when creating a baseline: ERRNO_CREATED_BASELINE
    fail_func = lambda returncode, _: returncode not in (0, 6)
  else:
    fail_func = lambda returncode, _: returncode != 0

  try:
    build_utils.CheckOutput(cmd,
                            print_stdout=True,
                            stdout_filter=stdout_filter,
                            fail_on_output=warnings_as_errors,
                            fail_func=fail_func)
  except build_utils.CalledProcessError as e:
    failed = True
    # Do not output the python stacktrace because it is lengthy and is not
    # relevant to the actual lint error.
    sys.stderr.write(e.output)
  finally:
    # When not treating warnings as errors, display the extra footer.
    is_debug = os.environ.get('LINT_DEBUG', '0') != '0'

    end = time.time() - start
    logging.info('Lint command took %ss', end)
    if not is_debug:
      shutil.rmtree(resource_root_dir, ignore_errors=True)
      shutil.rmtree(srcjar_root_dir, ignore_errors=True)
      if os.path.exists(project_xml_path):
        os.unlink(project_xml_path)
      shutil.rmtree(partials_dir, ignore_errors=True)

    if failed:
      print('- For more help with lint in Chrome:', _LINT_MD_URL)
      if is_debug:
        print('- DEBUG MODE: Here is the project.xml: {}'.format(
            _SrcRelative(project_xml_path)))
      else:
        print('- Run with LINT_DEBUG=1 to enable lint configuration debugging')
      sys.exit(1)

  logging.info('Lint completed')


def _ParseArgs(argv):
  parser = argparse.ArgumentParser()
  action_helpers.add_depfile_arg(parser)
  parser.add_argument('--target-name', help='Fully qualified GN target name.')
  parser.add_argument('--use-build-server',
                      action='store_true',
                      help='Always use the build server.')
  parser.add_argument('--lint-jar-path',
                      required=True,
                      help='Path to the lint jar.')
  parser.add_argument('--backported-methods',
                      help='Path to backported methods file created by R8.')
  parser.add_argument('--cache-dir',
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
  parser.add_argument('--android-sdk-root',
                      required=True,
                      help='Lint needs an explicit path to the android sdk.')
  parser.add_argument('--create-cache',
                      action='store_true',
                      help='Whether this invocation is just warming the cache.')
  parser.add_argument('--warnings-as-errors',
                      action='store_true',
                      help='Treat all warnings as errors.')
  parser.add_argument('--sources',
                      help='A list of files containing java and kotlin source '
                      'files.')
  parser.add_argument('--aars', help='GN list of included aars.')
  parser.add_argument('--srcjars', help='GN list of included srcjars.')
  parser.add_argument('--manifest',
                      help='Path to the merged AndroidManifest.xml.')
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
  args.sources = action_helpers.parse_gn_list(args.sources)
  args.aars = action_helpers.parse_gn_list(args.aars)
  args.srcjars = action_helpers.parse_gn_list(args.srcjars)
  args.resource_sources = action_helpers.parse_gn_list(args.resource_sources)
  args.resource_zips = action_helpers.parse_gn_list(args.resource_zips)
  args.classpath = action_helpers.parse_gn_list(args.classpath)

  if args.baseline:
    assert os.path.basename(args.baseline) == 'lint-baseline.xml', (
        'The baseline file needs to be named "lint-baseline.xml" in order for '
        'the autoroller to find and update it whenever lint is rolled to a new '
        'version.')

  return args


def main():
  build_utils.InitLogging('LINT_DEBUG')
  args = _ParseArgs(sys.argv[1:])

  sources = []
  for sources_file in args.sources:
    sources.extend(build_utils.ReadSourcesList(sources_file))
  resource_sources = []
  for resource_sources_file in args.resource_sources:
    resource_sources.extend(build_utils.ReadSourcesList(resource_sources_file))

  possible_depfile_deps = (args.srcjars + args.resource_zips + sources +
                           resource_sources + [args.baseline, args.manifest])
  depfile_deps = [p for p in possible_depfile_deps if p]

  if args.depfile:
    action_helpers.write_depfile(args.depfile, args.stamp, depfile_deps)

  # Avoid parallelizing cache creation since lint runs without the cache defeat
  # the purpose of creating the cache in the first place. Forward the command
  # after the depfile has been written as siso requires it.
  if (not args.create_cache
      and server_utils.MaybeRunCommand(name=args.target_name,
                                       argv=sys.argv,
                                       stamp_file=args.stamp,
                                       use_build_server=args.use_build_server)):
    return

  _RunLint(args.lint_jar_path,
           args.backported_methods,
           args.config_path,
           sources,
           args.classpath,
           args.cache_dir,
           args.android_sdk_version,
           args.aars,
           args.srcjars,
           resource_sources,
           args.resource_zips,
           args.android_sdk_root,
           args.lint_gen_dir,
           args.baseline,
           args.create_cache,
           args.manifest,
           warnings_as_errors=args.warnings_as_errors)
  logging.info('Creating stamp file')
  server_utils.MaybeTouch(args.stamp)


if __name__ == '__main__':
  sys.exit(main())
