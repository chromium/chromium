#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import os
import shutil
import sys
import tempfile

from util import build_utils
from util import proguard_util


_DANGEROUS_OPTIMIZATIONS = [
    # See crbug.com/825995 (can cause VerifyErrors)
    "class/merging/vertical",
    "class/unboxing/enum",
    # See crbug.com/625992
    "code/allocation/variable",
    # See crbug.com/625994
    "field/propagation/value",
    "method/propagation/parameter",
    "method/propagation/returnvalue",
]


# Example:
# android.arch.core.internal.SafeIterableMap$Entry -> b:
#     1:1:java.lang.Object getKey():353:353 -> getKey
#     2:2:java.lang.Object getValue():359:359 -> getValue
def _RemoveMethodMappings(orig_path, out_fd):
  with open(orig_path) as in_fd:
    for line in in_fd:
      if line[:1] != ' ':
        out_fd.write(line)
  out_fd.flush()


def _ParseOptions(args):
  parser = optparse.OptionParser()
  build_utils.AddDepfileOption(parser)
  parser.add_option('--proguard-path',
                    help='Path to the proguard.jar to use.')
  parser.add_option('--r8-path',
                    help='Path to the R8.jar to use.')
  parser.add_option('--input-paths',
                    help='Paths to the .jar files proguard should run on.')
  parser.add_option('--output-path', help='Path to the generated .jar file.')
  parser.add_option('--proguard-configs', action='append',
                    help='Paths to proguard configuration files.')
  parser.add_option('--proguard-config-exclusions',
                    default='',
                    help='GN list of paths to proguard configuration files '
                         'included by --proguard-configs, but that should '
                         'not actually be included.')
  parser.add_option('--mapping', help='Path to proguard mapping to apply.')
  parser.add_option('--mapping-output',
                    help='Path for proguard to output mapping file to.')
  parser.add_option('--classpath', action='append',
                    help='Classpath for proguard.')
  parser.add_option('--enable-dangerous-optimizations', action='store_true',
                    help='Enable optimizations which are known to have issues.')
  parser.add_option('--main-dex-rules-path', action='append',
                    help='Paths to main dex rules for multidex'
                         '- only works with R8.')
  parser.add_option('--min-api', default='',
                    help='Minimum Android API level compatibility.')
  parser.add_option('--verbose', '-v', action='store_true',
                    help='Print all proguard output')

  options, _ = parser.parse_args(args)

  assert not options.main_dex_rules_path or options.r8_path, \
      "R8 must be enabled to pass main dex rules."
  assert not options.min_api or options.r8_path, \
      "R8 must be enabled to pass min api."

  classpath = []
  for arg in options.classpath:
    classpath += build_utils.ParseGnList(arg)
  options.classpath = classpath

  configs = []
  for arg in options.proguard_configs:
    configs += build_utils.ParseGnList(arg)
  options.proguard_configs = configs
  options.proguard_config_exclusions = (
      build_utils.ParseGnList(options.proguard_config_exclusions))

  options.input_paths = build_utils.ParseGnList(options.input_paths)

  if not options.mapping_output:
    options.mapping_output = options.output_path + ".mapping"

  return options


def _MoveTempDexFile(tmp_dex_dir, dex_path):
  """Move the temp dex file out of |tmp_dex_dir|.

  Args:
    tmp_dex_dir: Path to temporary directory created with tempfile.mkdtemp().
      The directory should have just a single file.
    dex_path: Target path to move dex file to.

  Raises:
    Exception if there are multiple files in |tmp_dex_dir|.
  """
  tempfiles = os.listdir(tmp_dex_dir)
  if len(tempfiles) > 1:
    raise Exception('%d files created, expected 1' % len(tempfiles))

  tmp_dex_path = os.path.join(tmp_dex_dir, tempfiles[0])
  shutil.move(tmp_dex_path, dex_path)


def _CreateR8Command(options, map_output_path, output_dir):
  # TODO: R8 needs -applymapping equivalent.
  cmd = [
    'java', '-jar', options.r8_path,
    '--no-desugaring',
    '--no-data-resources',
    '--output', output_dir,
    '--pg-map-output', map_output_path,
  ]

  classpath = [
      p for p in set(options.classpath) if p not in options.input_paths
  ]
  for lib in classpath:
    cmd += ['--lib', lib]

  for config_file in options.proguard_configs:
    cmd += ['--pg-conf', config_file]

  if options.min_api:
    cmd += ['--min-api', options.min_api]

  if options.main_dex_rules_path:
    for main_dex_rule in options.main_dex_rules_path:
      cmd += ['--main-dex-rules', main_dex_rule]

  cmd += options.input_paths
  return cmd


def main(args):
  args = build_utils.ExpandFileArgs(args)
  options = _ParseOptions(args)

  proguard = proguard_util.ProguardCmdBuilder(options.proguard_path)
  proguard.injars(options.input_paths)
  proguard.configs(options.proguard_configs)
  proguard.config_exclusions(options.proguard_config_exclusions)
  proguard.outjar(options.output_path)
  proguard.mapping_output(options.mapping_output)

  # If a jar is part of input no need to include it as library jar.
  classpath = [
      p for p in set(options.classpath) if p not in options.input_paths
  ]
  proguard.libraryjars(classpath)
  proguard.verbose(options.verbose)
  if not options.enable_dangerous_optimizations:
    proguard.disable_optimizations(_DANGEROUS_OPTIMIZATIONS)

  # TODO(agrieve): Remove proguard usages.
  if options.r8_path:
    with tempfile.NamedTemporaryFile() as mapping_temp:
      if options.output_path.endswith('.dex'):
        with build_utils.TempDir() as tmp_dex_dir:
          cmd = _CreateR8Command(options, mapping_temp.name, tmp_dex_dir)
          build_utils.CheckOutput(cmd)
          _MoveTempDexFile(tmp_dex_dir, options.output_path)
      else:
        cmd = _CreateR8Command(options, mapping_temp.name, options.output_path)
        build_utils.CheckOutput(cmd)

      # Copy the mapping file back to where it should be.
      map_path = options.mapping_output
      with build_utils.AtomicOutput(map_path) as mapping:
        # Mapping files generated by R8 include comments that may break
        # some of our tooling so remove those.
        mapping_temp.seek(0)
        mapping.writelines(l for l in mapping_temp if not l.startswith("#"))

    build_utils.WriteDepfile(options.depfile, options.output_path,
                             inputs=proguard.GetDepfileDeps(),
                             add_pydeps=False)
  else:
    # Do not consider the temp file as an input since its name is random.
    input_paths = proguard.GetInputs()

    with tempfile.NamedTemporaryFile() as f:
      if options.mapping:
        input_paths.append(options.mapping)
        # Maintain only class name mappings in the .mapping file in order to
        # work around what appears to be a ProGuard bug in -applymapping:
        #     method 'int close()' is not being kept as 'a', but remapped to 'c'
        _RemoveMethodMappings(options.mapping, f)
        proguard.mapping(f.name)

      input_strings = proguard.build()
      if f.name in input_strings:
        input_strings[input_strings.index(f.name)] = '$M'

      build_utils.CallAndWriteDepfileIfStale(
          proguard.CheckOutput,
          options,
          input_paths=input_paths,
          input_strings=input_strings,
          output_paths=proguard.GetOutputs(),
          depfile_deps=proguard.GetDepfileDeps(),
          add_pydeps=False)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
