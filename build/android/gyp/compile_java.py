#!/usr/bin/env python3
#
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import concurrent.futures
import functools
import itertools
import logging
import argparse
import os
import pathlib
import platform
import re
import shlex
import shutil
import subprocess
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

_JAVAC_EXTRACTOR = os.path.join(
    build_utils.DIR_SOURCE_ROOT,
    'third_party',
    'android_prebuilts',
    'build_tools',
    'common',
    'framework',
    'javac_extractor.jar',
)

# A GraalVM native-image of javac. Exists only for linux-x64 hosts.
_JAVAC_GRAALVM_PATH = os.path.join(
    build_utils.JAVA_HOME, 'bin', 'javac-graalvm'
)

# Large compiles are split across multiple javac processes. Each process has
# a fixed cost (mostly loading the classpath), so this is a balance between
# wall time and CPU time. More than 8 shards is only slightly faster on an idle
# machine, and no faster during full builds.
_JAVAC_SHARD_TARGET_SIZE = 2 * 1024 * 1024
_MAX_JAVAC_SHARDS = 8
# Targets with fewer files than this are not sharded, which avoids stat()ing
# files of small targets. It is the number of files needed to reach the
# smallest total size that round()s to 2 shards, assuming an average file size
# of at most 256KB (the largest averages are from generated protos).
_MIN_FILES_TO_SHARD = (_JAVAC_SHARD_TARGET_SIZE * 3 // 2) // (256 * 1024)

# These warnings cannot be suppressed even for third party code. Deprecation
# warnings especially do not help since we must support older android version.
_OUTPUT_FILTER_RE = re.compile(
    r'^Note: .* uses? or overrides? a deprecated API.*\n|'
    r'^Note: .* uses? unchecked or unsafe operations.*\n|'
    r'^Note: Recompile with -Xlint:.* for details.*\n',
    re.MULTILINE,
)


def ProcessJavacOutput(output, target_name):
    output = _OUTPUT_FILTER_RE.sub('', output)
    output = build_utils.FilterReflectiveAccessJavaWarnings(output)

    # Warning currently cannot be silenced via javac flag.
    if 'Unsafe is internal proprietary API' in output:
        # Example:
        # HiddenApiBypass.java:69: warning: Unsafe is internal proprietary API and
        # may be removed in a future release
        # import sun.misc.Unsafe;
        #                 ^
        output = re.sub(
            r'.*?Unsafe is internal proprietary API[\s\S]*?\^\n', '', output
        )
        output = re.sub(r'\d+ warnings?\n', '', output)

    return javac_output_processor.Process(target_name, output)


@functools.cache
def _GetFileSize(path):
    return os.path.getsize(path)


def _ComputeNumShards(java_files):
    """Returns how many javac processes to split |java_files| across.

    Must depend only on the inputs (and not on e.g. the number of CPUs), since
    the shard boundaries affect the output (javac adds bridge methods when a
    superclass comes from a header jar rather than from source). Partial javac
    already compiles against the header jar, so has the same effect.
    """
    if len(java_files) < _MIN_FILES_TO_SHARD:
        return 1
    total_size = sum(_GetFileSize(p) for p in java_files)
    return max(
        1, min(_MAX_JAVAC_SHARDS, round(total_size / _JAVAC_SHARD_TARGET_SIZE))
    )


def _ShardJavaFiles(java_files, num_shards):
    """Splits |java_files| into |num_shards| lists of similar total size.

    Files are kept in sorted order so that files within the same package
    (which are most likely to reference one another) end up in the same shard.
    """
    sizes = [(p, _GetFileSize(p)) for p in sorted(java_files)]
    total_size = sum(s for _, s in sizes) or 1
    shards = [[] for _ in range(num_shards)]
    cumulative_size = 0
    for path, size in sizes:
        # Assign each file to the shard that contains its midpoint.
        idx = (cumulative_size + size // 2) * num_shards // total_size
        shards[min(idx, num_shards - 1)].append(path)
        cumulative_size += size
    return [s for s in shards if s]


def _RunShardedJavac(
    cmd,
    java_files,
    num_shards,
    java_files_rsp_path,
    options,
    output_filter,
    before_join_callback,
):
    """Runs |cmd| as multiple concurrent javac processes.

    Each process compiles a subset of |java_files|, and finds classes from the
    other subsets via the target's header jar (which is on the classpath). All
    processes write to the same output directory.
    """
    assert cmd[-1] == '@' + java_files_rsp_path
    shard_cmds = []
    for i, shard in enumerate(_ShardJavaFiles(java_files, num_shards)):
        shard_rsp_path = f'{java_files_rsp_path}.shard{i}'
        with open(shard_rsp_path, 'w', encoding='utf-8') as f:
            f.write(' '.join(shard))
        shard_cmds.append(cmd[:-1] + ['@' + shard_rsp_path])

    # Concurrency does not affect the output, so can depend on the machine.
    max_workers = min(len(shard_cmds), os.cpu_count() or 1)
    logging.info(
        'Running javac as %d shards (%d at a time)',
        len(shard_cmds),
        max_workers,
    )

    def run_shard(shard_cmd):
        return subprocess.run(
            shard_cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            encoding='utf-8',
            check=False,
        )

    with concurrent.futures.ThreadPoolExecutor(max_workers) as executor:
        futures = [executor.submit(run_shard, c) for c in shard_cmds]
        if before_join_callback:
            before_join_callback()
        results = [f.result() for f in futures]

    # Combine output in shard order so that it is deterministic. Filter it all
    # at once so that hints (e.g. for missing deps) are not repeated per shard.
    stdout = output_filter(''.join(r.stdout for r in results))
    stderr = output_filter(''.join(r.stderr for r in results))
    for shard_cmd, result in zip(shard_cmds, results):
        if result.returncode:
            raise build_utils.CalledProcessError(
                os.getcwd(), shard_cmd, stdout + stderr
            )

    build_utils.HandleCommandOutput(
        cmd,
        stdout,
        stderr,
        print_stdout=options.chromium_code,
        fail_on_output=options.warnings_as_errors,
    )


def CreateJarFile(
    jar_path,
    classes_dir,
    services_map=None,
    additional_jar_files=None,
    extra_classes_jar=None,
    filter_func=None,
):
    """Zips files from compilation into a single jar."""
    logging.info('Start creating jar file: %s', jar_path)
    with action_helpers.atomic_output(jar_path) as f:
        with zipfile.ZipFile(f.name, 'w') as z:
            path_transform = None
            if filter_func:
                path_transform = lambda p: p if filter_func(p) else None  # noqa: E731
            zip_helpers.zip_directory(
                z, classes_dir, path_transform=path_transform
            )
            if services_map:
                for service_class, impl_classes in sorted(services_map.items()):
                    zip_path = 'META-INF/services/' + service_class
                    data = ''.join(f'{x}\n' for x in sorted(impl_classes))
                    zip_helpers.add_to_zip_hermetic(z, zip_path, data=data)

            # Used to merge in kotlinc results.
            if extra_classes_jar:

                def path_transform2(zip_path):
                    if path_transform and not path_transform(zip_path):
                        return None
                    if zip_path.endswith('.class'):
                        return zip_path
                    return None

                zip_helpers.merge_zips(
                    z, [extra_classes_jar], path_transform=path_transform2
                )

            if additional_jar_files:
                for src_path, zip_path in additional_jar_files:
                    zip_helpers.add_to_zip_hermetic(
                        z, zip_path, src_path=src_path
                    )

    logging.info('Completed jar file: %s', jar_path)


# Java lines end in semicolon, whereas Kotlin lines do not.
_PACKAGE_RE = re.compile(r'^package\s+(.*?)(;|\s*$)', flags=re.MULTILINE)

_SERVICE_IMPL_RE = re.compile(
    r'^([\t ]*)@ServiceImpl\(\s*(.+?)\.class\)(.*?)\sclass\s+(\w+)',
    flags=re.MULTILINE | re.DOTALL,
)

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
    flags=re.MULTILINE,
)


def ParseJavaSource(data, services_map, path=None):
    """This should support both Java and Kotlin files."""
    package_name = ''
    if m := _PACKAGE_RE.search(data):
        package_name = m.group(1)

    class_names = _TOP_LEVEL_CLASSES_RE.findall(data)

    # Very rare, so worth an upfront check.
    if '@ServiceImpl' in data:
        for (
            indent,
            service_class,
            modifiers,
            impl_class,
        ) in _SERVICE_IMPL_RE.findall(data):
            if 'public' not in modifiers:
                raise Exception(
                    f'@ServiceImpl can be used only on public classes '
                    f'(when parsing {path})'
                )
            # Assume indent means nested class that is one level deep.
            if indent:
                impl_class = f'{class_names[0]}${impl_class}'
            else:
                assert class_names[0] == impl_class

            # Parse imports to resolve the class.
            dot_idx = service_class.find('.')
            # Handle @ServiceImpl(OuterClass.InnerClass.class)
            outer_class = (
                service_class if dot_idx == -1 else service_class[:dot_idx]
            )

            if m := re.search(
                r'^import\s+([\w\.]*\.' + outer_class + r')[;\s]',
                data,
                flags=re.MULTILINE,
            ):
                service_class = m.group(1) + service_class[len(outer_class) :]
            else:
                service_class = f'{package_name}.{service_class}'

            # Convert OuterClass.InnerClass -> OuterClass$InnerClass.
            for m in list(re.finditer(r'\.[A-Z]', service_class))[1:]:
                idx = m.start()
                service_class = (
                    service_class[:idx] + '$' + service_class[idx + 1 :]
                )

            services_map[service_class].append(f'{package_name}.{impl_class}')

    return package_name, class_names


def _CreateGlobFilter(exclude_globs, include_globs):

    def func(zip_path):
        if include_globs and not build_utils.MatchesGlob(
            zip_path, include_globs
        ):
            return False
        return not build_utils.MatchesGlob(zip_path, exclude_globs)

    return func


class _MetadataParser:
    def __init__(self, chromium_code, filter_func):
        self._chromium_code = chromium_code
        self._filter_func = filter_func
        # Map of .java path -> .srcjar/nested/path.java.
        self._srcjar_files = {}
        # Map of @ServiceImpl class -> impl class
        self.services_map = collections.defaultdict(list)

    def AddSrcJarSources(self, srcjar_path, extracted_paths, parent_dir):
        for path in extracted_paths:
            # We want the path inside the srcjar so the viewer can have a tree
            # structure.
            self._srcjar_files[path] = '{}/{}'.format(
                srcjar_path, os.path.relpath(path, parent_dir)
            )

    def _CheckPathMatchesClassName(self, source_file, package_name, class_name):
        if source_file.endswith('.java'):
            parts = package_name.split('.') + [class_name + '.java']
        else:
            parts = package_name.split('.') + [class_name + '.kt']
        expected_suffix = os.path.sep.join(parts)
        if not source_file.endswith(expected_suffix):
            raise Exception(
                (
                    'Source package+class name do not match its path.\n'
                    'Actual path: %s\nExpected path: %s'
                )
                % (source_file, expected_suffix)
            )

    def _ProcessInfo(self, java_file, package_name, class_names, source):
        for class_name in class_names:
            yield '{}.{}'.format(package_name, class_name)
            # Skip aidl srcjars since they don't indent code correctly.
            if '_aidl.srcjar' in source:
                continue
            assert not self._chromium_code or len(class_names) == 1, (
                'Chromium java files must only have one class: {} found: {}'.format(
                    source, class_names
                )
            )
            if self._chromium_code:
                # This check is not necessary but nice to check this somewhere.
                self._CheckPathMatchesClassName(
                    java_file, package_name, class_names[0]
                )

    def _ShouldIncludeInJarInfo(self, fully_qualified_name):
        name_as_class_glob = fully_qualified_name.replace('.', '/') + '.class'
        return self._filter_func(name_as_class_glob)

    def ParseAndWriteInfoFile(self, output_path, java_files, kt_files=None):
        """Writes a .jar.info file.

        Maps fully qualified names for classes to either the java file that they
        are defined in or the path of the srcjar that they came from.
        """
        logging.info('Collecting info file entries')
        entries = {}
        for path in itertools.chain(java_files, kt_files or []):
            data = pathlib.Path(path).read_text(encoding='utf-8')
            package_name, class_names = ParseJavaSource(
                data, self.services_map, path=path
            )
            source = self._srcjar_files.get(path, path)
            for fully_qualified_name in self._ProcessInfo(
                path, package_name, class_names, source
            ):
                if self._ShouldIncludeInJarInfo(fully_qualified_name):
                    entries[fully_qualified_name] = path

        logging.info('Writing info file: %s', output_path)
        with action_helpers.atomic_output(output_path, encoding='utf-8') as f:
            jar_info_utils.WriteJarInfoFile(f, entries, self._srcjar_files)
        logging.info('Completed info file: %s', output_path)


def _OnStaleMd5(
    changes,
    options,
    javac_cmd,
    javac_args,
    java_files,
    kt_files,
    use_errorprone=False,
):
    logging.info('Starting _OnStaleMd5')

    if options.enable_kythe_annotations:
        # Kythe requires those env variables to be set and compile_java.py does the
        # same
        if not os.environ.get('KYTHE_ROOT_DIRECTORY') or not os.environ.get(
            'KYTHE_OUTPUT_DIRECTORY'
        ):
            raise Exception(
                '--enable-kythe-annotations requires '
                'KYTHE_ROOT_DIRECTORY and KYTHE_OUTPUT_DIRECTORY '
                'environment variables to be set.'
            )
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
            _RunCompiler(
                changes,
                options,
                javac_extractor_cmd + javac_args,
                java_files,
                options.jar_path + '.javac_extractor',
                enable_partial_javac=False,
            )
        except build_utils.CalledProcessError as e:
            # Having no index for particular target is better than failing entire
            # codesearch. Log and error and move on.
            logging.error('Could not generate kzip: %s', e)

    intermediates_out_dir = None
    jar_info_path = None
    if not use_errorprone:
        # Delete any stale files in the generated directory. The purpose of
        # options.generated_dir is for codesearch and Android Studio.
        shutil.rmtree(options.generated_dir, True)
        intermediates_out_dir = options.generated_dir

        # Write .info file only for the main javac invocation (no need to do it
        # when running Error Prone.
        jar_info_path = options.jar_path + '.info'

    # Sharding with the JVM javac is not worthwhile since each process needs to
    # warm up its JIT. Annotation processors might need to see all sources.
    allow_sharding = (
        javac_cmd[0] == _JAVAC_GRAALVM_PATH
        and not options.processorpath
        and options.header_jar in options.classpath
    )

    # Compiles with Error Prone take twice as long to run as pure javac. Thus GN
    # rules run both in parallel, with Error Prone only used for checks.
    try:
        _RunCompiler(
            changes,
            options,
            javac_cmd + javac_args,
            java_files,
            options.jar_path,
            use_errorprone=use_errorprone,
            kt_files=kt_files,
            jar_info_path=jar_info_path,
            intermediates_out_dir=intermediates_out_dir,
            enable_partial_javac=True,
            allow_sharding=allow_sharding,
        )
    except build_utils.CalledProcessError as e:
        # Do not output stacktrace as it takes up space on gerrit UI, forcing
        # you to click though to find the actual compilation error. It's never
        # interesting to see the Python stacktrace for a Java compilation error.
        sys.stderr.write(e.output)
        sys.exit(1)

    logging.info('Completed all steps in _OnStaleMd5')


def _RunCompiler(
    changes,
    options,
    javac_cmd,
    java_files,
    jar_path,
    use_errorprone=False,
    kt_files=None,
    jar_info_path=None,
    intermediates_out_dir=None,
    enable_partial_javac=False,
    allow_sharding=False,
):
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
      allow_sharding: Allows splitting large compiles across multiple javac
          processes. Requires the target's header jar to be on the classpath.
    """
    logging.info('Starting _RunCompiler')

    java_files = java_files.copy()
    java_srcjars = options.java_srcjars
    parse_java_files = jar_info_path is not None

    # Use jar_path's directory to ensure paths are relative (needed for rbe).
    temp_dir = jar_path + '.staging'
    build_utils.DeleteDirectory(temp_dir)
    os.makedirs(temp_dir)
    filter_func = _CreateGlobFilter(
        options.jar_exclude_globs, options.jar_include_globs
    )
    metadata_parser = _MetadataParser(options.chromium_code, filter_func)
    try:
        classes_dir = os.path.join(temp_dir, 'classes')

        if java_files:
            os.makedirs(classes_dir)

            if enable_partial_javac and changes:
                all_changed_paths_are_java = all(
                    p.endswith(".java") for p in changes.IterChangedPaths()
                )
                if (
                    all_changed_paths_are_java
                    and not changes.HasStringChanges()
                    and os.path.exists(jar_path)
                    and (jar_info_path is None or os.path.exists(jar_info_path))
                    and (
                        not options.filtered_jar
                        or os.path.exists(options.filtered_jar)
                    )
                ):
                    # Log message is used by tests to determine whether partial javac
                    # optimization was used.
                    logging.info(
                        'Using partial javac optimization for %s compile',
                        jar_path,
                    )

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
                    if options.filtered_jar:
                        build_utils.ExtractAll(
                            options.filtered_jar, classes_dir
                        )

        if intermediates_out_dir is None:
            intermediates_out_dir = temp_dir

        input_srcjars_dir = os.path.join(intermediates_out_dir, 'input_srcjars')

        if java_srcjars:
            logging.info('Extracting srcjars to %s', input_srcjars_dir)
            build_utils.MakeDirectory(input_srcjars_dir)
            for srcjar in options.java_srcjars:
                extracted_files = build_utils.ExtractAll(
                    srcjar,
                    no_clobber=True,
                    path=input_srcjars_dir,
                    pattern='*.java',
                )
                java_files.extend(extracted_files)
                if parse_java_files:
                    metadata_parser.AddSrcJarSources(
                        srcjar, extracted_files, input_srcjars_dir
                    )
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
            with open(java_files_rsp_path, 'w', encoding='utf-8') as f:
                f.write(' '.join(java_files))
            cmd += ['@' + java_files_rsp_path]

            process_javac_output_partial = functools.partial(
                ProcessJavacOutput, target_name=options.target_name
            )

            logging.debug('Build command %s', cmd)
            start = time.time()
            before_join_callback = None
            if parse_java_files:
                before_join_callback = lambda: (  # noqa: E731
                    metadata_parser.ParseAndWriteInfoFile(
                        jar_info_path, java_files, kt_files
                    )
                )

            if options.print_javac_command_line:
                print(shlex.join(cmd))
                return

            num_shards = 1
            if allow_sharding:
                num_shards = _ComputeNumShards(java_files)
            if num_shards > 1:
                _RunShardedJavac(
                    cmd,
                    java_files,
                    num_shards,
                    java_files_rsp_path,
                    options,
                    process_javac_output_partial,
                    before_join_callback,
                )
            else:
                build_utils.CheckOutput(
                    cmd,
                    print_stdout=options.chromium_code,
                    stdout_filter=process_javac_output_partial,
                    stderr_filter=process_javac_output_partial,
                    fail_on_output=options.warnings_as_errors,
                    before_join_callback=before_join_callback,
                )
            end = time.time() - start
            logging.info('Java compilation took %ss', end)
        elif parse_java_files:
            if options.print_javac_command_line:
                raise Exception(
                    'need java files for --print-javac-command-line.'
                )
            metadata_parser.ParseAndWriteInfoFile(
                jar_info_path, java_files, kt_files
            )

        if use_errorprone:
            # There is no jar file when running errorprone and jar_path is actually
            # just the stamp file for that target.
            server_utils.MaybeTouch(jar_path)
        else:
            CreateJarFile(
                jar_path,
                classes_dir,
                metadata_parser.services_map,
                options.additional_jar_files,
                options.kotlin_jar_path,
                filter_func=filter_func,
            )
            if options.filtered_jar:
                CreateJarFile(
                    options.filtered_jar,
                    classes_dir,
                    filter_func=lambda p: not filter_func(p),
                )

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
            # Use ignore_errors=True so that any missing file during cleanup
            # does not raise FileNotFoundError and mask the real javac failure.
            shutil.rmtree(temp_dir, ignore_errors=True)


def _ParseOptions(argv):
    parser = argparse.ArgumentParser()
    action_helpers.add_depfile_arg(parser)

    parser.add_argument('--target-name', help='Fully qualified GN target name.')
    parser.add_argument(
        '--java-srcjars',
        action='append',
        default=[],
        help='List of srcjars to include in compilation.',
    )
    parser.add_argument(
        '--generated-dir',
        help='Subdirectory within target_gen_dir to place extracted srcjars and '
        'annotation processor output for codesearch to find.',
    )
    parser.add_argument(
        '--classpath', action='append', help='Classpath to use.'
    )
    parser.add_argument(
        '--processorpath',
        action='append',
        help='GN list of jars that comprise the classpath used for Annotation '
        'Processors.',
    )
    parser.add_argument(
        '--processor-arg',
        dest='processor_args',
        action='append',
        help='key=value arguments for the annotation processors.',
    )
    parser.add_argument(
        '--additional-jar-file',
        dest='additional_jar_files',
        action='append',
        help='Additional files to package into jar. By default, only Java .class '
        'files are packaged into the jar. Files should be specified in '
        'format <filename>:<path to be placed in jar>.',
    )
    parser.add_argument(
        '--jar-exclude-globs',
        help='GN list of exclude globs to filter from the output .jar.',
    )
    parser.add_argument(
        '--jar-include-globs',
        help='GN list of inlclude globs to filter from the output .jar.',
    )
    parser.add_argument(
        '--filtered-jar', help='Output filtered .class files here'
    )
    parser.add_argument(
        '--chromium-code',
        action='store_true',
        help='Whether code being compiled should be built with stricter '
        'warnings for chromium code.',
    )
    parser.add_argument(
        '--warnings-as-errors',
        action='store_true',
        help='Treat all warnings as errors.',
    )
    parser.add_argument('--jar-path', required=True, help='Jar output path.')
    parser.add_argument(
        '--javac-arg',
        action='append',
        default=[],
        help='Additional arguments to pass to javac.',
    )
    parser.add_argument(
        '--print-javac-command-line',
        action='store_true',
        help='Just show javac command line (for ide_query).',
    )
    parser.add_argument(
        '--enable-kythe-annotations',
        action='store_true',
        help='Enable generation of Kythe kzip, used for codesearch. Ensure '
        'proper environment variables are set before using this flag.',
    )
    parser.add_argument(
        '--header-jar',
        help='This is the header jar for the current target that contains '
        'META-INF/services/* files to be included in the output jar.',
    )
    parser.add_argument(
        '--kotlin-jar-path',
        help='Kotlin jar to be merged into the output jar. This contains the '
        ".class files from this target's .kt files.",
    )
    parser.add_argument('sources', nargs='*')

    options = parser.parse_args(argv)

    options.classpath = action_helpers.parse_gn_list(options.classpath)
    options.processorpath = action_helpers.parse_gn_list(options.processorpath)
    options.java_srcjars = action_helpers.parse_gn_list(options.java_srcjars)
    options.jar_exclude_globs = action_helpers.parse_gn_list(
        options.jar_exclude_globs
    )
    options.jar_include_globs = action_helpers.parse_gn_list(
        options.jar_include_globs
    )

    additional_jar_files = []
    for arg in options.additional_jar_files or []:
        filepath, jar_filepath = arg.split(':')
        additional_jar_files.append((filepath, jar_filepath))
    options.additional_jar_files = additional_jar_files

    files = []
    for arg in options.sources:
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


def main(
    argv, extra_javac_args=None, use_errorprone=False, write_depfile_only=False
):
    build_utils.InitLogging('JAVAC_DEBUG')
    argv = build_utils.ExpandFileArgs(argv)
    options, java_files, kt_files = _ParseOptions(argv)

    # The native-image javac does not support plugins (Error Prone), and
    # --print-javac-command-line is used by tools that expect a JVM javac.
    use_javac_graalvm = (
        sys.platform == 'linux'
        and platform.machine() == 'x86_64'
        and not use_errorprone
        and not options.print_javac_command_line
    )
    if use_javac_graalvm:
        javac_cmd = [_JAVAC_GRAALVM_PATH]
    else:
        javac_cmd = [
            build_utils.JAVAC_PATH,
            # https://crbug.com/1441023
            '-J-XX:+PerfDisableSharedMem',
        ]

    javac_args = [
        '-g',
        # Required for Error Prone's /* paramName= */ check.
        '-parameters',
        # Jacoco does not currently support a higher value.
        '--release',
        # Keep consistent with javac_cmd in //build/android/gyp/turbine.py and
        # -jvm-target in //build/android/gyp/compile_kt.py.
        # LINT.IfChange
        '25',
        # LINT.ThenChange(
        #     //build/android/gyp/compile_kt.py,
        #     //build/android/gyp/turbine.py,
        #     //components/cronet/gn2bp/templates/Android.extras.bp.template
        # )
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
        # Disable all annotation processors (we run them via Turbine).
        '-proc:none',
    ]

    # Apply -Xlint:unchecked to Chromium-owned code only. Third-party code
    # (chromium_code = false) is exempt.
    if options.chromium_code:
        javac_args += ['-Xlint:unchecked']

    if extra_javac_args:
        javac_args.extend(extra_javac_args)

    if options.processorpath:
        javac_args.extend(['-processorpath', ':'.join(options.processorpath)])
    if options.processor_args:
        for arg in options.processor_args:
            javac_args.extend(['-A%s' % arg])

    javac_args.extend(options.javac_arg)

    do_it = lambda changes: _OnStaleMd5(  # noqa: E731
        changes,
        options,
        javac_cmd,
        javac_args,
        java_files,
        kt_files,
        use_errorprone=use_errorprone,
    )

    if options.print_javac_command_line:
        if options.java_srcjars:
            raise Exception(
                '--print-javac-command-line does not work with --java-srcjars'
            )
        do_it(None)
        return 0

    classpath_jars = options.classpath + options.processorpath

    # javac does not complain about missing classpath files. Missing files can
    # result in misleading compile errors though, so do an upfront check.
    missing_jars = [p for p in classpath_jars if not os.path.exists(p)]
    if missing_jars:
        sys.stderr.write('One or more classpath .jar files does not exist:\n')
        sys.stderr.write('\n'.join(missing_jars) + '\n')
        sys.exit(1)

    output_paths = [options.jar_path]
    if not use_errorprone:
        jar_info_path = options.jar_path + '.info'
        output_paths.append(jar_info_path)
    if options.filtered_jar:
        output_paths.append(options.filtered_jar)

    # Incremental build optimization doesn't work for ErrorProne. Skip md5 check.
    if write_depfile_only:
        action_helpers.write_depfile(
            options.depfile, output_paths[0], classpath_jars
        )
    elif use_errorprone:
        do_it(None)
        action_helpers.write_depfile(
            options.depfile, output_paths[0], classpath_jars
        )
    else:
        # Files that are already inputs in GN should go in input_paths.
        input_paths = (
            [javac_cmd[0]]
            + classpath_jars
            + options.java_srcjars
            + java_files
            + kt_files
        )
        if options.header_jar:
            input_paths.append(options.header_jar)
        input_paths += [x[0] for x in options.additional_jar_files]

        input_strings = (
            javac_cmd
            + javac_args
            + options.classpath
            + java_files
            + kt_files
            + [
                options.warnings_as_errors,
                options.jar_exclude_globs,
                options.jar_include_globs,
            ]
        )

        # Use md5_check for |pass_changes| feature.
        md5_check.CallAndWriteDepfileIfStale(
            do_it,
            options,
            depfile_deps=classpath_jars,
            input_paths=input_paths,
            input_strings=input_strings,
            output_paths=output_paths,
            pass_changes=True,
        )
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
