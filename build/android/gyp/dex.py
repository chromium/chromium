#!/usr/bin/env python3
#
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import collections
import logging
import os
import re
import shutil
import shlex
import sys
import tempfile
import zipfile

from util import build_utils
from util import md5_check
import action_helpers  # build_utils adds //build to sys.path.
import zip_helpers


_DEX_XMX = '2G'  # Increase this when __final_dex OOMs.

DEFAULT_IGNORE_WARNINGS = (
    # Warning: Running R8 version main (build engineering), which cannot be
    # represented as a semantic version. Using an artificial version newer than
    # any known version for selecting Proguard configurations embedded under
    # META-INF/. This means that all rules with a '-upto-' qualifier will be
    # excluded and all rules with a -from- qualifier will be included.
    r'Running R8 version main', )

_IGNORE_SERVICE_ENTRIES = (
    # ServiceLoader call is used only for ProtoBuf full (non-lite).
    # BaseGeneratedExtensionRegistryLite$Loader conflicts with
    # ChromeGeneratedExtensionRegistryLite$Loader.
    'META-INF/services/com.google.protobuf.GeneratedExtensionRegistryLoader', )

INTERFACE_DESUGARING_WARNINGS = (r'default or static interface methods', )

_SKIPPED_CLASS_FILE_NAMES = (
    'module-info.class',  # Explicitly skipped by r8/utils/FileUtils#isClassFile
)


def _ParseArgs(args):
  args = build_utils.ExpandFileArgs(args)
  parser = argparse.ArgumentParser()

  action_helpers.add_depfile_arg(parser)
  parser.add_argument('--output', required=True, help='Dex output path.')
  parser.add_argument(
      '--class-inputs',
      action='append',
      help='GN-list of .jars with .class files.')
  parser.add_argument(
      '--class-inputs-filearg',
      action='append',
      help='GN-list of .jars with .class files (added to depfile).')
  parser.add_argument(
      '--dex-inputs', action='append', help='GN-list of .jars with .dex files.')
  parser.add_argument(
      '--dex-inputs-filearg',
      action='append',
      help='GN-list of .jars with .dex files (added to depfile).')
  parser.add_argument(
      '--incremental-dir',
      help='Path of directory to put intermediate dex files.')
  parser.add_argument('--library',
                      action='store_true',
                      help='Allow numerous dex files within output.')
  parser.add_argument('--r8-jar-path', required=True, help='Path to R8 jar.')
  parser.add_argument('--skip-custom-d8',
                      action='store_true',
                      help='When rebuilding the CustomD8 jar, this may be '
                      'necessary to avoid incompatibility with the new r8 '
                      'jar.')
  parser.add_argument('--custom-d8-jar-path',
                      required=True,
                      help='Path to our customized d8 jar.')
  parser.add_argument('--desugar-dependencies',
                      help='Path to store desugar dependencies.')
  parser.add_argument('--desugar', action='store_true')
  parser.add_argument(
      '--bootclasspath',
      action='append',
      help='GN-list of bootclasspath. Needed for --desugar')
  parser.add_argument('--show-desugar-default-interface-warnings',
                      action='store_true',
                      help='Enable desugaring warnings.')
  parser.add_argument(
      '--classpath',
      action='append',
      help='GN-list of full classpath. Needed for --desugar')
  parser.add_argument('--release',
                      action='store_true',
                      help='Run D8 in release mode.')
  parser.add_argument(
      '--min-api', help='Minimum Android API level compatibility.')
  parser.add_argument('--force-enable-assertions',
                      action='store_true',
                      help='Forcefully enable javac generated assertion code.')
  parser.add_argument('--assertion-handler',
                      help='The class name of the assertion handler class.')
  parser.add_argument('--warnings-as-errors',
                      action='store_true',
                      help='Treat all warnings as errors.')
  parser.add_argument('--dump-inputs',
                      action='store_true',
                      help='Use when filing D8 bugs to capture inputs.'
                      ' Stores inputs to d8inputs.zip')
  options = parser.parse_args(args)

  if options.force_enable_assertions and options.assertion_handler:
    parser.error('Cannot use both --force-enable-assertions and '
                 '--assertion-handler')

  options.class_inputs = action_helpers.parse_gn_list(options.class_inputs)
  options.class_inputs_filearg = action_helpers.parse_gn_list(
      options.class_inputs_filearg)
  options.bootclasspath = action_helpers.parse_gn_list(options.bootclasspath)
  options.classpath = action_helpers.parse_gn_list(options.classpath)
  options.dex_inputs = action_helpers.parse_gn_list(options.dex_inputs)
  options.dex_inputs_filearg = action_helpers.parse_gn_list(
      options.dex_inputs_filearg)

  return options


def CreateStderrFilter(filters):
  def filter_stderr(output):
    # Set this when debugging R8 output.
    if os.environ.get('R8_SHOW_ALL_OUTPUT', '0') != '0':
      return output

    # All missing definitions are logged as a single warning, but start on a
    # new line like "Missing class ...".
    warnings = re.split(r'^(?=Warning|Error|Missing (?:class|field|method))',
                        output,
                        flags=re.MULTILINE)
    preamble, *warnings = warnings

    combined_pattern = '|'.join(filters)
    preamble = build_utils.FilterLines(preamble, combined_pattern)

    compiled_re = re.compile(combined_pattern, re.DOTALL)
    warnings = [w for w in warnings if not compiled_re.search(w)]

    return preamble + ''.join(warnings)

  return filter_stderr


def _RunD8(dex_cmd, input_paths, output_path, warnings_as_errors,
           show_desugar_default_interface_warnings):
  dex_cmd = dex_cmd + ['--output', output_path] + input_paths

  # Missing deps can happen for prebuilts that are missing transitive deps
  # and have set enable_bytecode_checks=false.
  filters = list(DEFAULT_IGNORE_WARNINGS)
  if not show_desugar_default_interface_warnings:
    filters += INTERFACE_DESUGARING_WARNINGS

  stderr_filter = CreateStderrFilter(filters)

  is_debug = logging.getLogger().isEnabledFor(logging.DEBUG)

  # Avoid deleting the flag file when DEX_DEBUG is set in case the flag file
  # needs to be examined after the build.
  with tempfile.NamedTemporaryFile(mode='w', delete=not is_debug) as flag_file:
    # Chosen arbitrarily. Needed to avoid command-line length limits.
    MAX_ARGS = 50
    orig_dex_cmd = dex_cmd
    if len(dex_cmd) > MAX_ARGS:
      # Add all flags to D8 (anything after the first --) as well as all
      # positional args at the end to the flag file.
      for idx, cmd in enumerate(dex_cmd):
        if cmd.startswith('--'):
          flag_file.write('\n'.join(dex_cmd[idx:]))
          flag_file.flush()
          dex_cmd = dex_cmd[:idx]
          dex_cmd.append('@' + flag_file.name)
          break

    # stdout sometimes spams with things like:
    # Stripped invalid locals information from 1 method.
    try:
      build_utils.CheckOutput(dex_cmd,
                              stderr_filter=stderr_filter,
                              fail_on_output=warnings_as_errors)
    except Exception as e:
      if isinstance(e, build_utils.CalledProcessError):
        output = e.output  # pylint: disable=no-member
        if "global synthetic for 'Record desugaring'" in output:
          sys.stderr.write('Java records are not supported.\n')
          sys.stderr.write(
              'See https://chromium.googlesource.com/chromium/src/+/' +
              'main/styleguide/java/java.md#Records\n')
          sys.exit(1)
      if orig_dex_cmd is not dex_cmd:
        sys.stderr.write('Full command: ' + shlex.join(orig_dex_cmd) + '\n')
      raise


def _ZipAligned(dex_files, output_path, services_map):
  """Creates a .dex.jar with 4-byte aligned files.

  Args:
    dex_files: List of dex files.
    output_path: The output file in which to write the zip.
    services_map: map of path->data for META-INF/services
  """
  with zipfile.ZipFile(output_path, 'w') as z:
    for i, dex_file in enumerate(dex_files):
      name = 'classes{}.dex'.format(i + 1 if i > 0 else '')
      zip_helpers.add_to_zip_hermetic(z, name, src_path=dex_file, alignment=4)
    for path, data in sorted(services_map.items()):
      zip_helpers.add_to_zip_hermetic(z, path, data=data, alignment=4)


def _CreateServicesMap(service_jars):
  ret = {}
  origins = {}
  for jar_path in service_jars:
    with zipfile.ZipFile(jar_path, 'r') as z:
      for n in z.namelist():
        if n.startswith('META-INF/services/') and not n.endswith('/'):
          if n in _IGNORE_SERVICE_ENTRIES:
            continue
          data = z.read(n)
          if ret.get(n, data) == data:
            ret[n] = data
            origins[n] = jar_path
          else:
            # We should arguably just concat the files here, but Chrome's own
            # uses (via ServiceLoaderUtil) all assume only one entry.
            raise Exception(f"""\
Conflicting contents for: {n}
{origins[n]}:
{ret[n]}
{jar_path}:
{data}

If this entry can be safely ignored (because the ServiceLoader.load() call is \
never hit), update _IGNORE_SERVICE_ENTRIES in dex.py.
""")
  return ret


def _CreateFinalDex(d8_inputs,
                    output,
                    tmp_dir,
                    dex_cmd,
                    options=None,
                    service_jars=None):
  tmp_dex_output = os.path.join(tmp_dir, 'tmp_dex_output.zip')
  needs_dexing = not all(f.endswith('.dex') for f in d8_inputs)
  needs_dexmerge = output.endswith('.dex') or not (options and options.library)
  services_map = _CreateServicesMap(service_jars or [])
  if needs_dexing or needs_dexmerge:
    tmp_dex_dir = os.path.join(tmp_dir, 'tmp_dex_dir')
    os.mkdir(tmp_dex_dir)

    _RunD8(dex_cmd, d8_inputs, tmp_dex_dir,
           (not options or options.warnings_as_errors),
           (options and options.show_desugar_default_interface_warnings))
    logging.debug('Performed dex merging')

    dex_files = [os.path.join(tmp_dex_dir, f) for f in os.listdir(tmp_dex_dir)]

    if output.endswith('.dex'):
      if len(dex_files) > 1:
        raise Exception('%d files created, expected 1' % len(dex_files))
      tmp_dex_output = dex_files[0]
    else:
      _ZipAligned(sorted(dex_files), tmp_dex_output, services_map)
  else:
    # Skip dexmerger. Just put all incrementals into the .jar individually.
    _ZipAligned(sorted(d8_inputs), tmp_dex_output, services_map)
    logging.debug('Quick-zipped %d files', len(d8_inputs))

  # The dex file is complete and can be moved out of tmp_dir.
  shutil.move(tmp_dex_output, output)


def _IntermediateDexFilePathsFromInputJars(class_inputs, incremental_dir):
  """Returns list of intermediate dex file paths, .jar files with services."""
  dex_files = []
  service_jars = set()
  for jar in class_inputs:
    with zipfile.ZipFile(jar, 'r') as z:
      for subpath in z.namelist():
        if _IsClassFile(subpath):
          subpath = subpath[:-5] + 'dex'
          dex_files.append(os.path.join(incremental_dir, subpath))
        elif subpath.startswith('META-INF/services/'):
          service_jars.add(jar)
  return dex_files, sorted(service_jars)


def _DeleteStaleIncrementalDexFiles(dex_dir, dex_files):
  """Deletes intermediate .dex files that are no longer needed."""
  all_files = build_utils.FindInDirectory(dex_dir)
  desired_files = set(dex_files)
  for path in all_files:
    if path not in desired_files:
      os.unlink(path)


def _ParseDesugarDeps(desugar_dependencies_file):
  # pylint: disable=line-too-long
  """Returns a dict of dependent/dependency mapping parsed from the file.

  Example file format:
  $ tail out/Debug/gen/base/base_java__dex.desugardeps
  org/chromium/base/task/SingleThreadTaskRunnerImpl.class
    <-  org/chromium/base/task/SingleThreadTaskRunner.class
    <-  org/chromium/base/task/TaskRunnerImpl.class
  org/chromium/base/task/TaskRunnerImpl.class
    <-  org/chromium/base/task/TaskRunner.class
  org/chromium/base/task/TaskRunnerImplJni$1.class
    <-  obj/base/jni_java.turbine.jar:org/jni_zero/JniStaticTestMocker.class
  org/chromium/base/task/TaskRunnerImplJni.class
    <-  org/chromium/base/task/TaskRunnerImpl$Natives.class
  """
  # pylint: enable=line-too-long
  dependents_from_dependency = collections.defaultdict(set)
  if desugar_dependencies_file and os.path.exists(desugar_dependencies_file):
    with open(desugar_dependencies_file, 'r') as f:
      dependent = None
      for line in f:
        line = line.rstrip()
        if line.startswith('  <-  '):
          dependency = line[len('  <-  '):]
          # Note that this is a reversed mapping from the one in CustomD8.java.
          dependents_from_dependency[dependency].add(dependent)
        else:
          dependent = line
  return dependents_from_dependency


def _ComputeRequiredDesugarClasses(changes, desugar_dependencies_file,
                                   class_inputs, classpath):
  dependents_from_dependency = _ParseDesugarDeps(desugar_dependencies_file)
  required_classes = set()
  # Gather classes that need to be re-desugared from changes in the classpath.
  for jar in classpath:
    for subpath in changes.IterChangedSubpaths(jar):
      dependency = '{}:{}'.format(jar, subpath)
      required_classes.update(dependents_from_dependency[dependency])

  for jar in class_inputs:
    for subpath in changes.IterChangedSubpaths(jar):
      required_classes.update(dependents_from_dependency[subpath])

  return required_classes


def _IsClassFile(path):
  if os.path.basename(path) in _SKIPPED_CLASS_FILE_NAMES:
    return False
  return path.endswith('.class')


def _ExtractClassFiles(changes, tmp_dir, class_inputs, required_classes_set):
  classes_list = []
  for jar in class_inputs:
    if changes:
      changed_class_list = (set(changes.IterChangedSubpaths(jar))
                            | required_classes_set)
      predicate = lambda x: x in changed_class_list and _IsClassFile(x)
    else:
      predicate = _IsClassFile

    classes_list.extend(
        build_utils.ExtractAll(jar, path=tmp_dir, predicate=predicate))
  return classes_list


def _CreateIntermediateDexFiles(changes, options, tmp_dir, dex_cmd):
  # Create temporary directory for classes to be extracted to.
  tmp_extract_dir = os.path.join(tmp_dir, 'tmp_extract_dir')
  os.mkdir(tmp_extract_dir)

  # Do a full rebuild when changes occur in non-input files.
  allowed_changed = set(options.class_inputs)
  allowed_changed.update(options.dex_inputs)
  allowed_changed.update(options.classpath)
  strings_changed = changes.HasStringChanges()
  non_direct_input_changed = next(
      (p for p in changes.IterChangedPaths() if p not in allowed_changed), None)

  if strings_changed or non_direct_input_changed:
    logging.debug('Full dex required: strings_changed=%s path_changed=%s',
                  strings_changed, non_direct_input_changed)
    changes = None

  if changes is None:
    required_desugar_classes_set = set()
  else:
    required_desugar_classes_set = _ComputeRequiredDesugarClasses(
        changes, options.desugar_dependencies, options.class_inputs,
        options.classpath)
    logging.debug('Class files needing re-desugar: %d',
                  len(required_desugar_classes_set))
  class_files = _ExtractClassFiles(changes, tmp_extract_dir,
                                   options.class_inputs,
                                   required_desugar_classes_set)
  logging.debug('Extracted class files: %d', len(class_files))

  # If the only change is deleting a file, class_files will be empty.
  if class_files:
    # Dex necessary classes into intermediate dex files.
    dex_cmd = dex_cmd + ['--intermediate', '--file-per-class-file']
    if options.desugar_dependencies and not options.skip_custom_d8:
      # Adding os.sep to remove the entire prefix.
      dex_cmd += ['--file-tmp-prefix', tmp_extract_dir + os.sep]
      if changes is None and os.path.exists(options.desugar_dependencies):
        # Since incremental dexing only ever adds to the desugar_dependencies
        # file, whenever full dexes are required the .desugardeps files need to
        # be manually removed.
        os.unlink(options.desugar_dependencies)
    _RunD8(dex_cmd, class_files, options.incremental_dir,
           options.warnings_as_errors,
           options.show_desugar_default_interface_warnings)
    logging.debug('Dexed class files.')


def _OnStaleMd5(changes, options, final_dex_inputs, service_jars, dex_cmd):
  logging.debug('_OnStaleMd5')
  with build_utils.TempDir() as tmp_dir:
    if options.incremental_dir:
      # Create directory for all intermediate dex files.
      if not os.path.exists(options.incremental_dir):
        os.makedirs(options.incremental_dir)

      _DeleteStaleIncrementalDexFiles(options.incremental_dir, final_dex_inputs)
      logging.debug('Stale files deleted')
      _CreateIntermediateDexFiles(changes, options, tmp_dir, dex_cmd)

    _CreateFinalDex(final_dex_inputs,
                    options.output,
                    tmp_dir,
                    dex_cmd,
                    options=options,
                    service_jars=service_jars)


def MergeDexForIncrementalInstall(r8_jar_path, src_paths, dest_dex_jar,
                                  min_api):
  dex_cmd = build_utils.JavaCmd(xmx=_DEX_XMX) + [
      '-cp',
      r8_jar_path,
      'com.android.tools.r8.D8',
      '--min-api',
      min_api,
  ]
  with build_utils.TempDir() as tmp_dir:
    _CreateFinalDex(src_paths,
                    dest_dex_jar,
                    tmp_dir,
                    dex_cmd,
                    service_jars=src_paths)


def main(args):
  build_utils.InitLogging('DEX_DEBUG')
  options = _ParseArgs(args)

  options.class_inputs += options.class_inputs_filearg
  options.dex_inputs += options.dex_inputs_filearg

  input_paths = ([
      build_utils.JAVA_PATH_FOR_INPUTS, options.r8_jar_path,
      options.custom_d8_jar_path
  ] + options.class_inputs + options.dex_inputs)

  depfile_deps = options.class_inputs_filearg + options.dex_inputs_filearg

  output_paths = [options.output]

  track_subpaths_allowlist = []
  if options.incremental_dir:
    final_dex_inputs, service_jars = _IntermediateDexFilePathsFromInputJars(
        options.class_inputs, options.incremental_dir)
    output_paths += final_dex_inputs
    track_subpaths_allowlist += options.class_inputs
  else:
    final_dex_inputs = list(options.class_inputs)
    service_jars = final_dex_inputs
  service_jars += options.dex_inputs
  final_dex_inputs += options.dex_inputs

  dex_cmd = build_utils.JavaCmd(xmx=_DEX_XMX)

  if options.dump_inputs:
    dex_cmd += ['-Dcom.android.tools.r8.dumpinputtofile=d8inputs.zip']

  if not options.skip_custom_d8:
    dex_cmd += [
        '-cp',
        '{}:{}'.format(options.r8_jar_path, options.custom_d8_jar_path),
        'org.chromium.build.CustomD8',
    ]
  else:
    dex_cmd += [
        '-cp',
        options.r8_jar_path,
        'com.android.tools.r8.D8',
    ]

  if options.release:
    dex_cmd += ['--release']
  if options.min_api:
    dex_cmd += ['--min-api', options.min_api]

  if not options.desugar:
    dex_cmd += ['--no-desugaring']
  elif options.classpath:
    # The classpath is used by D8 to for interface desugaring.
    if options.desugar_dependencies and not options.skip_custom_d8:
      dex_cmd += ['--desugar-dependencies', options.desugar_dependencies]
      if track_subpaths_allowlist:
        track_subpaths_allowlist += options.classpath
    depfile_deps += options.classpath
    input_paths += options.classpath
    # Still pass the entire classpath in case a new dependency is needed by
    # desugar, so that desugar_dependencies will be updated for the next build.
    for path in options.classpath:
      dex_cmd += ['--classpath', path]

  if options.classpath:
    dex_cmd += ['--lib', build_utils.JAVA_HOME]
    for path in options.bootclasspath:
      dex_cmd += ['--lib', path]
    depfile_deps += options.bootclasspath
    input_paths += options.bootclasspath


  if options.assertion_handler:
    dex_cmd += ['--force-assertions-handler:' + options.assertion_handler]
  if options.force_enable_assertions:
    dex_cmd += ['--force-enable-assertions']

  # The changes feature from md5_check allows us to only re-dex the class files
  # that have changed and the class files that need to be re-desugared by D8.
  md5_check.CallAndWriteDepfileIfStale(
      lambda changes: _OnStaleMd5(changes, options, final_dex_inputs,
                                  service_jars, dex_cmd),
      options,
      input_paths=input_paths,
      input_strings=dex_cmd + [str(bool(options.incremental_dir))],
      output_paths=output_paths,
      pass_changes=True,
      track_subpaths_allowlist=track_subpaths_allowlist,
      depfile_deps=depfile_deps)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
