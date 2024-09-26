#!/usr/bin/env python3
#
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import logging
import os
import pathlib
import re
import shutil
import sys
import zipfile

import dex
from util import build_utils
from util import diff_utils
import action_helpers  # build_utils adds //build to sys.path.
import zip_helpers

_IGNORE_WARNINGS = (
    # E.g. Triggers for weblayer_instrumentation_test_apk since both it and its
    # apk_under_test have no shared_libraries.
    # https://crbug.com/1364192 << To fix this in a better way.
    r'Missing class org.chromium.build.NativeLibraries',
    # Caused by protobuf runtime using -identifiernamestring in a way that
    # doesn't work with R8. Looks like:
    # Rule matches the static final field `...`, which may have been inlined...
    # com.google.protobuf.*GeneratedExtensionRegistryLite {
    #   static java.lang.String CONTAINING_TYPE_*;
    # }
    r'GeneratedExtensionRegistryLite\.CONTAINING_TYPE_',
    # Relevant for R8 when optimizing an app that doesn't use protobuf.
    r'Ignoring -shrinkunusedprotofields since the protobuf-lite runtime is',
    # Ignore Unused Rule Warnings in third_party libraries.
    r'/third_party/.*Proguard configuration rule does not match anything',
    # Ignore cronet's test rules (low priority to fix).
    r'cronet/android/test/proguard.cfg.*Proguard configuration rule does not',
    r'Proguard configuration rule does not match anything:.*(?:' + '|'.join([
        # aapt2 generates keeps for these.
        r'class android\.',
        # Used internally.
        r'com.no.real.class.needed.receiver',
        # Ignore Unused Rule Warnings for annotations.
        r'@',
        # Ignore Unused Rule Warnings for * implements Foo (androidx has these).
        r'class \*+ implements',
        # Ignore rules that opt out of this check.
        r'!cr_allowunused',
        # https://crbug.com/1441225
        r'EditorDialogToolbar',
        # https://crbug.com/1441226
        r'PaymentRequest[BH]',
    ]) + ')',
    # TODO(agrieve): Remove once we update to U SDK.
    r'OnBackAnimationCallback',
    # This class was added only in the U PrivacySandbox SDK: crbug.com/333713111
    r'Missing class android.adservices.common.AdServicesOutcomeReceiver',
    # We enforce that this class is removed via -checkdiscard.
    r'FastServiceLoader\.class:.*Could not inline ServiceLoader\.load',
    # Happens on internal builds. It's a real failure, but happens in dead code.
    r'(?:GeneratedExtensionRegistryLoader|ExtensionRegistryLite)\.class:.*Could not inline ServiceLoader\.load',   # pylint: disable=line-too-long
    # This class is referenced by kotlinx-coroutines-core-jvm but it does not
    # depend on it. Not actually needed though.
    r'Missing class org.codehaus.mojo.animal_sniffer.IgnoreJRERequirement',
    # Ignore MethodParameter attribute count isn't matching in espresso.
    # This is a banner warning and each individual file affected will have
    # its own warning.
    r'Warning: Invalid parameter counts in MethodParameter attributes',
    # Full error: "Warning: InnerClasses attribute has entries missing a
    # corresponding EnclosingMethod attribute. Such InnerClasses attribute
    # entries are ignored."
    r'Warning: InnerClasses attribute has entries missing a corresponding EnclosingMethod attribute',  # pylint: disable=line-too-long
    # Full error example: "Warning in <path to target prebuilt>:
    # androidx/test/espresso/web/internal/deps/guava/collect/Maps$1.class:"
    # Also happens in espresso core.
    r'Warning in .*:androidx/test/espresso/.*/guava/collect/.*',

    # We are following up in b/290389974
    r'AppSearchDocumentClassMap\.class:.*Could not inline ServiceLoader\.load',
)

_BLOCKLISTED_EXPECTATION_PATHS = [
    # A separate expectation file is created for these files.
    # TODO(369195356): Remove this after cipd migration.
    'clank/third_party/google3/pg_confs/',
    'clank/third_party/google3/cipd/pg_confs/',
]

_DUMP_DIR_NAME = 'r8inputs_dir'


def _ParseOptions():
  args = build_utils.ExpandFileArgs(sys.argv[1:])
  parser = argparse.ArgumentParser()
  action_helpers.add_depfile_arg(parser)
  parser.add_argument('--r8-path',
                      required=True,
                      help='Path to the R8.jar to use.')
  parser.add_argument('--custom-r8-path',
                      required=True,
                      help='Path to our custom R8 wrapepr to use.')
  parser.add_argument('--input-paths',
                      action='append',
                      required=True,
                      help='GN-list of .jar files to optimize.')
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
  parser.add_argument('--sdk-jars',
                      action='append',
                      help='GN-list of .jar files to include as libraries.')
  parser.add_argument(
      '--sdk-extension-jars',
      action='append',
      help='GN-list of .jar files to include as libraries, and that are not a '
      'part of R8\'s API database.')
  parser.add_argument('--main-dex-rules-path',
                      action='append',
                      help='Path to main dex rules for multidex.')
  parser.add_argument(
      '--min-api', help='Minimum Android API level compatibility.')
  parser.add_argument('--enable-obfuscation',
                      action='store_true',
                      help='Minify symbol names')
  parser.add_argument(
      '--verbose', '-v', action='store_true', help='Print all ProGuard output')
  parser.add_argument('--repackage-classes',
                      default='',
                      help='Value for -repackageclasses.')
  parser.add_argument(
    '--disable-checks',
    action='store_true',
    help='Disable -checkdiscard directives and missing symbols check')
  parser.add_argument('--source-file', help='Value for source file attribute.')
  parser.add_argument('--package-name',
                      help='Goes into a comment in the mapping file.')
  parser.add_argument(
      '--force-enable-assertions',
      action='store_true',
      help='Forcefully enable javac generated assertion code.')
  parser.add_argument('--assertion-handler',
                      help='The class name of the assertion handler class.')
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
  parser.add_argument('--input-art-profile',
                      help='Path to the input unobfuscated ART profile.')
  parser.add_argument('--output-art-profile',
                      help='Path to the output obfuscated ART profile.')
  parser.add_argument(
      '--apply-startup-profile',
      action='store_true',
      help='Whether to pass --input-art-profile as a startup profile to R8.')
  parser.add_argument(
      '--keep-rules-targets-regex',
      metavar='KEEP_RULES_REGEX',
      help='If passed outputs keep rules for references from all other inputs '
      'to the subset of inputs that satisfy the KEEP_RULES_REGEX.')
  parser.add_argument(
      '--keep-rules-output-path',
      help='Output path to the keep rules for references to the '
      '--keep-rules-targets-regex inputs from the rest of the inputs.')
  parser.add_argument('--warnings-as-errors',
                      action='store_true',
                      help='Treat all warnings as errors.')
  parser.add_argument('--show-desugar-default-interface-warnings',
                      action='store_true',
                      help='Enable desugaring warnings.')
  parser.add_argument('--dump-inputs',
                      action='store_true',
                      help='Use when filing R8 bugs to capture inputs.'
                      ' Stores inputs to r8inputs.zip')
  parser.add_argument(
      '--dump-unknown-refs',
      action='store_true',
      help='Log all reasons why API modelling cannot determine API level')
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

  if bool(options.keep_rules_targets_regex) != bool(
      options.keep_rules_output_path):
    parser.error('You must path both --keep-rules-targets-regex and '
                 '--keep-rules-output-path')

  if options.output_art_profile and not options.input_art_profile:
    parser.error('--output-art-profile requires --input-art-profile')
  if options.apply_startup_profile and not options.input_art_profile:
    parser.error('--apply-startup-profile requires --input-art-profile')

  if options.force_enable_assertions and options.assertion_handler:
    parser.error('Cannot use both --force-enable-assertions and '
                 '--assertion-handler')

  options.sdk_jars = action_helpers.parse_gn_list(options.sdk_jars)
  options.sdk_extension_jars = action_helpers.parse_gn_list(
      options.sdk_extension_jars)
  options.proguard_configs = action_helpers.parse_gn_list(
      options.proguard_configs)
  options.input_paths = action_helpers.parse_gn_list(options.input_paths)
  options.extra_mapping_output_paths = action_helpers.parse_gn_list(
      options.extra_mapping_output_paths)
  if os.environ.get('R8_VERBOSE') == '1':
    options.verbose = True

  if options.feature_names:
    if 'base' not in options.feature_names:
      parser.error('"base" feature required when feature arguments are used.')
    if len(options.feature_names) != len(options.feature_jars) or len(
        options.feature_names) != len(options.dex_dests):
      parser.error('Invalid feature argument lengths.')

    options.feature_jars = [
        action_helpers.parse_gn_list(x) for x in options.feature_jars
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


class _SplitContext:
  def __init__(self, name, output_path, input_jars, work_dir, parent_name=None):
    self.name = name
    self.parent_name = parent_name
    self.input_jars = set(input_jars)
    self.final_output_path = output_path
    self.staging_dir = os.path.join(work_dir, name)
    os.mkdir(self.staging_dir)

  def CreateOutput(self):
    found_files = build_utils.FindInDirectory(self.staging_dir)
    if not found_files:
      raise Exception('Missing dex outputs in {}'.format(self.staging_dir))

    if self.final_output_path.endswith('.dex'):
      if len(found_files) != 1:
        raise Exception('Expected exactly 1 dex file output, found: {}'.format(
            '\t'.join(found_files)))
      shutil.move(found_files[0], self.final_output_path)
      return

    # Add to .jar using Python rather than having R8 output to a .zip directly
    # in order to disable compression of the .jar, saving ~500ms.
    tmp_jar_output = self.staging_dir + '.jar'
    zip_helpers.add_files_to_zip(found_files,
                                 tmp_jar_output,
                                 base_dir=self.staging_dir)
    shutil.move(tmp_jar_output, self.final_output_path)


def _OptimizeWithR8(options, config_paths, libraries, dynamic_config_data):
  with build_utils.TempDir() as tmp_dir:
    if dynamic_config_data:
      dynamic_config_path = os.path.join(tmp_dir, 'dynamic_config.flags')
      with open(dynamic_config_path, 'w') as f:
        f.write(dynamic_config_data)
      config_paths = config_paths + [dynamic_config_path]

    tmp_mapping_path = os.path.join(tmp_dir, 'mapping.txt')
    # If there is no output (no classes are kept), this prevents this script
    # from failing.
    build_utils.Touch(tmp_mapping_path)

    tmp_output = os.path.join(tmp_dir, 'r8out')
    os.mkdir(tmp_output)

    split_contexts_by_name = {}
    if options.feature_names:
      for name, dest_dex, input_jars in zip(options.feature_names,
                                            options.dex_dests,
                                            options.feature_jars):
        parent_name = options.uses_split.get(name)
        if parent_name is None and name != 'base':
          parent_name = 'base'
        split_context = _SplitContext(name,
                                      dest_dex,
                                      input_jars,
                                      tmp_output,
                                      parent_name=parent_name)
        split_contexts_by_name[name] = split_context
    else:
      # Base context will get populated via "extra_jars" below.
      split_contexts_by_name['base'] = _SplitContext('base',
                                                     options.output_path, [],
                                                     tmp_output)
    base_context = split_contexts_by_name['base']

    # R8 OOMs with xmx=2G.
    cmd = build_utils.JavaCmd(xmx='3G') + [
        # Allows -whyareyounotinlining, which we don't have by default, but
        # which is useful for one-off queries.
        '-Dcom.android.tools.r8.experimental.enablewhyareyounotinlining=1',
        # Restricts horizontal class merging to apply only to classes that
        # share a .java file (nested classes). https://crbug.com/1363709
        '-Dcom.android.tools.r8.enableSameFilePolicy=1',
        # Allow ServiceLoaderUtil.maybeCreate() to work with types that are
        # -kept (e.g. due to containing JNI).
        '-Dcom.android.tools.r8.allowServiceLoaderRewritingPinnedTypes=1',
        # Allow R8 to inline kept methods by default.
        # See: b/364267880#2
        '-Dcom.android.tools.r8.allowCodeReplacement=false',
        # Required to use "-keep,allowcodereplacement"
        '-Dcom.android.tools.r8.allowTestProguardOptions=true',
    ]
    if options.sdk_extension_jars:
      # Enable API modelling for OS extensions. https://b/326252366
      cmd += [
          '-Dcom.android.tools.r8.androidApiExtensionLibraries=' +
          ','.join(options.sdk_extension_jars)
      ]
    if options.dump_inputs:
      cmd += [f'-Dcom.android.tools.r8.dumpinputtodirectory={_DUMP_DIR_NAME}']
    if options.dump_unknown_refs:
      cmd += ['-Dcom.android.tools.r8.reportUnknownApiReferences=1']
    cmd += [
        '-cp',
        '{}:{}'.format(options.r8_path, options.custom_r8_path),
        'org.chromium.build.CustomR8',
        '--no-data-resources',
        '--map-id-template',
        f'{options.source_file} ({options.package_name})',
        '--source-file-template',
        options.source_file,
        '--output',
        base_context.staging_dir,
        '--pg-map-output',
        tmp_mapping_path,
    ]

    if options.uses_split:
      cmd += ['--isolated-splits']

    if options.disable_checks:
      cmd += ['--map-diagnostics:CheckDiscardDiagnostic', 'error', 'none']
    # Triggered by rules from deps we cannot control.
    cmd += [('--map-diagnostics:EmptyMemberRulesToDefaultInitRuleConversion'
             'Diagnostic'), 'warning', 'none']
    cmd += ['--map-diagnostics', 'info', 'warning']
    # An "error" level diagnostic causes r8 to return an error exit code. Doing
    # this allows our filter to decide what should/shouldn't break our build.
    cmd += ['--map-diagnostics', 'error', 'warning']

    if options.min_api:
      cmd += ['--min-api', options.min_api]

    if options.assertion_handler:
      cmd += ['--force-assertions-handler:' + options.assertion_handler]
    elif options.force_enable_assertions:
      cmd += ['--force-enable-assertions']

    for lib in libraries:
      cmd += ['--lib', lib]

    for config_file in config_paths:
      cmd += ['--pg-conf', config_file]

    if options.main_dex_rules_path:
      for main_dex_rule in options.main_dex_rules_path:
        cmd += ['--main-dex-rules', main_dex_rule]

    if options.output_art_profile:
      cmd += [
          '--art-profile',
          options.input_art_profile,
          options.output_art_profile,
      ]
    if options.apply_startup_profile:
      cmd += [
          '--startup-profile',
          options.input_art_profile,
      ]

    # Add any extra inputs to the base context (e.g. desugar runtime).
    extra_jars = set(options.input_paths)
    for split_context in split_contexts_by_name.values():
      extra_jars -= split_context.input_jars
    base_context.input_jars.update(extra_jars)

    for split_context in split_contexts_by_name.values():
      if split_context is base_context:
        continue
      for in_jar in sorted(split_context.input_jars):
        cmd += ['--feature', in_jar, split_context.staging_dir]

    cmd += sorted(base_context.input_jars)

    if options.verbose:
      stderr_filter = None
    else:
      filters = list(dex.DEFAULT_IGNORE_WARNINGS)
      filters += _IGNORE_WARNINGS
      if options.show_desugar_default_interface_warnings:
        filters += dex.INTERFACE_DESUGARING_WARNINGS
      stderr_filter = dex.CreateStderrFilter(filters)

    try:
      logging.debug('Running R8')
      build_utils.CheckOutput(cmd,
                              print_stdout=True,
                              stderr_filter=stderr_filter,
                              fail_on_output=options.warnings_as_errors)
    except build_utils.CalledProcessError as e:
      # Do not output command line because it is massive and makes the actual
      # error message hard to find.
      sys.stderr.write(e.output)
      sys.exit(1)

    logging.debug('Collecting ouputs')
    base_context.CreateOutput()
    for split_context in split_contexts_by_name.values():
      if split_context is not base_context:
        split_context.CreateOutput()

    shutil.move(tmp_mapping_path, options.mapping_output)
  return split_contexts_by_name


def _OutputKeepRules(r8_path, input_paths, libraries, targets_re_string,
                     keep_rules_output):

  cmd = build_utils.JavaCmd(xmx='2G') + [
      '-cp', r8_path, 'com.android.tools.r8.tracereferences.TraceReferences',
      '--map-diagnostics:MissingDefinitionsDiagnostic', 'error', 'warning',
      '--keep-rules', '--output', keep_rules_output
  ]
  targets_re = re.compile(targets_re_string)
  for path in input_paths:
    if targets_re.search(path):
      cmd += ['--target', path]
    else:
      cmd += ['--source', path]
  for path in libraries:
    cmd += ['--lib', path]

  build_utils.CheckOutput(cmd, print_stderr=False, fail_on_output=False)


def _CheckForMissingSymbols(options, dex_files, error_title):
  cmd = build_utils.JavaCmd(xmx='2G')

  if options.dump_inputs:
    cmd += [f'-Dcom.android.tools.r8.dumpinputtodirectory={_DUMP_DIR_NAME}']

  cmd += [
      '-cp', options.r8_path,
      'com.android.tools.r8.tracereferences.TraceReferences',
      '--map-diagnostics:MissingDefinitionsDiagnostic', 'error', 'warning',
      '--check'
  ]

  for path in options.sdk_jars + options.sdk_extension_jars:
    cmd += ['--lib', path]
  for path in dex_files:
    cmd += ['--source', path]

  failed_holder = [False]

  def stderr_filter(stderr):
    ignored_lines = [
        # Summary contains warning count, which our filtering makes wrong.
        'Warning: Tracereferences found',

        # TODO(agrieve): Create interface jars for these missing classes rather
        #     than allowlisting here.
        'dalvik.system',
        'libcore.io',
        'sun.misc.Unsafe',

        # Found in: com/facebook/fbui/textlayoutbuilder/StaticLayoutHelper
        'android.text.StaticLayout.<init>',
        # TODO(crbug.com/40261573): Remove once chrome builds with Android U
        # SDK.
        ' android.',

        # Explicictly guarded by try (NoClassDefFoundError) in Flogger's
        # PlatformProvider.
        'com.google.common.flogger.backend.google.GooglePlatform',
        'com.google.common.flogger.backend.system.DefaultPlatform',

        # TODO(agrieve): Exclude these only when use_jacoco_coverage=true.
        'java.lang.instrument.ClassFileTransformer',
        'java.lang.instrument.IllegalClassFormatException',
        'java.lang.instrument.Instrumentation',
        'java.lang.management.ManagementFactory',
        'javax.management.MBeanServer',
        'javax.management.ObjectInstance',
        'javax.management.ObjectName',
        'javax.management.StandardMBean',

        # Explicitly guarded by try (NoClassDefFoundError) in Firebase's
        # KotlinDetector: com.google.firebase.platforminfo.KotlinDetector.
        'kotlin.KotlinVersion',
    ]

    had_unfiltered_items = '  ' in stderr
    stderr = build_utils.FilterLines(
        stderr, '|'.join(re.escape(x) for x in ignored_lines))
    if stderr:
      if 'Missing' in stderr:
        failed_holder[0] = True
        stderr = 'TraceReferences failed: ' + error_title + """
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

  try:
    if options.verbose:
      stderr_filter = None
    build_utils.CheckOutput(cmd,
                            print_stdout=True,
                            stderr_filter=stderr_filter,
                            fail_on_output=options.warnings_as_errors)
  except build_utils.CalledProcessError as e:
    # Do not output command line because it is massive and makes the actual
    # error message hard to find.
    sys.stderr.write(e.output)
    sys.exit(1)
  return failed_holder[0]


def _CombineConfigs(configs,
                    dynamic_config_data,
                    embedded_configs,
                    exclude_generated=False):
  # Sort in this way so //clank versions of the same libraries will sort
  # to the same spot in the file.
  def sort_key(path):
    return tuple(reversed(path.split(os.path.sep)))

  def format_config_contents(path, contents):
    formatted_contents = []
    if not contents.strip():
      return []

    # Fix up line endings (third_party configs can have windows endings).
    contents = contents.replace('\r', '')
    # Remove numbers from generated rule comments to make file more
    # diff'able.
    contents = re.sub(r' #generated:\d+', '', contents)
    formatted_contents.append('# File: ' + path)
    formatted_contents.append(contents)
    formatted_contents.append('')
    return formatted_contents

  ret = []
  for config in sorted(configs, key=sort_key):
    if exclude_generated and config.endswith('.resources.proguard.txt'):
      continue

    # Exclude some confs from expectations.
    if any(entry in config for entry in _BLOCKLISTED_EXPECTATION_PATHS):
      continue

    with open(config) as config_file:
      contents = config_file.read().rstrip()

    ret.extend(format_config_contents(config, contents))

  for path, contents in sorted(embedded_configs.items()):
    ret.extend(format_config_contents(path, contents))


  if dynamic_config_data:
    ret.append('# File: //build/android/gyp/proguard.py (generated rules)')
    ret.append(dynamic_config_data)
    ret.append('')
  return '\n'.join(ret)


def _CreateDynamicConfig(options):
  ret = []
  if options.enable_obfuscation:
    ret.append(f"-repackageclasses '{options.repackage_classes}'")
  else:
    ret.append("-dontobfuscate")

  if options.apply_mapping:
    ret.append("-applymapping '%s'" % options.apply_mapping)

  return '\n'.join(ret)


def _ExtractEmbeddedConfigs(jar_path, embedded_configs):
  with zipfile.ZipFile(jar_path) as z:
    proguard_names = []
    r8_names = []
    for info in z.infolist():
      if info.is_dir():
        continue
      if info.filename.startswith('META-INF/proguard/'):
        proguard_names.append(info.filename)
      elif info.filename.startswith('META-INF/com.android.tools/r8/'):
        r8_names.append(info.filename)
      elif info.filename.startswith('META-INF/com.android.tools/r8-from'):
        # Assume our version of R8 is always latest.
        if '-upto-' not in info.filename:
          r8_names.append(info.filename)

    # Give preference to r8-from-*, then r8/, then proguard/.
    active = r8_names or proguard_names
    for filename in active:
      config_path = '{}:{}'.format(jar_path, filename)
      embedded_configs[config_path] = z.read(filename).decode('utf-8').rstrip()


def _MaybeWriteStampAndDepFile(options, inputs):
  output = options.output_path
  if options.stamp:
    build_utils.Touch(options.stamp)
    output = options.stamp
  if options.depfile:
    action_helpers.write_depfile(options.depfile, output, inputs=inputs)


def _IterParentContexts(context_name, split_contexts_by_name):
  while context_name:
    context = split_contexts_by_name[context_name]
    yield context
    context_name = context.parent_name


def _DoTraceReferencesChecks(options, split_contexts_by_name):
  # Set of all contexts that are a parent to another.
  parent_splits_context_names = {
      c.parent_name
      for c in split_contexts_by_name.values() if c.parent_name
  }
  context_sets = [
      list(_IterParentContexts(n, split_contexts_by_name))
      for n in parent_splits_context_names
  ]
  # Visit them in order of: base, base+chrome, base+chrome+thing.
  context_sets.sort(key=lambda x: (len(x), x[0].name))

  # Ensure there are no missing references when considering all dex files.
  error_title = 'DEX contains references to non-existent symbols after R8.'
  dex_files = sorted(c.final_output_path
                     for c in split_contexts_by_name.values())
  if _CheckForMissingSymbols(options, dex_files, error_title):
    # Failed but didn't raise due to warnings_as_errors=False
    return

  for context_set in context_sets:
    # Ensure there are no references from base -> chrome module, or from
    # chrome -> feature modules.
    error_title = (f'DEX within module "{context_set[0].name}" contains '
                   'reference(s) to symbols within child splits')
    dex_files = [c.final_output_path for c in context_set]
    # Each check currently takes about 3 seconds on a fast dev machine, and we
    # run 3 of them (all, base, base+chrome).
    # We could run them concurrently, to shave off 5-6 seconds, but would need
    # to make sure that the order is maintained.
    if _CheckForMissingSymbols(options, dex_files, error_title):
      # Failed but didn't raise due to warnings_as_errors=False
      return


def _Run(options):
  # ProGuard configs that are derived from flags.
  logging.debug('Preparing configs')
  dynamic_config_data = _CreateDynamicConfig(options)

  logging.debug('Looking for embedded configs')
  libraries = options.sdk_jars + options.sdk_extension_jars

  embedded_configs = {}
  for jar_path in options.input_paths:
    _ExtractEmbeddedConfigs(jar_path, embedded_configs)

  # ProGuard configs that are derived from flags.
  merged_configs = _CombineConfigs(options.proguard_configs,
                                   dynamic_config_data,
                                   embedded_configs,
                                   exclude_generated=True)

  depfile_inputs = options.proguard_configs + options.input_paths + libraries
  if options.expected_file:
    diff_utils.CheckExpectations(merged_configs, options)
    if options.only_verify_expectations:
      action_helpers.write_depfile(options.depfile,
                                   options.actual_file,
                                   inputs=depfile_inputs)
      return

  if options.keep_rules_output_path:
    _OutputKeepRules(options.r8_path, options.input_paths, libraries,
                     options.keep_rules_targets_regex,
                     options.keep_rules_output_path)
    return

  split_contexts_by_name = _OptimizeWithR8(options, options.proguard_configs,
                                           libraries, dynamic_config_data)

  if not options.disable_checks:
    logging.debug('Running tracereferences')
    _DoTraceReferencesChecks(options, split_contexts_by_name)

  for output in options.extra_mapping_output_paths:
    shutil.copy(options.mapping_output, output)

  if options.apply_mapping:
    depfile_inputs.append(options.apply_mapping)

  _MaybeWriteStampAndDepFile(options, depfile_inputs)


def main():
  build_utils.InitLogging('PROGUARD_DEBUG')
  options = _ParseOptions()

  if options.dump_inputs:
    # Dumping inputs causes output to be emitted, avoid failing due to stdout.
    options.warnings_as_errors = False
    # Use dumpinputtodirectory instead of dumpinputtofile to avoid failing the
    # build and keep running tracereferences.
    dump_dir_name = _DUMP_DIR_NAME
    dump_dir_path = pathlib.Path(dump_dir_name)
    if dump_dir_path.exists():
      shutil.rmtree(dump_dir_path)
    # The directory needs to exist before r8 adds the zip files in it.
    dump_dir_path.mkdir()

  # This ensure that the final outputs are zipped and easily uploaded to a bug.
  try:
    _Run(options)
  finally:
    if options.dump_inputs:
      zip_helpers.zip_directory('r8inputs.zip', _DUMP_DIR_NAME)


if __name__ == '__main__':
  main()
