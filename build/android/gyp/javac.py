#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import distutils.spawn
import itertools
import logging
import multiprocessing
import optparse
import os
import shutil
import re
import sys
import zipfile

from util import build_utils
from util import md5_check
from util import jar_info_utils

import jar

sys.path.insert(
    0,
    os.path.join(build_utils.DIR_SOURCE_ROOT, 'third_party', 'colorama', 'src'))
import colorama


ERRORPRONE_WARNINGS_TO_TURN_OFF = [
  # TODO(crbug.com/834807): Follow steps in bug
  'DoubleBraceInitialization',
  # TODO(crbug.com/834790): Follow steps in bug.
  'CatchAndPrintStackTrace',
  # TODO(crbug.com/801210): Follow steps in bug.
  'SynchronizeOnNonFinalField',
  # TODO(crbug.com/802073): Follow steps in bug.
  'TypeParameterUnusedInFormals',
  # TODO(crbug.com/803484): Follow steps in bug.
  'CatchFail',
  # TODO(crbug.com/803485): Follow steps in bug.
  'JUnitAmbiguousTestClass',
  # Android platform default is always UTF-8.
  # https://developer.android.com/reference/java/nio/charset/Charset.html#defaultCharset()
  'DefaultCharset',
  # Low priority since the alternatives still work.
  'JdkObsolete',
  # We don't use that many lambdas.
  'FunctionalInterfaceClash',
  # There are lots of times when we just want to post a task.
  'FutureReturnValueIgnored',
  # Nice to be explicit about operators, but not necessary.
  'OperatorPrecedence',
  # Just false positives in our code.
  'ThreadJoinLoop',
  # Low priority corner cases with String.split.
  # Linking Guava and using Splitter was rejected
  # in the https://chromium-review.googlesource.com/c/chromium/src/+/871630.
  'StringSplitter',
  # Preferred to use another method since it propagates exceptions better.
  'ClassNewInstance',
  # Nice to have static inner classes but not necessary.
  'ClassCanBeStatic',
  # Explicit is better than implicit.
  'FloatCast',
  # Results in false positives.
  'ThreadLocalUsage',
  # Also just false positives.
  'Finally',
  # False positives for Chromium.
  'FragmentNotInstantiable',
  # Low priority to fix.
  'HidingField',
  # Low priority.
  'IntLongMath',
  # Low priority.
  'BadComparable',
  # Low priority.
  'EqualsHashCode',
  # Nice to fix but low priority.
  'TypeParameterShadowing',
  # Good to have immutable enums, also low priority.
  'ImmutableEnumChecker',
  # False positives for testing.
  'InputStreamSlowMultibyteRead',
  # Nice to have better primitives.
  'BoxedPrimitiveConstructor',
  # Not necessary for tests.
  'OverrideThrowableToString',
  # Nice to have better type safety.
  'CollectionToArraySafeParameter',
  # Makes logcat debugging more difficult, and does not provide obvious
  # benefits in the Chromium codebase.
  'ObjectToString',
]

ERRORPRONE_WARNINGS_TO_ERROR = [
  # Add warnings to this after fixing/suppressing all instances in our codebase.
  'ArgumentSelectionDefectChecker',
  'AssertionFailureIgnored',
  'FloatingPointLiteralPrecision',
  'JavaLangClash',
  'MissingFail',
  'MissingOverride',
  'NarrowingCompoundAssignment',
  'OrphanedFormatString',
  'ParameterName',
  'ParcelableCreator',
  'ReferenceEquality',
  'StaticGuardedByInstance',
  'StaticQualifiedUsingExpression',
  'UseCorrectAssertInTests',
]


def ProcessJavacOutput(output):
  fileline_prefix = r'(?P<fileline>(?P<file>[-.\w/\\]+.java):(?P<line>[0-9]+):)'
  warning_re = re.compile(
      fileline_prefix + r'(?P<full_message> warning: (?P<message>.*))$')
  error_re = re.compile(
      fileline_prefix + r'(?P<full_message> (?P<message>.*))$')
  marker_re = re.compile(r'\s*(?P<marker>\^)\s*$')

  # These warnings cannot be suppressed even for third party code. Deprecation
  # warnings especially do not help since we must support older android version.
  deprecated_re = re.compile(
      r'(Note: .* uses? or overrides? a deprecated API.)$')
  unchecked_re = re.compile(
      r'(Note: .* uses? unchecked or unsafe operations.)$')
  recompile_re = re.compile(r'(Note: Recompile with -Xlint:.* for details.)$')

  warning_color = ['full_message', colorama.Fore.YELLOW + colorama.Style.DIM]
  error_color = ['full_message', colorama.Fore.MAGENTA + colorama.Style.BRIGHT]
  marker_color = ['marker',  colorama.Fore.BLUE + colorama.Style.BRIGHT]

  def Colorize(line, regex, color):
    match = regex.match(line)
    start = match.start(color[0])
    end = match.end(color[0])
    return (line[:start]
            + color[1] + line[start:end]
            + colorama.Fore.RESET + colorama.Style.RESET_ALL
            + line[end:])

  def ApplyFilters(line):
    return not (deprecated_re.match(line)
        or unchecked_re.match(line)
        or recompile_re.match(line))

  def ApplyColors(line):
    if warning_re.match(line):
      line = Colorize(line, warning_re, warning_color)
    elif error_re.match(line):
      line = Colorize(line, error_re, error_color)
    elif marker_re.match(line):
      line = Colorize(line, marker_re, marker_color)
    return line

  return '\n'.join(map(ApplyColors, filter(ApplyFilters, output.split('\n'))))


def _ExtractClassFiles(jar_path, dest_dir, java_files):
  """Extracts all .class files not corresponding to |java_files|."""
  # Two challenges exist here:
  # 1. |java_files| have prefixes that are not represented in the the jar paths.
  # 2. A single .java file results in multiple .class files when it contains
  #    nested classes.
  # Here's an example:
  #   source path: ../../base/android/java/src/org/chromium/Foo.java
  #   jar paths: org/chromium/Foo.class, org/chromium/Foo$Inner.class
  # To extract only .class files not related to the given .java files, we strip
  # off ".class" and "$*.class" and use a substring match against java_files.
  def extract_predicate(path):
    if not path.endswith('.class'):
      return False
    path_without_suffix = re.sub(r'(?:\$|\.)[^/]*class$', '', path)
    partial_java_path = path_without_suffix + '.java'
    return not any(p.endswith(partial_java_path) for p in java_files)

  logging.info('Extracting class files from %s', jar_path)
  build_utils.ExtractAll(jar_path, path=dest_dir, predicate=extract_predicate)
  for path in build_utils.FindInDirectory(dest_dir, '*.class'):
    shutil.copystat(jar_path, path)


def _ParsePackageAndClassNames(java_file):
  package_name = ''
  class_names = []
  with open(java_file) as f:
    for l in f:
      # Strip unindented comments.
      # Considers a leading * as a continuation of a multi-line comment (our
      # linter doesn't enforce a space before it like there should be).
      l = re.sub(r'^(?://.*|/?\*.*?(?:\*/\s*|$))', '', l)

      m = re.match(r'package\s+(.*?);', l)
      if m and not package_name:
        package_name = m.group(1)

      # Not exactly a proper parser, but works for sources that Chrome uses.
      # In order to not match nested classes, it just checks for lack of indent.
      m = re.match(r'(?:\S.*?)?(?:class|@?interface|enum)\s+(.+?)\b', l)
      if m:
        class_names.append(m.group(1))
  return package_name, class_names


def _CheckPathMatchesClassName(java_file, package_name, class_name):
  parts = package_name.split('.') + [class_name + '.java']
  expected_path_suffix = os.path.sep.join(parts)
  if not java_file.endswith(expected_path_suffix):
    raise Exception(('Java package+class name do not match its path.\n'
                     'Actual path: %s\nExpected path: %s') %
                    (java_file, expected_path_suffix))


def _MoveGeneratedJavaFilesToGenDir(classes_dir, generated_java_dir):
  # Move any Annotation Processor-generated .java files into $out/gen
  # so that codesearch can find them.
  javac_generated_sources = []
  for src_path in build_utils.FindInDirectory(classes_dir, '*.java'):
    dst_path = os.path.join(generated_java_dir,
                            os.path.relpath(src_path, classes_dir))
    build_utils.MakeDirectory(os.path.dirname(dst_path))
    shutil.move(src_path, dst_path)
    javac_generated_sources.append(dst_path)
  return javac_generated_sources


def _ProcessJavaFileForInfo(java_file):
  package_name, class_names = _ParsePackageAndClassNames(java_file)
  return java_file, package_name, class_names


def _ProcessInfo(java_file, package_name, class_names, source, chromium_code):
  for class_name in class_names:
    yield '{}.{}'.format(package_name, class_name)
    # Skip aidl srcjars since they don't indent code correctly.
    if '_aidl.srcjar' in source:
      continue
    assert not chromium_code or len(class_names) == 1, (
        'Chromium java files must only have one class: {}'.format(source))
    if chromium_code:
      # This check is not necessary but nice to check this somewhere.
      _CheckPathMatchesClassName(java_file, package_name, class_names[0])


def _ShouldIncludeInJarInfo(fully_qualified_name, excluded_globs):
  name_as_class_glob = fully_qualified_name.replace('.', '/') + '.class'
  return not build_utils.MatchesGlob(name_as_class_glob, excluded_globs)


def _CreateInfoFile(java_files, jar_path, chromium_code, srcjar_files,
                    classes_dir, generated_java_dir, excluded_globs):
  """Writes a .jar.info file.

  This maps fully qualified names for classes to either the java file that they
  are defined in or the path of the srcjar that they came from.
  """
  output_path = jar_path + '.info'
  logging.info('Start creating info file: %s', output_path)
  javac_generated_sources = _MoveGeneratedJavaFilesToGenDir(
      classes_dir, generated_java_dir)
  logging.info('Finished moving generated java files: %s', output_path)
  # 2 processes saves ~0.9s, 3 processes saves ~1.2s, 4 processes saves ~1.2s.
  pool = multiprocessing.Pool(processes=3)
  results = pool.imap_unordered(
      _ProcessJavaFileForInfo,
      itertools.chain(java_files, javac_generated_sources),
      chunksize=10)
  pool.close()
  all_info_data = {}
  for java_file, package_name, class_names in results:
    source = srcjar_files.get(java_file, java_file)
    for fully_qualified_name in _ProcessInfo(
        java_file, package_name, class_names, source, chromium_code):
      if _ShouldIncludeInJarInfo(fully_qualified_name, excluded_globs):
        all_info_data[fully_qualified_name] = java_file
  logging.info('Writing info file: %s', output_path)
  with build_utils.AtomicOutput(output_path) as f:
    jar_info_utils.WriteJarInfoFile(f, all_info_data, srcjar_files)
  logging.info('Completed info file: %s', output_path)


def _CreateJarFile(jar_path, provider_configurations, additional_jar_files,
                   classes_dir):
  logging.info('Start creating jar file: %s', jar_path)
  with build_utils.AtomicOutput(jar_path) as f:
    jar.JarDirectory(
        classes_dir,
        f.name,
        # Avoid putting generated java files into the jar since
        # _MoveGeneratedJavaFilesToGenDir has not completed yet
        predicate=lambda name: not name.endswith('.java'),
        provider_configurations=provider_configurations,
        additional_files=additional_jar_files)
  logging.info('Completed jar file: %s', jar_path)


def _OnStaleMd5(options, javac_cmd, java_files, classpath):
  logging.info('Starting _OnStaleMd5')

  # Compiles with Error Prone take twice as long to run as pure javac. Thus GN
  # rules run both in parallel, with Error Prone only used for checks.
  save_outputs = not options.enable_errorprone

  with build_utils.TempDir() as temp_dir:
    srcjars = options.java_srcjars

    classes_dir = os.path.join(temp_dir, 'classes')
    os.makedirs(classes_dir)

    if save_outputs:
      generated_java_dir = options.generated_dir
    else:
      generated_java_dir = os.path.join(temp_dir, 'gen')

    shutil.rmtree(generated_java_dir, True)

    srcjar_files = {}
    if srcjars:
      logging.info('Extracting srcjars to %s', generated_java_dir)
      build_utils.MakeDirectory(generated_java_dir)
      jar_srcs = []
      for srcjar in options.java_srcjars:
        extracted_files = build_utils.ExtractAll(
            srcjar, no_clobber=True, path=generated_java_dir, pattern='*.java')
        for path in extracted_files:
          # We want the path inside the srcjar so the viewer can have a tree
          # structure.
          srcjar_files[path] = '{}/{}'.format(
              srcjar, os.path.relpath(path, generated_java_dir))
        jar_srcs.extend(extracted_files)
      logging.info('Done extracting srcjars')
      java_files.extend(jar_srcs)

    if java_files:
      # Don't include the output directory in the initial set of args since it
      # being in a temp dir makes it unstable (breaks md5 stamping).
      cmd = javac_cmd + ['-d', classes_dir]

      # Pass classpath and source paths as response files to avoid extremely
      # long command lines that are tedius to debug.
      if classpath:
        cmd += ['-classpath', ':'.join(classpath)]

      java_files_rsp_path = os.path.join(temp_dir, 'files_list.txt')
      with open(java_files_rsp_path, 'w') as f:
        f.write(' '.join(java_files))
      cmd += ['@' + java_files_rsp_path]

      logging.debug('Build command %s', cmd)
      build_utils.CheckOutput(
          cmd,
          print_stdout=options.chromium_code,
          stderr_filter=ProcessJavacOutput)
      logging.info('Finished build command')

    if save_outputs:
      # Creating the jar file takes the longest, start it first on a separate
      # process to unblock the rest of the post-processing steps.
      jar_file_worker = multiprocessing.Process(
          target=_CreateJarFile,
          args=(options.jar_path, options.provider_configurations,
                options.additional_jar_files, classes_dir))
      jar_file_worker.start()
    else:
      jar_file_worker = None
      build_utils.Touch(options.jar_path)

    if save_outputs:
      _CreateInfoFile(java_files, options.jar_path, options.chromium_code,
                      srcjar_files, classes_dir, generated_java_dir,
                      options.jar_info_exclude_globs)
    else:
      build_utils.Touch(options.jar_path + '.info')

    if jar_file_worker:
      jar_file_worker.join()
    logging.info('Completed all steps in _OnStaleMd5')


def _ParseOptions(argv):
  parser = optparse.OptionParser()
  build_utils.AddDepfileOption(parser)

  parser.add_option(
      '--java-srcjars',
      action='append',
      default=[],
      help='List of srcjars to include in compilation.')
  parser.add_option(
      '--generated-dir',
      help='Subdirectory within target_gen_dir to place extracted srcjars and '
           'annotation processor output for codesearch to find.')
  parser.add_option(
      '--bootclasspath',
      action='append',
      default=[],
      help='Boot classpath for javac. If this is specified multiple times, '
      'they will all be appended to construct the classpath.')
  parser.add_option(
      '--java-version',
      help='Java language version to use in -source and -target args to javac.')
  parser.add_option('--classpath', action='append', help='Classpath to use.')
  parser.add_option(
      '--processors',
      action='append',
      help='GN list of annotation processor main classes.')
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
      '--provider-configuration',
      dest='provider_configurations',
      action='append',
      help='File to specify a service provider. Will be included '
           'in the jar under META-INF/services.')
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
  parser.add_option('--jar-path', help='Jar output path.')
  parser.add_option(
      '--javac-arg',
      action='append',
      default=[],
      help='Additional arguments to pass to javac.')

  options, args = parser.parse_args(argv)
  build_utils.CheckOptions(options, parser, required=('jar_path',))

  options.bootclasspath = build_utils.ParseGnList(options.bootclasspath)
  options.classpath = build_utils.ParseGnList(options.classpath)
  options.processorpath = build_utils.ParseGnList(options.processorpath)
  options.processors = build_utils.ParseGnList(options.processors)
  options.java_srcjars = build_utils.ParseGnList(options.java_srcjars)
  options.jar_info_exclude_globs = build_utils.ParseGnList(
      options.jar_info_exclude_globs)

  additional_jar_files = []
  for arg in options.additional_jar_files or []:
    filepath, jar_filepath = arg.split(':')
    additional_jar_files.append((filepath, jar_filepath))
  options.additional_jar_files = additional_jar_files

  java_files = []
  for arg in args:
    # Interpret a path prefixed with @ as a file containing a list of sources.
    if arg.startswith('@'):
      java_files.extend(build_utils.ReadSourcesList(arg[1:]))
    else:
      java_files.append(arg)

  return options, java_files


def main(argv):
  logging.basicConfig(
      level=logging.INFO if os.environ.get('JAVAC_DEBUG') else logging.WARNING,
      format='%(levelname).1s %(relativeCreated)6d %(message)s')
  colorama.init()

  argv = build_utils.ExpandFileArgs(argv)
  options, java_files = _ParseOptions(argv)

  # Until we add a version of javac via DEPS, use errorprone with all checks
  # disabled rather than javac. This ensures builds are reproducible.
  # https://crbug.com/693079
  # As of Jan 2019, on a z920, compiling chrome_java times:
  # * With javac: 17 seconds
  # * With errorprone (checks disabled): 20 seconds
  # * With errorprone (checks enabled): 30 seconds
  javac_path = build_utils.JAVA_PATH + 'c'

  javac_cmd = [
      javac_path,
      '-g',
      # Chromium only allows UTF8 source files.  Being explicit avoids
      # javac pulling a default encoding from the user's environment.
      '-encoding',
      'UTF-8',
      # Prevent compiler from compiling .java files not listed as inputs.
      # See: http://blog.ltgt.net/most-build-tools-misuse-javac/
      '-sourcepath',
      ':',
  ]

  if options.enable_errorprone:
    errorprone_flags = ['-Xplugin:ErrorProne']
    for warning in ERRORPRONE_WARNINGS_TO_TURN_OFF:
      errorprone_flags.append('-Xep:{}:OFF'.format(warning))
    for warning in ERRORPRONE_WARNINGS_TO_ERROR:
      errorprone_flags.append('-Xep:{}:ERROR'.format(warning))
    javac_cmd += ['-XDcompilePolicy=simple', ' '.join(errorprone_flags)]

  if options.java_version:
    javac_cmd.extend([
      '-source', options.java_version,
      '-target', options.java_version,
    ])
  if options.java_version == '1.8':
    # Android's boot jar doesn't contain all java 8 classes.
    options.bootclasspath.append(build_utils.RT_JAR_PATH)

  if options.chromium_code:
    javac_cmd.extend(['-Werror'])
  else:
    # XDignore.symbol.file makes javac compile against rt.jar instead of
    # ct.sym. This means that using a java internal package/class will not
    # trigger a compile warning or error.
    javac_cmd.extend(['-XDignore.symbol.file'])

  if options.processors:
    javac_cmd.extend(['-processor', ','.join(options.processors)])

  if options.bootclasspath:
    javac_cmd.extend(['-bootclasspath', ':'.join(options.bootclasspath)])

  if options.processorpath:
    javac_cmd.extend(['-processorpath', ':'.join(options.processorpath)])
  if options.processor_args:
    for arg in options.processor_args:
      javac_cmd.extend(['-A%s' % arg])

  javac_cmd.extend(options.javac_arg)

  classpath_inputs = (
      options.bootclasspath + options.classpath + options.processorpath)

  # GN already knows of java_files, so listing them just make things worse when
  # they change.
  depfile_deps = [javac_path] + classpath_inputs + options.java_srcjars
  input_paths = depfile_deps + java_files
  input_paths += [x[0] for x in options.additional_jar_files]

  output_paths = [
      options.jar_path,
      options.jar_path + '.info',
  ]

  input_strings = javac_cmd + options.classpath + java_files
  if options.jar_info_exclude_globs:
    input_strings.append(options.jar_info_exclude_globs)
  build_utils.CallAndWriteDepfileIfStale(
      lambda: _OnStaleMd5(options, javac_cmd, java_files, options.classpath),
      options,
      depfile_deps=depfile_deps,
      input_paths=input_paths,
      input_strings=input_strings,
      output_paths=output_paths)
  logging.info('Script complete: %s', __file__)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
