#!/usr/bin/env python3
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import collections
import logging
import os
import re
import shutil
import sys
import tempfile
import zipfile

from util import build_utils
from util import md5_check
from util import zipalign

sys.path.insert(1, os.path.join(os.path.dirname(__file__), os.path.pardir))

import convert_dex_profile


_DEX_XMX = '2G'  # Increase this when __final_dex OOMs.

_IGNORE_WARNINGS = (
    # Caused by Play Services:
    r'Type `libcore.io.Memory` was not found',
    # Caused by flogger supporting these as fallbacks. Not needed at runtime.
    r'Type `dalvik.system.VMStack` was not found',
    r'Type `sun.misc.SharedSecrets` was not found',
    # Caused by jacoco code coverage:
    r'Type `java.lang.management.ManagementFactory` was not found',
    # TODO(wnwen): Remove this after R8 version 3.0.26-dev:
    r'Missing class sun.misc.Unsafe',
    # Caused when the test apk and the apk under test do not having native libs.
    r'Missing class org.chromium.build.NativeLibraries',
    # Caused by internal annotation: https://crbug.com/1180222
    r'Missing class com.google.errorprone.annotations.RestrictedInheritance',
    # Caused by internal protobuf package: https://crbug.com/1183971
    r'referenced from: com.google.protobuf.GeneratedMessageLite$GeneratedExtension',  # pylint: disable=line-too-long
    # Caused by using Bazel desugar instead of D8 for desugar, since Bazel
    # desugar doesn't preserve interfaces in the same way. This should be
    # removed when D8 is used for desugaring.
    r'Warning: Cannot emulate interface ',
    # Only relevant for R8 when optimizing an app that doesn't use proto.
    r'Ignoring -shrinkunusedprotofields since the protobuf-lite runtime is',
)


def _ParseArgs(args):
  args = build_utils.ExpandFileArgs(args)
  parser = argparse.ArgumentParser()

  build_utils.AddDepfileOption(parser)
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
  parser.add_argument('--main-dex-rules-path',
                      action='append',
                      help='Path to main dex rules for multidex.')
  parser.add_argument(
      '--multi-dex',
      action='store_true',
      help='Allow multiple dex files within output.')
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
  parser.add_argument(
      '--desugar-jdk-libs-json', help='Path to desugar_jdk_libs.json.')
  parser.add_argument('--show-desugar-default-interface-warnings',
                      action='store_true',
                      help='Enable desugaring warnings.')
  parser.add_argument(
      '--classpath',
      action='append',
      help='GN-list of full classpath. Needed for --desugar')
  parser.add_argument(
      '--release',
      action='store_true',
      help='Run D8 in release mode. Release mode maximises main dex and '
      'deletes non-essential line number information (vs debug which minimizes '
      'main dex and keeps all line number information, and then some.')
  parser.add_argument(
      '--min-api', help='Minimum Android API level compatibility.')
  parser.add_argument('--force-enable-assertions',
                      action='store_true',
                      help='Forcefully enable javac generated assertion code.')
  parser.add_argument('--warnings-as-errors',
                      action='store_true',
                      help='Treat all warnings as errors.')
  parser.add_argument('--dump-inputs',
                      action='store_true',
                      help='Use when filing D8 bugs to capture inputs.'
                      ' Stores inputs to d8inputs.zip')

  group = parser.add_argument_group('Dexlayout')
  group.add_argument(
      '--dexlayout-profile',
      help=('Text profile for dexlayout. If present, a dexlayout '
            'pass will happen'))
  group.add_argument(
      '--profman-path',
      help=('Path to ART profman binary. There should be a lib/ directory at '
            'the same path with shared libraries (shared with dexlayout).'))
  group.add_argument(
      '--dexlayout-path',
      help=('Path to ART dexlayout binary. There should be a lib/ directory at '
            'the same path with shared libraries (shared with dexlayout).'))
  group.add_argument('--dexdump-path', help='Path to dexdump binary.')
  group.add_argument(
      '--proguard-mapping-path',
      help=('Path to proguard map from obfuscated symbols in the jar to '
            'unobfuscated symbols present in the code. If not present, the jar '
            'is assumed not to be obfuscated.'))

  options = parser.parse_args(args)

  if options.dexlayout_profile:
    build_utils.CheckOptions(
        options,
        parser,
        required=('profman_path', 'dexlayout_path', 'dexdump_path'))
  elif options.proguard_mapping_path is not None:
    parser.error('Unexpected proguard mapping without dexlayout')

  if options.main_dex_rules_path and not options.multi_dex:
    parser.error('--main-dex-rules-path is unused if multidex is not enabled')

  options.class_inputs = build_utils.ParseGnList(options.class_inputs)
  options.class_inputs_filearg = build_utils.ParseGnList(
      options.class_inputs_filearg)
  options.bootclasspath = build_utils.ParseGnList(options.bootclasspath)
  options.classpath = build_utils.ParseGnList(options.classpath)
  options.dex_inputs = build_utils.ParseGnList(options.dex_inputs)
  options.dex_inputs_filearg = build_utils.ParseGnList(
      options.dex_inputs_filearg)

  return options


def CreateStderrFilter(show_desugar_default_interface_warnings):
  def filter_stderr(output):
    patterns = list(_IGNORE_WARNINGS)

    # When using Bazel's Desugar tool to desugar lambdas and interface methods,
    # we do not provide D8 with a classpath, which causes a lot of warnings from
    # D8's default interface desugaring pass. Not having a classpath makes
    # incremental dexing much more effective. D8 still does backported method
    # desugaring.
    # These warnings are also turned off when bytecode checks are turned off.
    if not show_desugar_default_interface_warnings:
      patterns += ['default or static interface methods']

    combined_pattern = '|'.join(re.escape(p) for p in patterns)
    output = build_utils.FilterLines(output, combined_pattern)

    # Each warning has a prefix line of the file it's from. If we've filtered
    # out the warning, then also filter out the file header.
    # E.g.:
    # Warning in path/to/Foo.class:
    #   Error message #1 indented here.
    #   Error message #2 indented here.
    output = re.sub(r'^Warning in .*?:\n(?!  )', '', output, flags=re.MULTILINE)
    return output

  return filter_stderr


def _RunD8(dex_cmd, input_paths, output_path, warnings_as_errors,
           show_desugar_default_interface_warnings):
  dex_cmd = dex_cmd + ['--output', output_path] + input_paths

  stderr_filter = CreateStderrFilter(show_desugar_default_interface_warnings)

  with tempfile.NamedTemporaryFile(mode='w') as flag_file:
    # Chosen arbitrarily. Needed to avoid command-line length limits.
    MAX_ARGS = 50
    if len(dex_cmd) > MAX_ARGS:
      flag_file.write('\n'.join(dex_cmd[MAX_ARGS:]))
      flag_file.flush()
      dex_cmd = dex_cmd[:MAX_ARGS]
      dex_cmd.append('@' + flag_file.name)

    # stdout sometimes spams with things like:
    # Stripped invalid locals information from 1 method.
    build_utils.CheckOutput(dex_cmd,
                            stderr_filter=stderr_filter,
                            fail_on_output=warnings_as_errors)


def _EnvWithArtLibPath(binary_path):
  """Return an environment dictionary for ART host shared libraries.

  Args:
    binary_path: the path to an ART host binary.

  Returns:
    An environment dictionary where LD_LIBRARY_PATH has been augmented with the
    shared library path for the binary. This assumes that there is a lib/
    directory in the same location as the binary.
  """
  lib_path = os.path.join(os.path.dirname(binary_path), 'lib')
  env = os.environ.copy()
  libraries = [l for l in env.get('LD_LIBRARY_PATH', '').split(':') if l]
  libraries.append(lib_path)
  env['LD_LIBRARY_PATH'] = ':'.join(libraries)
  return env


def _CreateBinaryProfile(text_profile, input_dex, profman_path, temp_dir):
  """Create a binary profile for dexlayout.

  Args:
    text_profile: The ART text profile that will be converted to a binary
        profile.
    input_dex: The input dex file to layout.
    profman_path: Path to the profman binary.
    temp_dir: Directory to work in.

  Returns:
    The name of the binary profile, which will live in temp_dir.
  """
  binary_profile = os.path.join(
      temp_dir, 'binary_profile-for-' + os.path.basename(text_profile))
  open(binary_profile, 'w').close()  # Touch binary_profile.
  profman_cmd = [profman_path,
                 '--apk=' + input_dex,
                 '--dex-location=' + input_dex,
                 '--create-profile-from=' + text_profile,
                 '--reference-profile-file=' + binary_profile]
  build_utils.CheckOutput(
    profman_cmd,
    env=_EnvWithArtLibPath(profman_path),
    stderr_filter=lambda output:
        build_utils.FilterLines(output, '|'.join(
            [r'Could not find (method_id|proto_id|name):',
             r'Could not create type list'])))
  return binary_profile


def _LayoutDex(binary_profile, input_dex, dexlayout_path, temp_dir):
  """Layout a dexfile using a profile.

  Args:
    binary_profile: An ART binary profile, eg output from _CreateBinaryProfile.
    input_dex: The dex file used to create the binary profile.
    dexlayout_path: Path to the dexlayout binary.
    temp_dir: Directory to work in.

  Returns:
    List of output files produced by dexlayout. This will be one if the input
    was a single dexfile, or multiple files if the input was a multidex
    zip. These output files are located in temp_dir.
  """
  dexlayout_output_dir = os.path.join(temp_dir, 'dexlayout_output')
  os.mkdir(dexlayout_output_dir)
  dexlayout_cmd = [ dexlayout_path,
                    '-u',  # Update checksum
                    '-p', binary_profile,
                    '-w', dexlayout_output_dir,
                    input_dex ]
  build_utils.CheckOutput(
      dexlayout_cmd,
      env=_EnvWithArtLibPath(dexlayout_path),
      stderr_filter=lambda output:
          build_utils.FilterLines(output,
                                  r'Can.t mmap dex file.*please zipalign'))
  output_files = os.listdir(dexlayout_output_dir)
  if not output_files:
    raise Exception('dexlayout unexpectedly produced no output')
  return sorted([os.path.join(dexlayout_output_dir, f) for f in output_files])


def _ZipMultidex(file_dir, dex_files):
  """Zip dex files into a multidex.

  Args:
    file_dir: The directory into which to write the output.
    dex_files: The dexfiles forming the multizip. Their names must end with
      classes.dex, classes2.dex, ...

  Returns:
    The name of the multidex file, which will live in file_dir.
  """
  ordered_files = []  # List of (archive name, file name)
  for f in dex_files:
    if f.endswith('dex.jar'):
      ordered_files.append(('classes.dex', f))
      break
  if not ordered_files:
    raise Exception('Could not find classes.dex multidex file in %s',
                    dex_files)
  for dex_idx in xrange(2, len(dex_files) + 1):
    archive_name = 'classes%d.dex' % dex_idx
    for f in dex_files:
      if f.endswith(archive_name):
        ordered_files.append((archive_name, f))
        break
    else:
      raise Exception('Could not find classes%d.dex multidex file in %s',
                      dex_files)
  if len(set(f[1] for f in ordered_files)) != len(ordered_files):
    raise Exception('Unexpected clashing filenames for multidex in %s',
                    dex_files)

  zip_name = os.path.join(file_dir, 'multidex_classes.zip')
  build_utils.DoZip(((archive_name, os.path.join(file_dir, file_name))
                     for archive_name, file_name in ordered_files),
                    zip_name)
  return zip_name


def _ZipAligned(dex_files, output_path):
  """Creates a .dex.jar with 4-byte aligned files.

  Args:
    dex_files: List of dex files.
    output_path: The output file in which to write the zip.
  """
  with zipfile.ZipFile(output_path, 'w') as z:
    for i, dex_file in enumerate(dex_files):
      name = 'classes{}.dex'.format(i + 1 if i > 0 else '')
      zipalign.AddToZipHermetic(z, name, src_path=dex_file, alignment=4)


def _PerformDexlayout(tmp_dir, tmp_dex_output, options):
  if options.proguard_mapping_path is not None:
    matching_profile = os.path.join(tmp_dir, 'obfuscated_profile')
    convert_dex_profile.ObfuscateProfile(
        options.dexlayout_profile, tmp_dex_output,
        options.proguard_mapping_path, options.dexdump_path, matching_profile)
  else:
    logging.warning('No obfuscation for %s', options.dexlayout_profile)
    matching_profile = options.dexlayout_profile
  binary_profile = _CreateBinaryProfile(matching_profile, tmp_dex_output,
                                        options.profman_path, tmp_dir)
  output_files = _LayoutDex(binary_profile, tmp_dex_output,
                            options.dexlayout_path, tmp_dir)
  if len(output_files) > 1:
    return _ZipMultidex(tmp_dir, output_files)

  if zipfile.is_zipfile(output_files[0]):
    return output_files[0]

  final_output = os.path.join(tmp_dir, 'dex_classes.zip')
  _ZipAligned(output_files, final_output)
  return final_output


def _CreateFinalDex(d8_inputs, output, tmp_dir, dex_cmd, options=None):
  tmp_dex_output = os.path.join(tmp_dir, 'tmp_dex_output.zip')
  needs_dexing = not all(f.endswith('.dex') for f in d8_inputs)
  needs_dexmerge = output.endswith('.dex') or not (options and options.library)
  if needs_dexing or needs_dexmerge:
    if options and options.main_dex_rules_path:
      for main_dex_rule in options.main_dex_rules_path:
        dex_cmd = dex_cmd + ['--main-dex-rules', main_dex_rule]

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
      _ZipAligned(sorted(dex_files), tmp_dex_output)
  else:
    # Skip dexmerger. Just put all incrementals into the .jar individually.
    _ZipAligned(sorted(d8_inputs), tmp_dex_output)
    logging.debug('Quick-zipped %d files', len(d8_inputs))

  if options and options.dexlayout_profile:
    tmp_dex_output = _PerformDexlayout(tmp_dir, tmp_dex_output, options)

  # The dex file is complete and can be moved out of tmp_dir.
  shutil.move(tmp_dex_output, output)


def _IntermediateDexFilePathsFromInputJars(class_inputs, incremental_dir):
  """Returns a list of all intermediate dex file paths."""
  dex_files = []
  for jar in class_inputs:
    with zipfile.ZipFile(jar, 'r') as z:
      for subpath in z.namelist():
        if subpath.endswith('.class'):
          subpath = subpath[:-5] + 'dex'
          dex_files.append(os.path.join(incremental_dir, subpath))
  return dex_files


def _DeleteStaleIncrementalDexFiles(dex_dir, dex_files):
  """Deletes intermediate .dex files that are no longer needed."""
  all_files = build_utils.FindInDirectory(dex_dir)
  desired_files = set(dex_files)
  for path in all_files:
    if path not in desired_files:
      os.unlink(path)


def _ParseDesugarDeps(desugar_dependencies_file):
  dependents_from_dependency = collections.defaultdict(set)
  if desugar_dependencies_file and os.path.exists(desugar_dependencies_file):
    with open(desugar_dependencies_file, 'r') as f:
      for line in f:
        dependent, dependency = line.rstrip().split(' -> ')
        dependents_from_dependency[dependency].add(dependent)
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


def _ExtractClassFiles(changes, tmp_dir, class_inputs, required_classes_set):
  classes_list = []
  for jar in class_inputs:
    if changes:
      changed_class_list = (set(changes.IterChangedSubpaths(jar))
                            | required_classes_set)
      predicate = lambda x: x in changed_class_list and x.endswith('.class')
    else:
      predicate = lambda x: x.endswith('.class')

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

  if changes:
    required_desugar_classes_set = _ComputeRequiredDesugarClasses(
        changes, options.desugar_dependencies, options.class_inputs,
        options.classpath)
    logging.debug('Class files needing re-desugar: %d',
                  len(required_desugar_classes_set))
  else:
    required_desugar_classes_set = set()
  class_files = _ExtractClassFiles(changes, tmp_extract_dir,
                                   options.class_inputs,
                                   required_desugar_classes_set)
  logging.debug('Extracted class files: %d', len(class_files))

  # If the only change is deleting a file, class_files will be empty.
  if class_files:
    # Dex necessary classes into intermediate dex files.
    dex_cmd = dex_cmd + ['--intermediate', '--file-per-class-file']
    if options.desugar_dependencies and not options.skip_custom_d8:
      dex_cmd += ['--file-tmp-prefix', tmp_extract_dir]
    _RunD8(dex_cmd, class_files, options.incremental_dir,
           options.warnings_as_errors,
           options.show_desugar_default_interface_warnings)
    logging.debug('Dexed class files.')


def _OnStaleMd5(changes, options, final_dex_inputs, dex_cmd):
  logging.debug('_OnStaleMd5')
  with build_utils.TempDir() as tmp_dir:
    if options.incremental_dir:
      # Create directory for all intermediate dex files.
      if not os.path.exists(options.incremental_dir):
        os.makedirs(options.incremental_dir)

      _DeleteStaleIncrementalDexFiles(options.incremental_dir, final_dex_inputs)
      logging.debug('Stale files deleted')
      _CreateIntermediateDexFiles(changes, options, tmp_dir, dex_cmd)

    _CreateFinalDex(
        final_dex_inputs, options.output, tmp_dir, dex_cmd, options=options)


def MergeDexForIncrementalInstall(r8_jar_path, src_paths, dest_dex_jar,
                                  min_api):
  dex_cmd = build_utils.JavaCmd(verify=False, xmx=_DEX_XMX) + [
      '-cp',
      r8_jar_path,
      'com.android.tools.r8.D8',
      '--min-api',
      min_api,
  ]
  with build_utils.TempDir() as tmp_dir:
    _CreateFinalDex(src_paths, dest_dex_jar, tmp_dir, dex_cmd)


def main(args):
  build_utils.InitLogging('DEX_DEBUG')
  options = _ParseArgs(args)

  options.class_inputs += options.class_inputs_filearg
  options.dex_inputs += options.dex_inputs_filearg

  input_paths = options.class_inputs + options.dex_inputs
  input_paths.append(options.r8_jar_path)
  input_paths.append(options.custom_d8_jar_path)
  if options.main_dex_rules_path:
    input_paths.extend(options.main_dex_rules_path)

  depfile_deps = options.class_inputs_filearg + options.dex_inputs_filearg

  output_paths = [options.output]

  track_subpaths_allowlist = []
  if options.incremental_dir:
    final_dex_inputs = _IntermediateDexFilePathsFromInputJars(
        options.class_inputs, options.incremental_dir)
    output_paths += final_dex_inputs
    track_subpaths_allowlist += options.class_inputs
  else:
    final_dex_inputs = list(options.class_inputs)
  final_dex_inputs += options.dex_inputs

  dex_cmd = build_utils.JavaCmd(options.warnings_as_errors, xmx=_DEX_XMX)

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

  if options.classpath or options.main_dex_rules_path:
    # --main-dex-rules requires bootclasspath.
    dex_cmd += ['--lib', build_utils.JAVA_HOME]
    for path in options.bootclasspath:
      dex_cmd += ['--lib', path]
    depfile_deps += options.bootclasspath
    input_paths += options.bootclasspath


  if options.desugar_jdk_libs_json:
    dex_cmd += ['--desugared-lib', options.desugar_jdk_libs_json]
  if options.force_enable_assertions:
    dex_cmd += ['--force-enable-assertions']

  # The changes feature from md5_check allows us to only re-dex the class files
  # that have changed and the class files that need to be re-desugared by D8.
  md5_check.CallAndWriteDepfileIfStale(
      lambda changes: _OnStaleMd5(changes, options, final_dex_inputs, dex_cmd),
      options,
      input_paths=input_paths,
      input_strings=dex_cmd + [bool(options.incremental_dir)],
      output_paths=output_paths,
      pass_changes=True,
      track_subpaths_allowlist=track_subpaths_allowlist,
      depfile_deps=depfile_deps)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
