#!/usr/bin/env python3
#
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import functools
import itertools
import logging
import optparse
import os
import pathlib
import re
import shlex
import shutil
import sys
import time
import zipfile

import javac_output_processor
from util import build_utils
from util import md5_check
from util import jar_info_utils
from util import server_utils
import action_helpers  # build_utils adds //build to sys.path.
import zip_helpers

_JAVAC_EXTRACTOR = os.path.join(build_utils.DIR_SOURCE_ROOT, 'third_party',
                                'android_prebuilts', 'build_tools', 'common',
                                'framework', 'javac_extractor.jar')

# Add a check here to cause the suggested fix to be applied while compiling.
# Use this when trying to enable more checks.
ERRORPRONE_CHECKS_TO_APPLY = []

# Full list of checks: https://errorprone.info/bugpatterns
ERRORPRONE_WARNINGS_TO_DISABLE = [
    'InlineMeInliner',
    'InlineMeSuggester',
    # These are all for Javadoc, which we don't really care about.
    # vvv
    'InvalidBlockTag',
    'InvalidParam',
    'InvalidLink',
    'InvalidInlineTag',
    'MalformedInlineTag',
    'MissingSummary',
    'UnescapedEntity',
    'UnrecognisedJavadocTag',
    # ^^^
    'StaticAssignmentInConstructor',
    'MutablePublicArray',
    'NonCanonicalType',
    'ReturnValueIgnored',
    'DoNotClaimAnnotations',
    'JavaUtilDate',
    'IdentityHashMapUsage',
    'StaticMockMember',
    'MissingSuperCall',
    'ToStringReturnsNull',
    # Triggers in tests where this is useful to do.
    'StaticAssignmentOfThrowable',
    # TODO(crbug.com/41384349): Follow steps in bug.
    'CatchAndPrintStackTrace',
    # TODO(crbug.com/41364336): Follow steps in bug.
    'SynchronizeOnNonFinalField',
    # TODO(crbug.com/41364806): Follow steps in bug.
    'TypeParameterUnusedInFormals',
    # Android platform default is always UTF-8.
    # https://developer.android.com/reference/java/nio/charset/Charset.html#defaultCharset()
    'DefaultCharset',
    # Low priority since the alternatives still work.
    'JdkObsolete',
    # There are lots of times when we just want to post a task.
    'FutureReturnValueIgnored',
    # Just false positives in our code.
    'ThreadJoinLoop',
    # Low priority corner cases with String.split.
    # Linking Guava and using Splitter was rejected
    # in the https://chromium-review.googlesource.com/c/chromium/src/+/871630.
    'StringSplitter',
    # Preferred to use another method since it propagates exceptions better.
    'ClassNewInstance',
    # Results in false positives.
    'ThreadLocalUsage',
    # Also just false positives.
    'Finally',
    # False positives for Chromium.
    'FragmentNotInstantiable',
    # Low priority to fix.
    'HidingField',
    # Low priority.
    'EqualsHashCode',
    # Not necessary for tests.
    'OverrideThrowableToString',
    # Nice to have better type safety.
    'CollectionToArraySafeParameter',
    # Triggers on private methods that are @CalledByNative.
    'UnusedMethod',
    # Triggers on generated R.java files.
    'UnusedVariable',
    # Not that useful.
    'UnsafeReflectiveConstructionCast',
    # Not that useful.
    'MixedMutabilityReturnType',
    # Nice to have.
    'EqualsGetClass',
    # A lot of false-positives from CharSequence.equals().
    'UndefinedEquals',
    # Dagger generated code triggers this.
    'SameNameButDifferent',
    # Does not apply to Android because it assumes no desugaring.
    'UnnecessaryLambda',
    # Nice to have.
    'EmptyCatch',
    # Nice to have.
    'BadImport',
    # Nice to have.
    'UseCorrectAssertInTests',
    # Nice to have.
    'InlineFormatString',
    # Must be off since we are now passing in annotation processor generated
    # code as a source jar (deduplicating work with turbine).
    'RefersToDaggerCodegen',
    # We already have presubmit checks for this. We don't want it to fail
    # local compiles.
    'RemoveUnusedImports',
    # Only has false positives (would not want to enable this).
    'UnicodeEscape',
    # Nice to have.
    'AlreadyChecked',
    # A lot of existing violations. e.g. Should return List and not ArrayList
    'NonApiType',
    # Nice to have.
    'DirectInvocationOnMock',
    # Nice to have.
    'StringCharset',
    # Nice to have.
    'MockNotUsedInProduction',
    # Nice to have.
    'StringCaseLocaleUsage',
]

# Full list of checks: https://errorprone.info/bugpatterns
# Only those marked as "experimental" need to be listed here in order to be
# enabled.
ERRORPRONE_WARNINGS_TO_ENABLE = [
    'BinderIdentityRestoredDangerously',
    'EmptyIf',
    'EqualsBrokenForNull',
    'InvalidThrows',
    'LongLiteralLowerCaseSuffix',
    'MultiVariableDeclaration',
    'RedundantOverride',
    'StaticQualifiedUsingExpression',
    'TimeUnitMismatch',
    'UnnecessaryStaticImport',
    'UseBinds',
    'WildcardImport',
]


def ProcessJavacOutput(output, target_name):
  # These warnings cannot be suppressed even for third party code. Deprecation
  # warnings especially do not help since we must support older android version.
  deprecated_re = re.compile(r'Note: .* uses? or overrides? a deprecated API')
  unchecked_re = re.compile(
      r'(Note: .* uses? unchecked or unsafe operations.)$')
  recompile_re = re.compile(r'(Note: Recompile with -Xlint:.* for details.)$')

  def ApplyFilters(line):
    return not (deprecated_re.match(line) or unchecked_re.match(line)
                or recompile_re.match(line))

  output = build_utils.FilterReflectiveAccessJavaWarnings(output)

  # Warning currently cannot be silenced via javac flag.
  if 'Unsafe is internal proprietary API' in output:
    # Example:
    # HiddenApiBypass.java:69: warning: Unsafe is internal proprietary API and
    # may be removed in a future release
    # import sun.misc.Unsafe;
    #                 ^
    output = re.sub(r'.*?Unsafe is internal proprietary API[\s\S]*?\^\n', '',
                    output)
    output = re.sub(r'\d+ warnings\n', '', output)

  lines = (l for l in output.split('\n') if ApplyFilters(l))

  output_processor = javac_output_processor.JavacOutputProcessor(target_name)
  lines = output_processor.Process(lines)

  return '\n'.join(lines)


def CreateJarFile(jar_path,
                  classes_dir,
                  services_map=None,
                  additional_jar_files=None,
                  extra_classes_jar=None):
  """Zips files from compilation into a single jar."""
  logging.info('Start creating jar file: %s', jar_path)
  with action_helpers.atomic_output(jar_path) as f:
    with zipfile.ZipFile(f.name, 'w') as z:
      zip_helpers.zip_directory(z, classes_dir)
      if services_map:
        for service_class, impl_classes in sorted(services_map.items()):
          zip_path = 'META-INF/services/' + service_class
          data = ''.join(f'{x}\n' for x in sorted(impl_classes))
          zip_helpers.add_to_zip_hermetic(z, zip_path, data=data)

      if additional_jar_files:
        for src_path, zip_path in additional_jar_files:
          zip_helpers.add_to_zip_hermetic(z, zip_path, src_path=src_path)
      if extra_classes_jar:
        path_transform = lambda p: p if p.endswith('.class') else None
        zip_helpers.merge_zips(z, [extra_classes_jar],
                               path_transform=path_transform)
  logging.info('Completed jar file: %s', jar_path)


# Java lines end in semicolon, whereas Kotlin lines do not.
_PACKAGE_RE = re.compile(r'^package\s+(.*?)(;|\s*$)', flags=re.MULTILINE)

_SERVICE_IMPL_RE = re.compile(
    r'^([\t ]*)@ServiceImpl\(\s*(.+?)\.class\)(.*?)\sclass\s+(\w+)',
    flags=re.MULTILINE | re.DOTALL)

# Finds all top-level classes (by looking for those that are not indented).
_TOP_LEVEL_CLASSES_RE = re.compile(
    # Start of line, or after /* package */
    r'^(?:/\*.*\*/\s*)?'
    # Annotations
    r'(?:@\w+(?:\(.*\))\s+)*'
    r'(?:(?:public|protected|private)\s+)?'
    r'(?:(?:static|abstract|final|sealed)\s+)*'
    r'(?:class|@?interface|enum|record)\s+'
    r'(\w+?)\b[^"]*?$',
    flags=re.MULTILINE)


def ParseJavaSource(data, services_map, path=None):
  """This should support both Java and Kotlin files."""
  package_name = ''
  if m := _PACKAGE_RE.search(data):
    package_name = m.group(1)

  class_names = _TOP_LEVEL_CLASSES_RE.findall(data)

  # Very rare, so worth an upfront check.
  if '@ServiceImpl' in data:
    for indent, service_class, modifiers, impl_class in (
        _SERVICE_IMPL_RE.findall(data)):
      if 'public' not in modifiers:
        raise Exception(f'@ServiceImpl can be used only on public classes '
                        f'(when parsing {path})')
      # Assume indent means nested class that is one level deep.
      if indent:
        impl_class = f'{class_names[0]}${impl_class}'
      else:
        assert class_names[0] == impl_class

      # Parse imports to resolve the class.
      dot_idx = service_class.find('.')
      # Handle @ServiceImpl(OuterClass.InnerClass.class)
      outer_class = service_class if dot_idx == -1 else service_class[:dot_idx]

      if m := re.search(r'^import\s+([\w\.]*\.' + outer_class + r')[;\s]',
                        data,
                        flags=re.MULTILINE):
        service_class = m.group(1) + service_class[len(outer_class):]
      else:
        service_class = f'{package_name}.{service_class}'

      # Convert OuterClass.InnerClass -> OuterClass$InnerClass.
      for m in list(re.finditer(r'\.[A-Z]', service_class))[1:]:
        idx = m.start()
        service_class = service_class[:idx] + '$' + service_class[idx + 1:]

      services_map[service_class].append(f'{package_name}.{impl_class}')

  return package_name, class_names


class _MetadataParser:

  def __init__(self, chromium_code, excluded_globs):
    self._chromium_code = chromium_code
    self._excluded_globs = excluded_globs
    # Map of .java path -> .srcjar/nested/path.java.
    self._srcjar_files = {}
    # Map of @ServiceImpl class -> impl class
    self.services_map = collections.defaultdict(list)

  def AddSrcJarSources(self, srcjar_path, extracted_paths, parent_dir):
    for path in extracted_paths:
      # We want the path inside the srcjar so the viewer can have a tree
      # structure.
      self._srcjar_files[path] = '{}/{}'.format(
          srcjar_path, os.path.relpath(path, parent_dir))

  def _CheckPathMatchesClassName(self, source_file, package_name, class_name):
    if source_file.endswith('.java'):
      parts = package_name.split('.') + [class_name + '.java']
    else:
      parts = package_name.split('.') + [class_name + '.kt']
    expected_suffix = os.path.sep.join(parts)
    if not source_file.endswith(expected_suffix):
      raise Exception(('Source package+class name do not match its path.\n'
                       'Actual path: %s\nExpected path: %s') %
                      (source_file, expected_suffix))

  def _ProcessInfo(self, java_file, package_name, class_names, source):
    for class_name in class_names:
      yield '{}.{}'.format(package_name, class_name)
      # Skip aidl srcjars since they don't indent code correctly.
      if '_aidl.srcjar' in source:
        continue
      assert not self._chromium_code or len(class_names) == 1, (
          'Chromium java files must only have one class: {} found: {}'.format(
              source, class_names))
      if self._chromium_code:
        # This check is not necessary but nice to check this somewhere.
        self._CheckPathMatchesClassName(java_file, package_name, class_names[0])

  def _ShouldIncludeInJarInfo(self, fully_qualified_name):
    name_as_class_glob = fully_qualified_name.replace('.', '/') + '.class'
    return not build_utils.MatchesGlob(name_as_class_glob, self._excluded_globs)

  def ParseAndWriteInfoFile(self, output_path, java_files, kt_files=None):
    """Writes a .jar.info file.

    Maps fully qualified names for classes to either the java file that they
    are defined in or the path of the srcjar that they came from.
    """
    logging.info('Collecting info file entries')
    entries = {}
    for path in itertools.chain(java_files, kt_files or []):
      data = pathlib.Path(path).read_text()
      package_name, class_names = ParseJavaSource(data,
                                                  self.services_map,
                                                  path=path)
      source = self._srcjar_files.get(path, path)
      for fully_qualified_name in self._ProcessInfo(path, package_name,
                                                    class_names, source):
        if self._ShouldIncludeInJarInfo(fully_qualified_name):
          entries[fully_qualified_name] = path

    logging.info('Writing info file: %s', output_path)
    with action_helpers.atomic_output(output_path, mode='wb') as f:
      jar_info_utils.WriteJarInfoFile(f, entries, self._srcjar_files)
    logging.info('Completed info file: %s', output_path)


def _OnStaleMd5(changes, options, javac_cmd, javac_args, java_files, kt_files):
  logging.info('Starting _OnStaleMd5')

  # Use the build server for errorprone runs.
  if (options.enable_errorprone and not options.skip_build_server
      and server_utils.MaybeRunCommand(name=options.target_name,
                                       argv=sys.argv,
                                       stamp_file=options.jar_path,
                                       force=options.use_build_server)):
    return

  if options.enable_kythe_annotations:
    # Kythe requires those env variables to be set and compile_java.py does the
    # same
    if not os.environ.get('KYTHE_ROOT_DIRECTORY') or \
        not os.environ.get('KYTHE_OUTPUT_DIRECTORY'):
      raise Exception('--enable-kythe-annotations requires '
                      'KYTHE_ROOT_DIRECTORY and KYTHE_OUTPUT_DIRECTORY '
                      'environment variables to be set.')
    javac_extractor_cmd = build_utils.JavaCmd() + [
        '--add-exports=jdk.compiler/com.sun.tools.javac.api=ALL-UNNAMED',
        '--add-exports=jdk.compiler/com.sun.tools.javac.code=ALL-UNNAMED',
        '--add-exports=jdk.compiler/com.sun.tools.javac.file=ALL-UNNAMED',
        '--add-exports=jdk.compiler/com.sun.tools.javac.util=ALL-UNNAMED',
        '--add-exports=jdk.compiler/com.sun.tools.javac.main=ALL-UNNAMED',
        '--add-exports=jdk.compiler/com.sun.tools.javac.tree=ALL-UNNAMED',
        '--add-exports=jdk.internal.opt/jdk.internal.opt=ALL-UNNAMED',
        '-jar',
        _JAVAC_EXTRACTOR,
    ]
    try:
      # _RunCompiler()'s partial javac implementation does not support
      # generating outputs in $KYTHE_OUTPUT_DIRECTORY.
      _RunCompiler(changes,
                   options,
                   javac_extractor_cmd + javac_args,
                   java_files,
                   options.jar_path + '.javac_extractor',
                   enable_partial_javac=False)
    except build_utils.CalledProcessError as e:
      # Having no index for particular target is better than failing entire
      # codesearch. Log and error and move on.
      logging.error('Could not generate kzip: %s', e)

  intermediates_out_dir = None
  jar_info_path = None
  if not options.enable_errorprone:
    # Delete any stale files in the generated directory. The purpose of
    # options.generated_dir is for codesearch and Android Studio.
    shutil.rmtree(options.generated_dir, True)
    intermediates_out_dir = options.generated_dir

    # Write .info file only for the main javac invocation (no need to do it
    # when running Error Prone.
    jar_info_path = options.jar_path + '.info'

  # Compiles with Error Prone take twice as long to run as pure javac. Thus GN
  # rules run both in parallel, with Error Prone only used for checks.
  try:
    _RunCompiler(changes,
                 options,
                 javac_cmd + javac_args,
                 java_files,
                 options.jar_path,
                 kt_files=kt_files,
                 jar_info_path=jar_info_path,
                 intermediates_out_dir=intermediates_out_dir,
                 enable_partial_javac=True)
  except build_utils.CalledProcessError as e:
    # Do not output stacktrace as it takes up space on gerrit UI, forcing
    # you to click though to find the actual compilation error. It's never
    # interesting to see the Python stacktrace for a Java compilation error.
    sys.stderr.write(e.output)
    sys.exit(1)

  logging.info('Completed all steps in _OnStaleMd5')


def _RunCompiler(changes,
                 options,
                 javac_cmd,
                 java_files,
                 jar_path,
                 kt_files=None,
                 jar_info_path=None,
                 intermediates_out_dir=None,
                 enable_partial_javac=False):
  """Runs java compiler.

  Args:
    changes: md5_check.Changes object.
    options: Object with command line flags.
    javac_cmd: Command to execute.
    java_files: List of java files passed from command line.
    jar_path: Path of output jar file.
    kt_files: List of Kotlin files passed from command line if any.
    jar_info_path: Path of the .info file to generate.
        If None, .info file will not be generated.
    intermediates_out_dir: Directory for saving intermediate outputs.
        If None a temporary directory is used.
    enable_partial_javac: Enables compiling only Java files which have changed
        in the special case that no method signatures have changed. This is
        useful for large GN targets.
        Not supported if compiling generates outputs other than |jar_path| and
        |jar_info_path|.
  """
  logging.info('Starting _RunCompiler')

  java_files = java_files.copy()
  java_srcjars = options.java_srcjars
  parse_java_files = jar_info_path is not None

  # Use jar_path's directory to ensure paths are relative (needed for rbe).
  temp_dir = jar_path + '.staging'
  build_utils.DeleteDirectory(temp_dir)
  os.makedirs(temp_dir)
  metadata_parser = _MetadataParser(options.chromium_code,
                                    options.jar_info_exclude_globs)
  try:
    classes_dir = os.path.join(temp_dir, 'classes')

    if java_files:
      os.makedirs(classes_dir)

      if enable_partial_javac and changes:
        all_changed_paths_are_java = all(
            p.endswith(".java") for p in changes.IterChangedPaths())
        if (all_changed_paths_are_java and not changes.HasStringChanges()
            and os.path.exists(jar_path)
            and (jar_info_path is None or os.path.exists(jar_info_path))):
          # Log message is used by tests to determine whether partial javac
          # optimization was used.
          logging.info('Using partial javac optimization for %s compile' %
                       (jar_path))

          # Header jar corresponding to |java_files| did not change.
          # As a build speed optimization (crbug.com/1170778), re-compile only
          # java files which have changed. Re-use old jar .info file.
          java_files = list(changes.IterChangedPaths())

          # Disable srcjar extraction, since we know the srcjar didn't show as
          # changed (only .java files).
          java_srcjars = None

          # @ServiceImpl has class retention, so will alter header jars when
          # modified (and hence not reach this block).
          # Likewise, nothing in .info files can change if header jar did not
          # change.
          parse_java_files = False

          # Extracts .class as well as META-INF/services.
          build_utils.ExtractAll(jar_path, classes_dir)

    if intermediates_out_dir is None:
      intermediates_out_dir = temp_dir

    input_srcjars_dir = os.path.join(intermediates_out_dir, 'input_srcjars')

    if java_srcjars:
      logging.info('Extracting srcjars to %s', input_srcjars_dir)
      build_utils.MakeDirectory(input_srcjars_dir)
      for srcjar in options.java_srcjars:
        extracted_files = build_utils.ExtractAll(
            srcjar, no_clobber=True, path=input_srcjars_dir, pattern='*.java')
        java_files.extend(extracted_files)
        if parse_java_files:
          metadata_parser.AddSrcJarSources(srcjar, extracted_files,
                                           input_srcjars_dir)
      logging.info('Done extracting srcjars')

    if java_files:
      # Don't include the output directory in the initial set of args since it
      # being in a temp dir makes it unstable (breaks md5 stamping).
      cmd = list(javac_cmd)
      cmd += ['-d', classes_dir]

      if options.classpath:
        cmd += ['-classpath', ':'.join(options.classpath)]

      # Pass source paths as response files to avoid extremely long command
      # lines that are tedius to debug.
      java_files_rsp_path = os.path.join(temp_dir, 'files_list.txt')
      with open(java_files_rsp_path, 'w') as f:
        f.write(' '.join(java_files))
      cmd += ['@' + java_files_rsp_path]

      process_javac_output_partial = functools.partial(
          ProcessJavacOutput, target_name=options.target_name)

      logging.debug('Build command %s', cmd)
      start = time.time()
      before_join_callback = None
      if parse_java_files:
        before_join_callback = lambda: metadata_parser.ParseAndWriteInfoFile(
            jar_info_path, java_files, kt_files)

      if options.print_javac_command_line:
        print(shlex.join(cmd))
        return

      build_utils.CheckOutput(cmd,
                              print_stdout=options.chromium_code,
                              stdout_filter=process_javac_output_partial,
                              stderr_filter=process_javac_output_partial,
                              fail_on_output=options.warnings_as_errors,
                              before_join_callback=before_join_callback)
      end = time.time() - start
      logging.info('Java compilation took %ss', end)
    elif parse_java_files:
      if options.print_javac_command_line:
        raise Exception('need java files for --print-javac-command-line.')
      metadata_parser.ParseAndWriteInfoFile(jar_info_path, java_files, kt_files)

    CreateJarFile(jar_path, classes_dir, metadata_parser.services_map,
                  options.additional_jar_files, options.kotlin_jar_path)

    # Remove input srcjars that confuse Android Studio:
    # https://crbug.com/353326240
    for root, _, files in os.walk(intermediates_out_dir):
      for subpath in files:
        p = os.path.join(root, subpath)
        # JNI Zero placeholders
        if '_jni_java/' in p and not p.endswith('Jni.java'):
          os.unlink(p)

    logging.info('Completed all steps in _RunCompiler')
  finally:
    # preserve temp_dir for rsp fie when --print-javac-command-line
    if not options.print_javac_command_line:
      shutil.rmtree(temp_dir)


def _ParseOptions(argv):
  parser = optparse.OptionParser()
  action_helpers.add_depfile_arg(parser)

  parser.add_option('--target-name', help='Fully qualified GN target name.')
  parser.add_option('--skip-build-server',
                    action='store_true',
                    help='Avoid using the build server.')
  parser.add_option('--use-build-server',
                    action='store_true',
                    help='Always use the build server.')
  parser.add_option(
      '--java-srcjars',
      action='append',
      default=[],
      help='List of srcjars to include in compilation.')
  parser.add_option(
      '--generated-dir',
      help='Subdirectory within target_gen_dir to place extracted srcjars and '
      'annotation processor output for codesearch to find.')
  parser.add_option('--classpath', action='append', help='Classpath to use.')
  parser.add_option(
      '--processorpath',
      action='append',
      help='GN list of jars that comprise the classpath used for Annotation '
      'Processors.')
  parser.add_option(
      '--processor-arg',
      dest='processor_args',
      action='append',
      help='key=value arguments for the annotation processors.')
  parser.add_option(
      '--additional-jar-file',
      dest='additional_jar_files',
      action='append',
      help='Additional files to package into jar. By default, only Java .class '
      'files are packaged into the jar. Files should be specified in '
      'format <filename>:<path to be placed in jar>.')
  parser.add_option(
      '--jar-info-exclude-globs',
      help='GN list of exclude globs to filter from generated .info files.')
  parser.add_option(
      '--chromium-code',
      type='int',
      help='Whether code being compiled should be built with stricter '
      'warnings for chromium code.')
  parser.add_option(
      '--errorprone-path', help='Use the Errorprone compiler at this path.')
  parser.add_option(
      '--enable-errorprone',
      action='store_true',
      help='Enable errorprone checks')
  parser.add_option(
      '--warnings-as-errors',
      action='store_true',
      help='Treat all warnings as errors.')
  parser.add_option('--jar-path', help='Jar output path.')
  parser.add_option(
      '--javac-arg',
      action='append',
      default=[],
      help='Additional arguments to pass to javac.')
  parser.add_option('--print-javac-command-line',
                    action='store_true',
                    help='Just show javac command line (for ide_query).')
  parser.add_option(
      '--enable-kythe-annotations',
      action='store_true',
      help='Enable generation of Kythe kzip, used for codesearch. Ensure '
      'proper environment variables are set before using this flag.')
  parser.add_option(
      '--header-jar',
      help='This is the header jar for the current target that contains '
      'META-INF/services/* files to be included in the output jar.')
  parser.add_option(
      '--kotlin-jar-path',
      help='Kotlin jar to be merged into the output jar. This contains the '
      ".class files from this target's .kt files.")

  options, args = parser.parse_args(argv)
  build_utils.CheckOptions(options, parser, required=('jar_path', ))

  options.classpath = action_helpers.parse_gn_list(options.classpath)
  options.processorpath = action_helpers.parse_gn_list(options.processorpath)
  options.java_srcjars = action_helpers.parse_gn_list(options.java_srcjars)
  options.jar_info_exclude_globs = action_helpers.parse_gn_list(
      options.jar_info_exclude_globs)

  additional_jar_files = []
  for arg in options.additional_jar_files or []:
    filepath, jar_filepath = arg.split(':')
    additional_jar_files.append((filepath, jar_filepath))
  options.additional_jar_files = additional_jar_files

  files = []
  for arg in args:
    # Interpret a path prefixed with @ as a file containing a list of sources.
    if arg.startswith('@'):
      files.extend(build_utils.ReadSourcesList(arg[1:]))
    else:
      files.append(arg)

  # The target's .sources file contains both Java and Kotlin files. We use
  # compile_kt.py to compile the Kotlin files to .class and header jars. Javac
  # is run only on .java files.
  java_files = [f for f in files if f.endswith('.java')]
  # Kotlin files are needed to populate the info file and attribute size in
  # supersize back to the appropriate Kotlin file.
  kt_files = [f for f in files if f.endswith('.kt')]

  return options, java_files, kt_files


def main(argv):
  build_utils.InitLogging('JAVAC_DEBUG')
  argv = build_utils.ExpandFileArgs(argv)
  options, java_files, kt_files = _ParseOptions(argv)

  javac_cmd = [build_utils.JAVAC_PATH]

  javac_args = [
      '-g',
      # Jacoco does not currently support a higher value.
      '--release',
      '17',
      # Chromium only allows UTF8 source files.  Being explicit avoids
      # javac pulling a default encoding from the user's environment.
      '-encoding',
      'UTF-8',
      # Prevent compiler from compiling .java files not listed as inputs.
      # See: http://blog.ltgt.net/most-build-tools-misuse-javac/
      '-sourcepath',
      ':',
      # protobuf-generated files fail this check (javadoc has @deprecated,
      # but method missing @Deprecated annotation).
      '-Xlint:-dep-ann',
      # Do not warn about finalize() methods. Android still intends to support
      # them.
      '-Xlint:-removal',
      # https://crbug.com/1441023
      '-J-XX:+PerfDisableSharedMem',
  ]

  if options.enable_errorprone:
    # All errorprone args are passed space-separated in a single arg.
    errorprone_flags = ['-Xplugin:ErrorProne']
    # Make everything a warning so that when treat_warnings_as_errors is false,
    # they do not fail the build.
    errorprone_flags += ['-XepAllErrorsAsWarnings']
    # Don't check generated files.
    errorprone_flags += ['-XepDisableWarningsInGeneratedCode']
    errorprone_flags.extend('-Xep:{}:OFF'.format(x)
                            for x in ERRORPRONE_WARNINGS_TO_DISABLE)
    errorprone_flags.extend('-Xep:{}:WARN'.format(x)
                            for x in ERRORPRONE_WARNINGS_TO_ENABLE)

    if ERRORPRONE_CHECKS_TO_APPLY:
      errorprone_flags += [
          '-XepPatchLocation:IN_PLACE',
          '-XepPatchChecks:,' + ','.join(ERRORPRONE_CHECKS_TO_APPLY)
      ]

    # These are required to use JDK 16, and are taken directly from
    # https://errorprone.info/docs/installation
    javac_args += [
        '-J--add-exports=jdk.compiler/com.sun.tools.javac.api=ALL-UNNAMED',
        '-J--add-exports=jdk.compiler/com.sun.tools.javac.file=ALL-UNNAMED',
        '-J--add-exports=jdk.compiler/com.sun.tools.javac.main=ALL-UNNAMED',
        '-J--add-exports=jdk.compiler/com.sun.tools.javac.model=ALL-UNNAMED',
        '-J--add-exports=jdk.compiler/com.sun.tools.javac.parser=ALL-UNNAMED',
        '-J--add-exports=jdk.compiler/com.sun.tools.javac.processing='
        'ALL-UNNAMED',
        '-J--add-exports=jdk.compiler/com.sun.tools.javac.tree=ALL-UNNAMED',
        '-J--add-exports=jdk.compiler/com.sun.tools.javac.util=ALL-UNNAMED',
        '-J--add-opens=jdk.compiler/com.sun.tools.javac.code=ALL-UNNAMED',
        '-J--add-opens=jdk.compiler/com.sun.tools.javac.comp=ALL-UNNAMED',
    ]

    javac_args += ['-XDcompilePolicy=simple', ' '.join(errorprone_flags)]

    # This flag quits errorprone after checks and before code generation, since
    # we do not need errorprone outputs, this speeds up errorprone by 4 seconds
    # for chrome_java.
    if not ERRORPRONE_CHECKS_TO_APPLY:
      javac_args += ['-XDshould-stop.ifNoError=FLOW']

  # This effectively disables all annotation processors, even including
  # annotation processors in service provider configuration files named
  # META-INF/. See the following link for reference:
  #     https://docs.oracle.com/en/java/javase/11/tools/javac.html
  javac_args.extend(['-proc:none'])

  if options.processorpath:
    javac_args.extend(['-processorpath', ':'.join(options.processorpath)])
  if options.processor_args:
    for arg in options.processor_args:
      javac_args.extend(['-A%s' % arg])

  javac_args.extend(options.javac_arg)

  if options.print_javac_command_line:
    if options.java_srcjars:
      raise Exception(
          '--print-javac-command-line does not work with --java-srcjars')
    _OnStaleMd5(None, options, javac_cmd, javac_args, java_files, kt_files)
    return 0

  classpath_inputs = options.classpath + options.processorpath

  depfile_deps = classpath_inputs
  # Files that are already inputs in GN should go in input_paths.
  input_paths = ([build_utils.JAVAC_PATH] + depfile_deps +
                 options.java_srcjars + java_files + kt_files)
  if options.header_jar:
    input_paths.append(options.header_jar)
  input_paths += [x[0] for x in options.additional_jar_files]

  output_paths = [options.jar_path]
  if not options.enable_errorprone:
    output_paths += [options.jar_path + '.info']

  input_strings = (javac_cmd + javac_args + options.classpath + java_files +
                   kt_files +
                   [options.warnings_as_errors, options.jar_info_exclude_globs])

  # Use md5_check for |pass_changes| feature.
  md5_check.CallAndWriteDepfileIfStale(lambda changes: _OnStaleMd5(
      changes, options, javac_cmd, javac_args, java_files, kt_files),
                                       options,
                                       depfile_deps=depfile_deps,
                                       input_paths=input_paths,
                                       input_strings=input_strings,
                                       output_paths=output_paths,
                                       pass_changes=True)
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
