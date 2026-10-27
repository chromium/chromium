#!/usr/bin/env python3
#
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import collections
import concurrent.futures
import logging
import os
import platform
import re
import shutil
import shlex
import sys
import tempfile
import zipfile

from util import build_utils
from util import md5_check
from util import zipbuilder
import action_helpers  # build_utils adds //build to sys.path.
import zip_helpers


_DEX_XMX = '3G'  # Increase this when __final_dex OOMs.
_CUSTOM_D8_BINARY_PATH = os.path.join(
    build_utils.DIR_SOURCE_ROOT, 'third_party', 'r8', 'cipd', 'bin', 'custom_d8'
)
# For --file-per-class-file invocations (_CreateIntermediateDexJar), shard
# inputs across concurrent GraalVM processes targeting ~1.5 MB per shard to
# avoid GraalVM Serial GC pauses (e.g. 20.4 MB chrome_java: 5.60s JVM -> 1.18s).
_GRAALVM_SHARD_TARGET_SIZE = int(1.5 * 1024 * 1024)
# Start with a big heap to avoid frequent GCs. Without this, the JVM is faster
# for large inputs (e.g. 97.6MB dex merge: 16.2s -> 11.3s, vs 13.1s for the
# JVM). Not used for shards, which are small enough to not benefit, and whose
# RSS would increase (129MB -> 423MB per shard).
_GRAALVM_XMS_FLAG = '-Xms1g'

DEFAULT_IGNORE_WARNINGS = (
    # Warning: Running R8 version main (build engineering), which cannot be
    # represented as a semantic version. Using an artificial version newer than
    # any known version for selecting Proguard configurations embedded under
    # META-INF/. This means that all rules with a '-upto-' qualifier will be
    # excluded and all rules with a -from- qualifier will be included.
    r'Running R8 version main',
    # https://issuetracker.google.com/327611582
    r'The companion object Companion could not be found',
    # https://crbug.com/408280256
    r'MethodHandle.invoke',
    # JDK update warnings
    r'A terminally deprecated method in sun.misc.Unsafe',
    r'sun.misc.Unsafe::.* has been called',
    r'sun.misc.Unsafe::.* will be removed',
    r'Please consider reporting this to the maintainers of',
)

_MERGE_SERVICE_ENTRIES = (
    # Uses ServiceLoader to find all implementing classes, so multiple are
    # expected.
    'META-INF/services/androidx.appsearch.app.AppSearchDocumentClassMap',
    'META-INF/services/kotlinx.coroutines.CoroutineExceptionHandler',
    'META-INF/services/kotlinx.coroutines.internal.MainDispatcherFactory',
    'META-INF/services/org.chromium.base.test.util.LeakCanaryChecker$LeakCanaryConfigProvider',
    'META-INF/services/org.chromium.base.test.BaseJUnit4ClassRunner$ClassCleanupHook',
    'META-INF/services/org.chromium.base.test.BaseJUnit4ClassRunner$AfterCleanupCheck',
    'META-INF/services/org.chromium.on_device_model.AiCoreFactory',
)

_IGNORE_SERVICE_ENTRIES = (
    # ServiceLoader call is used only for ProtoBuf full (non-lite).
    # BaseGeneratedExtensionRegistryLite$Loader conflicts with
    # ChromeGeneratedExtensionRegistryLite$Loader.
    'META-INF/services/com.google.protobuf.GeneratedExtensionRegistryLoader',
)

INTERFACE_DESUGARING_WARNINGS = (r'default or static interface methods',)

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
        help='GN-list of .jars with .class files.',
    )
    parser.add_argument(
        '--class-inputs-filearg',
        action='append',
        help='GN-list of .jars with .class files (added to depfile).',
    )
    parser.add_argument(
        '--dex-inputs',
        action='append',
        help='GN-list of .jars with .dex files.',
    )
    parser.add_argument(
        '--dex-inputs-filearg',
        action='append',
        help='GN-list of .jars with .dex files (added to depfile).',
    )
    parser.add_argument(
        '--incremental-dir',
        help='Path of directory to put intermediate dex files.',
    )
    parser.add_argument(
        '--intermediate',
        action='store_true',
        help='Dex must still be merged before being used.',
    )
    parser.add_argument('--r8-jar-path', required=True, help='Path to R8 jar.')
    parser.add_argument(
        '--skip-custom-d8',
        action='store_true',
        help='When rebuilding the CustomD8 jar, this may be '
        'necessary to avoid incompatibility with the new r8 '
        'jar.',
    )
    parser.add_argument(
        '--custom-d8-jar-path',
        required=True,
        help='Path to our customized d8 jar.',
    )
    parser.add_argument(
        '--desugar-dependencies', help='Path to store desugar dependencies.'
    )
    parser.add_argument('--desugar', action='store_true')
    parser.add_argument(
        '--bootclasspath',
        action='append',
        help='GN-list of bootclasspath. Needed for --desugar',
    )
    parser.add_argument(
        '--show-desugar-default-interface-warnings',
        action='store_true',
        help='Enable desugaring warnings.',
    )
    parser.add_argument(
        '--classpath',
        action='append',
        help='GN-list of full classpath. Needed for --desugar',
    )
    parser.add_argument(
        '--release', action='store_true', help='Run D8 in release mode.'
    )
    parser.add_argument(
        '--min-api', help='Minimum Android API level compatibility.'
    )
    parser.add_argument(
        '--force-enable-assertions',
        action='store_true',
        help='Forcefully enable javac generated assertion code.',
    )
    parser.add_argument(
        '--assertion-handler',
        help='The class name of the assertion handler class.',
    )
    parser.add_argument(
        '--warnings-as-errors',
        action='store_true',
        help='Treat all warnings as errors.',
    )
    parser.add_argument(
        '--dump-inputs',
        action='store_true',
        help='Use when filing D8 bugs to capture inputs.'
        ' Stores inputs to d8inputs.zip',
    )
    options = parser.parse_args(args)

    if options.force_enable_assertions and options.assertion_handler:
        parser.error(
            'Cannot use both --force-enable-assertions and --assertion-handler'
        )

    options.class_inputs = action_helpers.parse_gn_list(options.class_inputs)
    options.class_inputs_filearg = action_helpers.parse_gn_list(
        options.class_inputs_filearg
    )
    options.bootclasspath = action_helpers.parse_gn_list(options.bootclasspath)
    options.classpath = action_helpers.parse_gn_list(options.classpath)
    options.dex_inputs = action_helpers.parse_gn_list(options.dex_inputs)
    options.dex_inputs_filearg = action_helpers.parse_gn_list(
        options.dex_inputs_filearg
    )

    return options


def CreateStderrFilter(filters):
    def filter_stderr(output):
        # Set this when debugging R8 output.
        if os.environ.get('R8_SHOW_ALL_OUTPUT', '0') != '0':
            return output

        # All missing definitions are logged as a single warning, but start on a
        # new line like "Missing class ...".
        warnings = re.split(
            r'(?i)^(?=warning|error|missing (?:class|field|method))',
            output,
            flags=re.MULTILINE,
        )
        preamble, *warnings = warnings

        combined_pattern = '|'.join(filters)
        preamble = build_utils.FilterLines(preamble, combined_pattern)

        compiled_re = re.compile(combined_pattern, re.DOTALL)
        warnings = [w for w in warnings if not compiled_re.search(w)]

        return preamble + ''.join(warnings)

    return filter_stderr


def _RunD8(
    dex_cmd,
    input_paths,
    output_path,
    warnings_as_errors,
    show_desugar_default_interface_warnings,
):
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
    with tempfile.NamedTemporaryFile(
        mode='w', delete=not is_debug
    ) as flag_file:
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
            build_utils.CheckOutput(
                dex_cmd,
                print_stdout=is_debug,
                stderr_filter=stderr_filter,
                fail_on_output=(warnings_as_errors and not is_debug),
            )
        except Exception:
            if orig_dex_cmd is not dex_cmd:
                sys.stderr.write(
                    'Full command: ' + shlex.join(orig_dex_cmd) + '\n'
                )
            raise


def _ZipStored(dex_files, output_path, services_map):
    """Creates a .dex.jar with uncompressed (STORED) files.

    Args:
      dex_files: List of dex files.
      output_path: The output file in which to write the zip.
      services_map: map of path->data for META-INF/services
    """
    with zipfile.ZipFile(output_path, 'w') as z:
        for i, dex_file in enumerate(dex_files):
            name = 'classes{}.dex'.format(i + 1 if i > 0 else '')
            zip_helpers.add_to_zip_hermetic(z, name, src_path=dex_file)
        for path, data in sorted(services_map.items()):
            zip_helpers.add_to_zip_hermetic(z, path, data=data)


def _CreateServicesMap(service_jars):
    ret = {}
    origins = {}
    for jar_path in service_jars:
        with zipfile.ZipFile(jar_path, 'r') as z:
            for n in z.namelist():
                if n.startswith('META-INF/services/') and not n.endswith('/'):
                    if n in _IGNORE_SERVICE_ENTRIES:
                        continue
                    old_lines = ret.get(n, '').splitlines()
                    new_lines = z.read(n).decode('utf8').splitlines()
                    old_lines.extend(l for l in new_lines if l not in old_lines)  # noqa: E741
                    data = '\n'.join(old_lines) + '\n'
                    if n in _MERGE_SERVICE_ENTRIES or ret.get(n, data) == data:
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

If this service is meant to allow multiple implementations, update \
_MERGE_SERVICE_ENTRIES in dex.py.
""")
    return ret


def _WriteServiceArgs(services_map, tmp_dir):
    args = []
    for i, (path, data) in enumerate(sorted(services_map.items())):
        svc_file = os.path.join(tmp_dir, f'service_{i}.txt')
        with open(svc_file, 'w', encoding='utf-8', newline='') as f:
            f.write(data)
        args += ['--add-file', f'{path}={svc_file}']
    return args


def _MergeShardJars(shard_jars, output_path, services_map, warnings_as_errors):
    with zipbuilder.ZipBuilder() as zb:
        for sj in shard_jars:
            zb.add_all_from_zip(sj, compress_level=zipbuilder.KEEP_COMPRESSION)
        for path, data in sorted(services_map.items()):
            zb.add_data(path, data, compress_level=0)
        zb.write(output_path, warnings_as_errors=warnings_as_errors)


def _CreateFinalDex(
    d8_inputs,
    output,
    tmp_dir,
    dex_cmd,
    options=None,
    services_map=None,
):
    services_map = services_map or {}
    warnings_as_errors = not options or options.warnings_as_errors
    show_desugar_warnings = bool(
        options and options.show_desugar_default_interface_warnings
    )
    if not output.endswith('.dex') and not (options and options.skip_custom_d8):
        tmp_dex_output = os.path.join(tmp_dir, 'tmp_dex_output.jar')
        extra_args = _WriteServiceArgs(services_map, tmp_dir)
        _RunD8(
            dex_cmd + extra_args,
            d8_inputs,
            tmp_dex_output,
            warnings_as_errors,
            show_desugar_warnings,
        )
        logging.debug('Performed dex merging directly to jar')
    else:
        tmp_dex_output = os.path.join(tmp_dir, 'tmp_dex_output.zip')
        tmp_dex_dir = os.path.join(tmp_dir, 'tmp_dex_dir')
        os.mkdir(tmp_dex_dir)
        _RunD8(
            dex_cmd,
            d8_inputs,
            tmp_dex_dir,
            warnings_as_errors,
            show_desugar_warnings,
        )
        logging.debug('Performed dex merging')
        dex_files = [
            os.path.join(tmp_dex_dir, f) for f in os.listdir(tmp_dex_dir)
        ]
        if output.endswith('.dex'):
            if len(dex_files) > 1:
                raise Exception('%d files created, expected 1' % len(dex_files))
            tmp_dex_output = dex_files[0]
        else:
            _ZipStored(sorted(dex_files), tmp_dex_output, services_map)

    # The dex file is complete and can be moved out of tmp_dir.
    shutil.move(tmp_dex_output, output)


def _ScanClassInputJars(class_inputs):
    """Returns ({jar: [(subpath, size), ...]}, [service_jars])."""
    jar_class_entries = {}
    service_jars = []
    for jar in class_inputs:
        entries = []
        has_services = False
        with zipfile.ZipFile(jar, 'r') as z:
            for info in z.infolist():
                subpath = info.filename
                if _IsClassFile(subpath):
                    entries.append((subpath, info.file_size))
                elif subpath.startswith(
                    'META-INF/services/'
                ) and not subpath.endswith('/'):
                    has_services = True
        jar_class_entries[jar] = entries
        if has_services:
            service_jars.append(jar)
    return jar_class_entries, service_jars


def _ParseDesugarDeps(desugar_dependencies_file):
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
    dependents_from_dependency = collections.defaultdict(set)
    if desugar_dependencies_file and os.path.exists(desugar_dependencies_file):
        with open(desugar_dependencies_file, 'r', encoding='utf-8') as f:
            dependent = None
            for line in f:
                line = line.rstrip()
                if line.startswith('  <-  '):
                    dependency = line[len('  <-  ') :]
                    # Note that this is a reversed mapping from the one in CustomD8.java.
                    dependents_from_dependency[dependency].add(dependent)
                else:
                    dependent = line
    return dependents_from_dependency


def _ComputeRequiredDesugarClasses(
    changes, desugar_dependencies_file, class_inputs, classpath
):
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


def _ClassFileNestPrefix(class_path):
    """Returns the javac nest-host prefix for a .class subpath.

    Nest members follow the binary-name convention "Outer$Member.class"; the
    nest host's binary name never contains '$'.

    E.g. 'pkg/Outer$Inner.class' -> 'pkg/Outer'
         'pkg/Outer.class'       -> 'pkg/Outer'
    """
    base = class_path[: -len('.class')]
    slash = base.rfind('/')
    dollar = base.find('$', slash + 1)
    if dollar != -1:
        base = base[:dollar]
    return base


def _SingleShard(jar_class_entries):
    single = {
        jar: [name for name, _ in entries]
        for jar, entries in jar_class_entries.items()
        if entries
    }
    return [single]


def _PartitionClassEntriesIntoShards(jar_class_entries, target_shard_size):
    """Partitions .class entries into balanced shards, keeping nests together.

    Args:
      jar_class_entries: Dict of {jar_path: [(entry_name, file_size), ...]}.
      target_shard_size: Target uncompressed byte size per shard.

    Returns:
      List of shard dicts, each mapping {jar_path: [entry_name, ...]}.
    """
    total_size = sum(
        sz for entries in jar_class_entries.values() for _, sz in entries
    )
    num_shards = min(
        os.cpu_count() or 8,
        max(1, round(total_size / target_shard_size)),
    )
    if num_shards <= 1:
        return _SingleShard(jar_class_entries)

    groups = collections.defaultdict(list)
    group_sizes = collections.defaultdict(int)
    for jar, entries in jar_class_entries.items():
        for name, sz in entries:
            key = (jar, _ClassFileNestPrefix(name))
            groups[key].append(name)
            group_sizes[key] += sz

    num_shards = min(num_shards, len(groups))
    if num_shards <= 1:
        return _SingleShard(jar_class_entries)

    sorted_keys = sorted(groups.keys(), key=lambda k: (-group_sizes[k], k))
    shards = [collections.defaultdict(list) for _ in range(num_shards)]
    shard_sizes = [0] * num_shards
    # Keep adding the next largest class cluster to the smallest shard.
    for key in sorted_keys:
        idx = min(range(num_shards), key=lambda i: shard_sizes[i])
        shards[idx][key[0]].extend(groups[key])
        shard_sizes[idx] += group_sizes[key]
    return [dict(s) for s in shards if s]


def _BuildJarInputArgs(shard_map, jar_class_entries):
    args = []
    for jar, entry_names in shard_map.items():
        if not entry_names:
            continue
        args += ['--jar-input', jar]
        if len(entry_names) < len(jar_class_entries[jar]):
            for name in entry_names:
                args += ['--jar-class', name]
    return args


def _CreateIntermediateDexJar(
    changes,
    options,
    tmp_dir,
    dex_cmd,
    output_jar,
    jar_class_entries,
    services_map,
):
    allowed_changed = set(options.class_inputs)
    allowed_changed.update(options.dex_inputs)
    allowed_changed.update(options.classpath)
    strings_changed = changes.HasStringChanges()
    non_direct_input_changed = next(
        (p for p in changes.IterChangedPaths() if p not in allowed_changed),
        None,
    )

    if strings_changed or non_direct_input_changed:
        logging.debug(
            'Full dex required: strings_changed=%s path_changed=%s',
            strings_changed,
            non_direct_input_changed,
        )

    is_incremental = bool(
        options.incremental_dir
        and not strings_changed
        and not non_direct_input_changed
        and os.path.exists(output_jar)
    )

    if is_incremental:
        required_desugar_classes_set = _ComputeRequiredDesugarClasses(
            changes,
            options.desugar_dependencies,
            options.class_inputs,
            options.classpath,
        )
        logging.debug(
            'Class files needing re-desugar: %d',
            len(required_desugar_classes_set),
        )
        classes_to_compile = {}
        removed_dex_entries = []
        for jar in options.class_inputs:
            changed_class_set = (
                set(changes.IterChangedSubpaths(jar))
                | required_desugar_classes_set
            )
            # D8 nest-based access desugaring requires the entire nest group to
            # be present, else it aborts with "Class X requires its nest host Y
            # to be on program or class path." Pull in sibling nestmates by
            # host prefix.
            nest_prefixes = {
                _ClassFileNestPrefix(path)
                for path in changed_class_set
                if _IsClassFile(path)
            }
            classes_to_compile[jar] = [
                (name, sz)
                for name, sz in jar_class_entries[jar]
                if _ClassFileNestPrefix(name) in nest_prefixes
            ]
            removed_dex_entries.extend(
                path[:-6] + '.dex'
                for path in changes.IterRemovedSubpaths(jar)
                if _IsClassFile(path)
            )
        base_jar = output_jar
    else:
        if options.desugar_dependencies and os.path.exists(
            options.desugar_dependencies
        ):
            # Since incremental dexing only ever adds to desugar_dependencies,
            # whenever full dexes are required the .desugardeps files need to
            # be manually removed.
            os.unlink(options.desugar_dependencies)
        classes_to_compile = jar_class_entries
        removed_dex_entries = []
        base_jar = None

    num_classes = sum(len(v) for v in classes_to_compile.values())
    logging.debug(
        'Class entries to dex: %d (incremental=%s)', num_classes, is_incremental
    )

    base_dex_cmd = dex_cmd + ['--file-per-class-file']
    can_shard = (
        dex_cmd[0] == _CUSTOM_D8_BINARY_PATH
        and not options.desugar_dependencies
        and not options.dump_inputs
        and not options.dex_inputs
    )
    if can_shard:
        shards = _PartitionClassEntriesIntoShards(
            classes_to_compile, _GRAALVM_SHARD_TARGET_SIZE
        )
    else:
        shards = _SingleShard(classes_to_compile)

    tmp_dex_output = os.path.join(tmp_dir, 'tmp_dex_output.jar')
    if len(shards) == 1:
        cmd_args = _BuildJarInputArgs(shards[0], jar_class_entries)
        if base_jar:
            cmd_args += ['--base-jar', base_jar]
            for dex_name in removed_dex_entries:
                cmd_args += ['--exclude-entry', dex_name]
        cmd_args += _WriteServiceArgs(services_map, tmp_dir)
        _RunD8(
            base_dex_cmd + cmd_args,
            [],
            tmp_dex_output,
            options.warnings_as_errors,
            options.show_desugar_default_interface_warnings,
        )
    else:
        logging.debug('Dexing across %d shards.', len(shards))
        shard_jars = [
            os.path.join(tmp_dir, f'shard_{i}.dex.jar')
            for i in range(len(shards))
        ]
        other_shards_dex_names = []
        if base_jar:
            other_shards_dex_names = [
                name[:-6] + '.dex'
                for s in shards[1:]
                for names in s.values()
                for name in names
            ]

        shard_dex_cmd = [a for a in base_dex_cmd if a != _GRAALVM_XMS_FLAG]

        def run_shard(idx):
            cmd_args = _BuildJarInputArgs(shards[idx], jar_class_entries)
            if idx == 0 and base_jar:
                cmd_args += ['--base-jar', base_jar]
                for dex_name in removed_dex_entries + other_shards_dex_names:
                    cmd_args += ['--exclude-entry', dex_name]
            _RunD8(
                shard_dex_cmd + cmd_args,
                [],
                shard_jars[idx],
                options.warnings_as_errors,
                options.show_desugar_default_interface_warnings,
            )

        with concurrent.futures.ThreadPoolExecutor(
            max_workers=len(shards)
        ) as executor:
            for _ in executor.map(run_shard, range(len(shards))):
                pass
        _MergeShardJars(
            shard_jars,
            tmp_dex_output,
            services_map,
            options.warnings_as_errors,
        )

    shutil.move(tmp_dex_output, output_jar)
    logging.debug('Dexed class files into %s.', output_jar)


def _OnStaleMd5(changes, options, dex_cmd):
    logging.debug('_OnStaleMd5')
    with build_utils.TempDir() as tmp_dir:
        if (
            options.incremental_dir
            and options.class_inputs
            and not options.skip_custom_d8
        ):
            jar_class_entries, class_service_jars = _ScanClassInputJars(
                options.class_inputs
            )
            services_map = _CreateServicesMap(
                class_service_jars + options.dex_inputs
            )
            if options.intermediate and not options.dex_inputs:
                _CreateIntermediateDexJar(
                    changes,
                    options,
                    tmp_dir,
                    dex_cmd,
                    options.output,
                    jar_class_entries,
                    services_map,
                )
                return

            os.makedirs(options.incremental_dir, exist_ok=True)
            inter_jar = os.path.join(
                options.incremental_dir, 'intermediate.dex.jar'
            )
            _CreateIntermediateDexJar(
                changes,
                options,
                tmp_dir,
                dex_cmd + ['--intermediate'],
                inter_jar,
                jar_class_entries,
                {},
            )
            _CreateFinalDex(
                [inter_jar] + options.dex_inputs,
                options.output,
                tmp_dir,
                dex_cmd,
                options=options,
                services_map=services_map,
            )
            return

        d8_inputs = options.class_inputs + options.dex_inputs
        _CreateFinalDex(
            d8_inputs,
            options.output,
            tmp_dir,
            dex_cmd,
            options=options,
            services_map=_CreateServicesMap(d8_inputs),
        )


def MergeDexAndServices(
    src_jars,
    zip_builder,
    apk_root_dir='',
    apk_dex_dir='',
    uncompress_dex=False,
    compress_level=1,
):
    """Adds dex files and services from src_jars to zip_builder.

    Args:
      src_jars: List of input jar paths.
      zip_builder: A zipbuilder.ZipBuilder to add entries to.
      apk_root_dir: Prefix for service paths in the zip.
      apk_dex_dir: Prefix for dex paths in the zip.
      uncompress_dex: Whether to store dex files uncompressed.
      compress_level: Compression level for compressed dex entries.
    """
    services_map = _CreateServicesMap(src_jars)
    for path, data in sorted(services_map.items()):
        zip_builder.add_data(
            apk_root_dir + path, data, compress_level=0, alignment=4
        )

    dex_idx = 0
    for jar_path in src_jars:
        with zipfile.ZipFile(jar_path, 'r') as z:
            dex_names = sorted([n for n in z.namelist() if n.endswith('.dex')])
        for name in dex_names:
            if len(src_jars) == 1:
                dest_name = name
            else:
                dest_name = 'classes{}.dex'.format(
                    dex_idx + 1 if dex_idx > 0 else ''
                )

            if uncompress_dex:
                level = 0
                alignment = 4
            else:
                level = compress_level
                alignment = None
            zip_builder.add_from_zip(
                apk_dex_dir + dest_name,
                jar_path,
                name,
                compress_level=level,
                alignment=alignment,
            )
            dex_idx += 1


def main(args):
    build_utils.InitLogging('DEX_DEBUG')
    options = _ParseArgs(args)

    options.class_inputs += options.class_inputs_filearg
    options.dex_inputs += options.dex_inputs_filearg

    input_paths = options.class_inputs + options.dex_inputs

    depfile_deps = options.class_inputs_filearg + options.dex_inputs_filearg

    output_paths = [options.output]
    track_subpaths_allowlist = (
        list(options.class_inputs) if options.incremental_dir else []
    )

    use_custom_d8_binary = (
        not options.skip_custom_d8
        and sys.platform == 'linux'
        and platform.machine() == 'x86_64'
    )
    if use_custom_d8_binary:
        input_paths.append(_CUSTOM_D8_BINARY_PATH)
        if (
            options.incremental_dir
            and options.class_inputs
            and not options.dex_inputs
            and not options.desugar_dependencies
            and not options.dump_inputs
        ):
            input_paths += zipbuilder.get_input_paths()
        dex_cmd = [
            _CUSTOM_D8_BINARY_PATH,
            f'-Djava.home={build_utils.JAVA_HOME}',
            _GRAALVM_XMS_FLAG,
        ]
    else:
        input_paths += [
            build_utils.JAVA_PATH_FOR_INPUTS,
            options.r8_jar_path,
            options.custom_d8_jar_path,
        ]
        dex_cmd = build_utils.JavaCmd(xmx=_DEX_XMX)

        # As of Feb 2026, a hyperfine benchmark of dexing chrome_java:
        # Without flags:
        # Time (mean ± σ): 7.744 s ±  0.177 s   [User: 161.982 s, System: 8.416 s]
        # With flags:
        # Time (mean ± σ): 6.579 s ±  0.057 s   [User: 37.612 s, System: 8.015 s]
        dex_cmd += ['-XX:TieredStopAtLevel=1']

    if logging.getLogger().isEnabledFor(logging.DEBUG):
        dex_cmd += ['-Dcom.android.tools.r8.printtimes=1']

    if options.dump_inputs:
        dex_cmd += ['-Dcom.android.tools.r8.dumpinputtofile=d8inputs.zip']

    if options.skip_custom_d8:
        dex_cmd += [
            '-cp',
            options.r8_jar_path,
            'com.android.tools.r8.D8',
        ]
    elif not use_custom_d8_binary:
        dex_cmd += [
            '-cp',
            f'{options.r8_jar_path}:{options.custom_d8_jar_path}',
            'org.chromium.build.CustomD8',
        ]

    if options.release:
        dex_cmd += ['--release']
    elif options.intermediate:
        dex_cmd += ['--intermediate']
        # Discard global synthetics (e.g. from record desugaring). The final
        # dex step adds them all back via global_synthetics.dex.jar from the
        # r8 CIPD package (see third_party/r8/3pp/3pp.py).
        dex_cmd += ['--ignore-globals']
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
        lambda changes: _OnStaleMd5(changes, options, dex_cmd),
        options,
        input_paths=input_paths,
        input_strings=dex_cmd + [str(bool(options.incremental_dir)), 'v2'],
        output_paths=output_paths,
        pass_changes=True,
        track_subpaths_allowlist=track_subpaths_allowlist,
        depfile_deps=depfile_deps,
    )


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
