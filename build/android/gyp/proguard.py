#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
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
_CHECKDISCARD_RE = re.compile(r'^\s*-checkdiscard[\s\S]*?}', re.MULTILINE)
_DIRECTIVE_RE = re.compile(r'^\s*-', re.MULTILINE)


def _ValidateAndFilterCheckDiscards(configs):
  """Check for invalid -checkdiscard rules and filter out -checkdiscards.

  -checkdiscard assertions often don't work for test APKs and are not actually
  helpful. Additionally, test APKs may pull in dependency proguard configs which
  makes filtering out these rules difficult in GN. Instead, we enforce that
  configs that use -checkdiscard do not contain any other rules so that we can
  filter out the undesired -checkdiscard rule files here.

  Args:
    configs: List of paths to proguard configuration files.

  Returns:
    A list of configs with -checkdiscard-containing-configs removed.
  """
  valid_configs = []
  for config_path in configs:
    with open(config_path) as f:
      contents = f.read()
      if _CHECKDISCARD_RE.search(contents):
        contents = _CHECKDISCARD_RE.sub('', contents)
        if _DIRECTIVE_RE.search(contents):
          raise Exception('Proguard configs containing -checkdiscards cannot '
                          'contain other directives so that they can be '
                          'disabled in test APKs ({}).'.format(config_path))
      else:
        valid_configs.append(config_path)

  return valid_configs


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
      '--disable-checkdiscard',
      action='store_true',
      help='Disable -checkdiscard directives')
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

    module_input_jars = set(base_dex_context.input_paths)
    for feature in feature_contexts:
      feature_input_jars = [
          p for p in feature.input_paths if p not in module_input_jars
      ]
      module_input_jars.update(feature_input_jars)
      for in_jar in feature_input_jars:
        cmd += ['--feature', in_jar, feature.staging_dir]

    cmd += base_dex_context.input_paths
    # Add any extra input jars to the base module (e.g. desugar runtime).
    extra_jars = set(options.input_paths) - module_input_jars
    cmd += sorted(extra_jars)

    try:
      stderr_filter = dex.CreateStderrFilter(
          options.show_desugar_default_interface_warnings)
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

    base_has_imported_lib = False
    if options.desugar_jdk_libs_json:
      existing_files = build_utils.FindInDirectory(base_dex_context.staging_dir)
      jdk_dex_output = os.path.join(base_dex_context.staging_dir,
                                    'classes%d.dex' % (len(existing_files) + 1))
      base_has_imported_lib = dex_jdk_libs.DexJdkLibJar(
          options.r8_path, options.min_api, options.desugar_jdk_libs_json,
          options.desugar_jdk_libs_jar,
          options.desugar_jdk_libs_configuration_jar,
          options.desugared_library_keep_rule_output, jdk_dex_output,
          options.warnings_as_errors)

    base_dex_context.CreateOutput(base_has_imported_lib,
                                  options.desugared_library_keep_rule_output)
    for feature in feature_contexts:
      feature.CreateOutput()

    with open(options.mapping_output, 'w') as out_file, \
        open(tmp_mapping_path) as in_file:
      # Mapping files generated by R8 include comments that may break
      # some of our tooling so remove those (specifically: apkanalyzer).
      out_file.writelines(l for l in in_file if not l.startswith('#'))


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
  options = _ParseOptions()

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

  proguard_configs = options.proguard_configs
  if options.disable_checkdiscard:
    proguard_configs = _ValidateAndFilterCheckDiscards(proguard_configs)

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

  _OptimizeWithR8(options, proguard_configs, libraries, dynamic_config_data,
                  print_stdout)

  # After ProGuard / R8 has run:
  for output in options.extra_mapping_output_paths:
    shutil.copy(options.mapping_output, output)

  inputs = options.proguard_configs + options.input_paths + libraries
  if options.apply_mapping:
    inputs.append(options.apply_mapping)

  _MaybeWriteStampAndDepFile(options, inputs)


if __name__ == '__main__':
  main()
