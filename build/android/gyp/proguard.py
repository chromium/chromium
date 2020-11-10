#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
from collections import defaultdict
import logging
import os
import re
import shutil
import sys
import tempfile
import zipfile

import dex
import dex_jdk_libs
from util import build_utils
from util import diff_utils

_API_LEVEL_VERSION_CODE = [
    (21, 'L'),
    (22, 'LolliopoMR1'),
    (23, 'M'),
    (24, 'N'),
    (25, 'NMR1'),
    (26, 'O'),
    (27, 'OMR1'),
    (28, 'P'),
    (29, 'Q'),
    (30, 'R'),
]


def _ParseOptions():
  args = build_utils.ExpandFileArgs(sys.argv[1:])
  parser = argparse.ArgumentParser()
  build_utils.AddDepfileOption(parser)
  parser.add_argument('--r8-path',
                      required=True,
                      help='Path to the R8.jar to use.')
  parser.add_argument(
      '--desugar-jdk-libs-json', help='Path to desugar_jdk_libs.json.')
  parser.add_argument('--input-paths',
                      action='append',
                      required=True,
                      help='GN-list of .jar files to optimize.')
  parser.add_argument('--desugar-jdk-libs-jar',
                      help='Path to desugar_jdk_libs.jar.')
  parser.add_argument('--desugar-jdk-libs-configuration-jar',
                      help='Path to desugar_jdk_libs_configuration.jar.')
  parser.add_argument('--output-path', help='Path to the generated .jar file.')
  parser.add_argument(
      '--proguard-configs',
      action='append',
      required=True,
      help='GN-list of configuration files.')
  parser.add_argument(
      '--apply-mapping', help='Path to ProGuard mapping to apply.')
  parser.add_argument(
      '--mapping-output',
      required=True,
      help='Path for ProGuard to output mapping file to.')
  parser.add_argument(
      '--extra-mapping-output-paths',
      help='GN-list of additional paths to copy output mapping file to.')
  parser.add_argument(
      '--classpath',
      action='append',
      help='GN-list of .jar files to include as libraries.')
  parser.add_argument(
      '--main-dex-rules-path',
      action='append',
      help='Path to main dex rules for multidex'
      '- only works with R8.')
  parser.add_argument(
      '--min-api', help='Minimum Android API level compatibility.')
  parser.add_argument(
      '--verbose', '-v', action='store_true', help='Print all ProGuard output')
  parser.add_argument(
      '--repackage-classes', help='Package all optimized classes are put in.')
  parser.add_argument(
      '--disable-outlining',
      action='store_true',
      help='Disable the outlining optimization provided by R8.')
  parser.add_argument(
    '--disable-checks',
    action='store_true',
    help='Disable -checkdiscard directives and missing symbols check')
  parser.add_argument('--sourcefile', help='Value for source file attribute')
  parser.add_argument(
      '--force-enable-assertions',
      action='store_true',
      help='Forcefully enable javac generated assertion code.')
  parser.add_argument(
      '--feature-jars',
      action='append',
      help='GN list of path to jars which comprise the corresponding feature.')
  parser.add_argument(
      '--dex-dest',
      action='append',
      dest='dex_dests',
      help='Destination for dex file of the corresponding feature.')
  parser.add_argument(
      '--feature-name',
      action='append',
      dest='feature_names',
      help='The name of the feature module.')
  parser.add_argument(
      '--uses-split',
      action='append',
      help='List of name pairs separated by : mapping a feature module to a '
      'dependent feature module.')
  parser.add_argument('--warnings-as-errors',
                      action='store_true',
                      help='Treat all warnings as errors.')
  parser.add_argument('--show-desugar-default-interface-warnings',
                      action='store_true',
                      help='Enable desugaring warnings.')
  parser.add_argument(
      '--stamp',
      help='File to touch upon success. Mutually exclusive with --output-path')
  parser.add_argument('--desugared-library-keep-rule-output',
                      help='Path to desugared library keep rule output file.')

  diff_utils.AddCommandLineFlags(parser)
  options = parser.parse_args(args)

  if options.feature_names:
    if options.output_path:
      parser.error('Feature splits cannot specify an output in GN.')
    if not options.actual_file and not options.stamp:
      parser.error('Feature splits require a stamp file as output.')
  elif not options.output_path:
    parser.error('Output path required when feature splits aren\'t used')

  options.classpath = build_utils.ParseGnList(options.classpath)
  options.proguard_configs = build_utils.ParseGnList(options.proguard_configs)
  options.input_paths = build_utils.ParseGnList(options.input_paths)
  options.extra_mapping_output_paths = build_utils.ParseGnList(
      options.extra_mapping_output_paths)

  if options.feature_names:
    if 'base' not in options.feature_names:
      parser.error('"base" feature required when feature arguments are used.')
    if len(options.feature_names) != len(options.feature_jars) or len(
        options.feature_names) != len(options.dex_dests):
      parser.error('Invalid feature argument lengths.')

    options.feature_jars = [
        build_utils.ParseGnList(x) for x in options.feature_jars
    ]

  split_map = {}
  if options.uses_split:
    for split_pair in options.uses_split:
      child, parent = split_pair.split(':')
      for name in (child, parent):
        if name not in options.feature_names:
          parser.error('"%s" referenced in --uses-split not present.' % name)
      split_map[child] = parent
  options.uses_split = split_map

  return options


class _DexPathContext(object):
  def __init__(self, name, output_path, input_jars, work_dir):
    self.name = name
    self.input_paths = input_jars
    self._final_output_path = output_path
    self.staging_dir = os.path.join(work_dir, name)
    os.mkdir(self.staging_dir)

  def CreateOutput(self, has_imported_lib=False, keep_rule_output=None):
    found_files = build_utils.FindInDirectory(self.staging_dir)
    if not found_files:
      raise Exception('Missing dex outputs in {}'.format(self.staging_dir))

    if self._final_output_path.endswith('.dex'):
      if has_imported_lib:
        raise Exception(
            'Trying to create a single .dex file, but a dependency requires '
            'JDK Library Desugaring (which necessitates a second file).'
            'Refer to %s to see what desugaring was required' %
            keep_rule_output)
      if len(found_files) != 1:
        raise Exception('Expected exactly 1 dex file output, found: {}'.format(
            '\t'.join(found_files)))
      shutil.move(found_files[0], self._final_output_path)
      return

    # Add to .jar using Python rather than having R8 output to a .zip directly
    # in order to disable compression of the .jar, saving ~500ms.
    tmp_jar_output = self.staging_dir + '.jar'
    build_utils.DoZip(found_files, tmp_jar_output, base_dir=self.staging_dir)
    shutil.move(tmp_jar_output, self._final_output_path)


def _OptimizeWithR8(options,
                    config_paths,
                    libraries,
                    dynamic_config_data,
                    print_stdout=False):
  with build_utils.TempDir() as tmp_dir:
    if dynamic_config_data:
      tmp_config_path = os.path.join(tmp_dir, 'proguard_config.txt')
      with open(tmp_config_path, 'w') as f:
        f.write(dynamic_config_data)
      config_paths = config_paths + [tmp_config_path]

    tmp_mapping_path = os.path.join(tmp_dir, 'mapping.txt')
    # If there is no output (no classes are kept), this prevents this script
    # from failing.
    build_utils.Touch(tmp_mapping_path)

    tmp_output = os.path.join(tmp_dir, 'r8out')
    os.mkdir(tmp_output)

    feature_contexts = []
    if options.feature_names:
      for name, dest_dex, input_paths in zip(
          options.feature_names, options.dex_dests, options.feature_jars):
        feature_context = _DexPathContext(name, dest_dex, input_paths,
                                          tmp_output)
        if name == 'base':
          base_dex_context = feature_context
        else:
          feature_contexts.append(feature_context)
    else:
      base_dex_context = _DexPathContext('base', options.output_path,
                                         options.input_paths, tmp_output)

    cmd = build_utils.JavaCmd(options.warnings_as_errors) + [
        '-Dcom.android.tools.r8.allowTestProguardOptions=1',
        '-Dcom.android.tools.r8.verticalClassMerging=1',
    ]
    if options.disable_outlining:
      cmd += ['-Dcom.android.tools.r8.disableOutlining=1']
    cmd += [
        '-cp',
        options.r8_path,
        'com.android.tools.r8.R8',
        '--no-data-resources',
        '--output',
        base_dex_context.staging_dir,
        '--pg-map-output',
        tmp_mapping_path,
    ]

    if options.disable_checks:
      # Info level priority logs are not printed by default.
      cmd += ['--map-diagnostics:CheckDiscardDiagnostic', 'error', 'info']

    if options.desugar_jdk_libs_json:
      cmd += [
          '--desugared-lib',
          options.desugar_jdk_libs_json,
          '--desugared-lib-pg-conf-output',
          options.desugared_library_keep_rule_output,
      ]

    if options.min_api:
      cmd += ['--min-api', options.min_api]

    if options.force_enable_assertions:
      cmd += ['--force-enable-assertions']

    for lib in libraries:
      cmd += ['--lib', lib]

    for config_file in config_paths:
      cmd += ['--pg-conf', config_file]

    if options.main_dex_rules_path:
      for main_dex_rule in options.main_dex_rules_path:
        cmd += ['--main-dex-rules', main_dex_rule]

    base_jars = set(base_dex_context.input_paths)
    input_path_map = defaultdict(set)
    for feature in feature_contexts:
      parent = options.uses_split.get(feature.name, feature.name)
      input_path_map[parent].update(feature.input_paths)

    # If a jar is present in multiple features, it should be moved to the base
    # module.
    all_feature_jars = set()
    for input_paths in input_path_map.values():
      base_jars.update(all_feature_jars.intersection(input_paths))
      all_feature_jars.update(input_paths)

    module_input_jars = base_jars.copy()
    for feature in feature_contexts:
      input_paths = input_path_map.get(feature.name)
      # Input paths can be missing for a child feature present in the uses_split
      # map. These features get their input paths added to the parent, and are
      # split out later with DexSplitter.
      if input_paths is None:
        continue
      feature_input_jars = [
          p for p in input_paths if p not in module_input_jars
      ]
      module_input_jars.update(feature_input_jars)
      for in_jar in feature_input_jars:
        cmd += ['--feature', in_jar, feature.staging_dir]

    cmd += sorted(base_jars)
    # Add any extra input jars to the base module (e.g. desugar runtime).
    extra_jars = set(options.input_paths) - module_input_jars
    cmd += sorted(extra_jars)

    try:
      stderr_filter = dex.CreateStderrFilter(
          options.show_desugar_default_interface_warnings)
      logging.debug('Running R8')
      build_utils.CheckOutput(cmd,
                              print_stdout=print_stdout,
                              stderr_filter=stderr_filter,
                              fail_on_output=options.warnings_as_errors)
    except build_utils.CalledProcessError as err:
      debugging_link = ('\n\nR8 failed. Please see {}.'.format(
          'https://chromium.googlesource.com/chromium/src/+/HEAD/build/'
          'android/docs/java_optimization.md#Debugging-common-failures\n'))
      raise build_utils.CalledProcessError(err.cwd, err.args,
                                           err.output + debugging_link)

    if options.uses_split:
      _SplitChildFeatures(options, feature_contexts, tmp_dir, tmp_mapping_path,
                          print_stdout)

    base_has_imported_lib = False
    if options.desugar_jdk_libs_json:
      logging.debug('Running L8')
      existing_files = build_utils.FindInDirectory(base_dex_context.staging_dir)
      jdk_dex_output = os.path.join(base_dex_context.staging_dir,
                                    'classes%d.dex' % (len(existing_files) + 1))
      base_has_imported_lib = dex_jdk_libs.DexJdkLibJar(
          options.r8_path, options.min_api, options.desugar_jdk_libs_json,
          options.desugar_jdk_libs_jar,
          options.desugar_jdk_libs_configuration_jar,
          options.desugared_library_keep_rule_output, jdk_dex_output,
          options.warnings_as_errors)

    logging.debug('Collecting ouputs')
    base_dex_context.CreateOutput(base_has_imported_lib,
                                  options.desugared_library_keep_rule_output)
    for feature in feature_contexts:
      feature.CreateOutput()

    with open(options.mapping_output, 'w') as out_file, \
        open(tmp_mapping_path) as in_file:
      # Mapping files generated by R8 include comments that may break
      # some of our tooling so remove those (specifically: apkanalyzer).
      out_file.writelines(l for l in in_file if not l.startswith('#'))


def _CheckForMissingSymbols(r8_path, dex_files, classpath, warnings_as_errors):
  cmd = build_utils.JavaCmd(warnings_as_errors) + [
      '-cp', r8_path, 'com.android.tools.r8.tracereferences.TraceReferences',
      '--map-diagnostics:MissingDefinitionsDiagnostic', 'error', 'warning'
  ]

  for path in classpath:
    cmd += ['--lib', path]
  for path in dex_files:
    cmd += ['--source', path]

  def stderr_filter(stderr):
    ignored_lines = [
        # Summary contains warning count, which our filtering makes wrong.
        'Warning: Tracereferences found',

        # TODO(agrieve): Create interface jars for these missing classes rather
        #     than allowlisting here.
        'dalvik/system',
        'libcore/io',
        'sun/misc/Unsafe',

        # Found in: com/facebook/fbui/textlayoutbuilder/StaticLayoutHelper
        ('android/text/StaticLayout;<init>(Ljava/lang/CharSequence;IILandroid'
         '/text/TextPaint;ILandroid/text/Layout$Alignment;Landroid/text/'
         'TextDirectionHeuristic;FFZLandroid/text/TextUtils$TruncateAt;II)V'),

        # Found in
        # com/google/android/gms/cast/framework/media/internal/ResourceProvider
        # Missing due to setting "strip_resources = true".
        'com/google/android/gms/cast/framework/R',

        # Found in com/google/android/gms/common/GoogleApiAvailability
        # Missing due to setting "strip_drawables = true".
        'com/google/android/gms/base/R$drawable',

        # Explicictly guarded by try (NoClassDefFoundError) in Flogger's
        # PlatformProvider.
        'com/google/common/flogger/backend/google/GooglePlatform',
        'com/google/common/flogger/backend/system/DefaultPlatform',

        # trichrome_webview_google_bundle contains this missing reference.
        # TODO(crbug.com/1142530): Fix this missing reference properly.
        'org/chromium/base/library_loader/NativeLibraries',

        # Currently required when enable_chrome_android_internal=true.
        'com/google/protos/research/ink/InkEventProto',
        'ink_sdk/com/google/protobuf/Internal$EnumVerifier',
        'ink_sdk/com/google/protobuf/MessageLite',
        'com/google/protobuf/GeneratedMessageLite$GeneratedExtension',

        # Referenced from GeneratedExtensionRegistryLite.
        # Exists only for Chrome Modern (not Monochrome nor Trichrome).
        # TODO(agrieve): Figure out why. Perhaps related to Feed V2.
        ('com/google/wireless/android/play/playlog/proto/ClientAnalytics$'
         'ClientInfo'),

        # TODO(agrieve): Exclude these only when use_jacoco_coverage=true.
        'Ljava/lang/instrument/ClassFileTransformer',
        'Ljava/lang/instrument/IllegalClassFormatException',
        'Ljava/lang/instrument/Instrumentation',
        'Ljava/lang/management/ManagementFactory',
        'Ljavax/management/MBeanServer',
        'Ljavax/management/ObjectInstance',
        'Ljavax/management/ObjectName',
        'Ljavax/management/StandardMBean',
    ]

    had_unfiltered_items = '  ' in stderr
    stderr = build_utils.FilterLines(
        stderr, '|'.join(re.escape(x) for x in ignored_lines))
    if stderr:
      if '  ' in stderr:
        stderr = """
DEX contains references to non-existent symbols after R8 optimization.
Tip: Build with:
        is_java_debug=false
        treat_warnings_as_errors=false
        enable_proguard_obfuscation=false
     and then use dexdump to see which class(s) reference them.

     E.g.:
       third_party/android_sdk/public/build-tools/*/dexdump -d \
out/Release/apks/YourApk.apk > dex.txt
""" + stderr
      elif had_unfiltered_items:
        # Left only with empty headings. All indented items filtered out.
        stderr = ''
    return stderr

  logging.debug('cmd: %s', ' '.join(cmd))
  build_utils.CheckOutput(cmd,
                          print_stdout=True,
                          stderr_filter=stderr_filter,
                          fail_on_output=warnings_as_errors)


def _SplitChildFeatures(options, feature_contexts, tmp_dir, mapping_path,
                        print_stdout):
  feature_map = {f.name: f for f in feature_contexts}
  parent_to_child = defaultdict(list)
  for child, parent in options.uses_split.items():
    parent_to_child[parent].append(child)
  for parent, children in parent_to_child.items():
    split_output = os.path.join(tmp_dir, 'split_%s' % parent)
    os.mkdir(split_output)
    # DexSplitter is not perfect and can cause issues related to inlining and
    # class merging (see crbug.com/1032609). If strange class loading errors
    # happen in DFMs specifying uses_split, this may be the cause.
    split_cmd = build_utils.JavaCmd(options.warnings_as_errors) + [
        '-cp',
        options.r8_path,
        'com.android.tools.r8.dexsplitter.DexSplitter',
        '--output',
        split_output,
        '--proguard-map',
        mapping_path,
    ]

    parent_jars = set(feature_map[parent].input_paths)
    for base_jar in sorted(parent_jars):
      split_cmd += ['--base-jar', base_jar]

    for child in children:
      for feature_jar in feature_map[child].input_paths:
        if feature_jar not in parent_jars:
          split_cmd += ['--feature-jar', '%s:%s' % (feature_jar, child)]

    # The inputs are the outputs for the parent from the original R8 call.
    parent_dir = feature_map[parent].staging_dir
    for file_name in os.listdir(parent_dir):
      split_cmd += ['--input', os.path.join(parent_dir, file_name)]
    logging.debug('Running R8 DexSplitter')
    build_utils.CheckOutput(split_cmd,
                            print_stdout=print_stdout,
                            fail_on_output=options.warnings_as_errors)

    # Copy the parent dex back into the parent's staging dir.
    base_split_output = os.path.join(split_output, 'base')
    shutil.rmtree(parent_dir)
    os.mkdir(parent_dir)
    for dex_file in os.listdir(base_split_output):
      shutil.move(os.path.join(base_split_output, dex_file),
                  os.path.join(parent_dir, dex_file))

    # Copy each child dex back into the child's staging dir.
    for child in children:
      child_split_output = os.path.join(split_output, child)
      child_staging_dir = feature_map[child].staging_dir
      shutil.rmtree(child_staging_dir)
      os.mkdir(child_staging_dir)
      for dex_file in os.listdir(child_split_output):
        shutil.move(os.path.join(child_split_output, dex_file),
                    os.path.join(child_staging_dir, dex_file))


def _CombineConfigs(configs, dynamic_config_data, exclude_generated=False):
  ret = []

  # Sort in this way so //clank versions of the same libraries will sort
  # to the same spot in the file.
  def sort_key(path):
    return tuple(reversed(path.split(os.path.sep)))

  for config in sorted(configs, key=sort_key):
    if exclude_generated and config.endswith('.resources.proguard.txt'):
      continue

    ret.append('# File: ' + config)
    with open(config) as config_file:
      contents = config_file.read().rstrip()

    # Fix up line endings (third_party configs can have windows endings).
    contents = contents.replace('\r', '')
    # Remove numbers from generated rule comments to make file more
    # diff'able.
    contents = re.sub(r' #generated:\d+', '', contents)
    ret.append(contents)
    ret.append('')

  if dynamic_config_data:
    ret.append('# File: //build/android/gyp/proguard.py (generated rules)')
    ret.append(dynamic_config_data)
    ret.append('')
  return '\n'.join(ret)


def _CreateDynamicConfig(options):
  ret = []
  if options.sourcefile:
    ret.append("-renamesourcefileattribute '%s' # OMIT FROM EXPECTATIONS" %
               options.sourcefile)

  if options.apply_mapping:
    ret.append("-applymapping '%s'" % os.path.abspath(options.apply_mapping))
  if options.repackage_classes:
    ret.append("-repackageclasses '%s'" % options.repackage_classes)

  _min_api = int(options.min_api) if options.min_api else 0
  for api_level, version_code in _API_LEVEL_VERSION_CODE:
    annotation_name = 'org.chromium.base.annotations.VerifiesOn' + version_code
    if api_level > _min_api:
      ret.append('-keep @interface %s' % annotation_name)
      ret.append("""\
-if @%s class * {
    *** *(...);
}
-keep,allowobfuscation class <1> {
    *** <2>(...);
}""" % annotation_name)
      ret.append("""\
-keepclassmembers,allowobfuscation class ** {
  @%s <methods>;
}""" % annotation_name)
  return '\n'.join(ret)


def _VerifyNoEmbeddedConfigs(jar_paths):
  failed = False
  for jar_path in jar_paths:
    with zipfile.ZipFile(jar_path) as z:
      for name in z.namelist():
        if name.startswith('META-INF/proguard/'):
          failed = True
          sys.stderr.write("""\
Found embedded proguard config within {}.
Embedded configs are not permitted (https://crbug.com/989505)
""".format(jar_path))
          break
  if failed:
    sys.exit(1)


def _ContainsDebuggingConfig(config_str):
  debugging_configs = ('-whyareyoukeeping', '-whyareyounotinlining')
  return any(config in config_str for config in debugging_configs)


def _MaybeWriteStampAndDepFile(options, inputs):
  output = options.output_path
  if options.stamp:
    build_utils.Touch(options.stamp)
    output = options.stamp
  if options.depfile:
    build_utils.WriteDepfile(options.depfile, output, inputs=inputs)


def main():
  build_utils.InitLogging('PROGUARD_DEBUG')
  options = _ParseOptions()

  logging.debug('Preparing configs')
  proguard_configs = options.proguard_configs

  # ProGuard configs that are derived from flags.
  dynamic_config_data = _CreateDynamicConfig(options)

  # ProGuard configs that are derived from flags.
  merged_configs = _CombineConfigs(
      proguard_configs, dynamic_config_data, exclude_generated=True)
  print_stdout = _ContainsDebuggingConfig(merged_configs) or options.verbose

  if options.expected_file:
    diff_utils.CheckExpectations(merged_configs, options)
    if options.only_verify_expectations:
      build_utils.WriteDepfile(options.depfile,
                               options.actual_file,
                               inputs=options.proguard_configs)
      return

  logging.debug('Looking for embedded configs')
  libraries = []
  for p in options.classpath:
    # TODO(bjoyce): Remove filter once old android support libraries are gone.
    # Fix for having Library class extend program class dependency problem.
    if 'com_android_support' in p or 'android_support_test' in p:
      continue
    # If a jar is part of input no need to include it as library jar.
    if p not in libraries and p not in options.input_paths:
      libraries.append(p)
  _VerifyNoEmbeddedConfigs(options.input_paths + libraries)

  _OptimizeWithR8(options, proguard_configs, libraries, dynamic_config_data,
                  print_stdout)

  if not options.disable_checks:
    logging.debug('Running tracereferences')
    all_dex_files = []
    if options.output_path:
      all_dex_files.append(options.output_path)
    if options.dex_dests:
      all_dex_files.extend(options.dex_dests)
    _CheckForMissingSymbols(options.r8_path, all_dex_files, options.classpath,
                            options.warnings_as_errors)

  for output in options.extra_mapping_output_paths:
    shutil.copy(options.mapping_output, output)

  inputs = options.proguard_configs + options.input_paths + libraries
  if options.apply_mapping:
    inputs.append(options.apply_mapping)

  _MaybeWriteStampAndDepFile(options, inputs)


if __name__ == '__main__':
  main()
