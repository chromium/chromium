#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Runs R8's TraceReferences tool to ensure DEX files are valid.

Also checks that DEX files do not reference disallowed library classes.
"""

import argparse
import collections
import json
import logging
import os
import pathlib
import re
import shutil
import sys
import tempfile
import zipfile

from util import build_utils
from util import server_utils
import action_helpers  # build_utils adds //build to sys.path.

_DUMP_DIR_NAME = 'r8inputs_tracerefs'
_SINGLEJAR_PATH = os.path.join(
    build_utils.DIR_SOURCE_ROOT,
    'third_party/android_build_tools/bazel_tools/cipd/singlejar',
)

_SUPPRESSION_PATTERN = '|'.join(
    [
        # Summary contains warning count, which our filtering makes wrong.
        r'Warning: Tracereferences found',
        r'dalvik\.system',
        r'libcore\.io',
        r'sun\.misc\.Unsafe',
        # Explicictly guarded by try (NoClassDefFoundError) in Flogger's
        # PlatformProvider.
        r'com\.google\.common\.flogger\.backend\.google\.GooglePlatform',
        r'com\.google\.common\.flogger\.backend\.system\.DefaultPlatform',
        # TODO(agrieve): Exclude these only when use_jacoco_coverage=true.
        r'java\.lang\.instrument\.ClassFileTransformer',
        r'java\.lang\.instrument\.IllegalClassFormatException',
        r'java\.lang\.instrument\.Instrumentation',
        r'java\.lang\.management\.ManagementFactory',
        r'javax\.management\.MBeanServer',
        r'javax\.management\.ObjectInstance',
        r'javax\.management\.ObjectName',
        r'javax\.management\.StandardMBean',
        # Explicitly guarded by try (NoClassDefFoundError) in Firebase's
        # KotlinDetector: com.google.firebase.platforminfo.KotlinDetector.
        r'kotlin\.KotlinVersion',
        # Not sure why these two are missing, but they do not seem important.
        r'ResultIgnorabilityUnspecified',
        r'kotlin\.DeprecationLevel',
        # Assume missing android.* / java.* references are OS APIs that are not in
        # android.jar. Not in the above list so as to not match parameter types.
        # E.g. Missing method void android.media.MediaRouter2$RouteCallback
        # E.g. Missing class android.util.StatsEvent$Builder
        r'Missing (?:field|method) \S+ android\.',
        r'Missing class android\.',
        # The follow classes are from Android XR system libraries and used on
        # immersive environment.
        r'Missing class com.google.ar.imp.core\.',
    ]
)

# E.g.: Missing method void android.webkit.WebView.reload() (referenced from:
#     void org.Foo.bar() and 2 other contexts)
_MISSING_DEFINITION_RE = re.compile(
    r'Missing (class|field|method) (.+?)'
    r'(?: \(referenced from: (.+?)(?: and \d+ other contexts?)?\))?$'
)


def _ParentDirs(name):
    """E.g.: "a/b/C.class" -> ["a/", "a/b/"]."""
    parts = name.split('/')
    return ['/'.join(parts[:i]) + '/' for i in range(1, len(parts))]


def _IncludePrefixes(names, excluded):
    """Returns prefixes that match all zip entries in names except excluded.

    Parent directories of excluded entries are not matched either. Uses
    directories where possible to keep the list short. Returns an empty list
    when there are no other entries.
    """
    excluded_dirs = {d for name in excluded for d in _ParentDirs(name)}
    ret = set()
    for name in names:
        if name in excluded or name in excluded_dirs:
            continue
        dirs = _ParentDirs(name)
        ret.add(next((d for d in dirs if d not in excluded_dirs), name))
    return sorted(ret)


def _RemoveClassesFromJars(jar_paths, class_names, tmp_dir):
    """Returns jar_paths, with jars that contain any of class_names replaced.

    Replacement jars are written to tmp_dir, and omit the given classes.
    """
    entries_to_remove = {
        name.replace('.', '/') + '.class' for name in class_names
    }
    found_entries = set()
    ret = []
    for idx, jar_path in enumerate(jar_paths):
        with zipfile.ZipFile(jar_path) as z:
            names = z.namelist()
        matches = entries_to_remove.intersection(names)
        if not matches:
            ret.append(jar_path)
            continue
        found_entries.update(matches)
        prefixes = _IncludePrefixes(names, matches)
        if not prefixes:
            # Nothing else in the jar (and singlejar would include everything).
            continue
        new_path = os.path.join(tmp_dir, f'{idx}_{os.path.basename(jar_path)}')
        # singlejar copies entries without decompressing them, which is much
        # faster than zipfile. It has no exclude flag, so uses include prefixes.
        cmd = [
            _SINGLEJAR_PATH,
            '--output',
            new_path,
            '--sources',
            jar_path,
            '--dont_change_compression',
            '--exclude_build_data',
            '--include_prefixes',
        ]
        build_utils.CheckOutput(cmd + prefixes)
        ret.append(new_path)

    missing_entries = entries_to_remove - found_entries
    if missing_entries:
        missing_classes = sorted(
            e[: -len('.class')].replace('/', '.') for e in missing_entries
        )
        raise Exception(
            'Disallowed library classes not found in any library jar: '
            + ', '.join(missing_classes)
        )
    return ret


def _SymbolClass(kind, symbol):
    """Returns the class that a missing symbol belongs to."""
    # E.g.: "org.Foo", "int org.Foo.mBar", or "void org.Foo.bar(int, int)".
    # Uses only leading tokens so that unexpected trailing text is ignored.
    tokens = symbol.split(' ')
    if kind == 'class':
        return tokens[0]
    return tokens[1].split('(')[0].rpartition('.')[0]


def _ParseContext(context):
    """Returns (class_name, member) for a "referenced from" context.

    member is a ProGuard member specification, or None when the context is a
    class, or is a member that is not present in R8's inputs.
    """
    signature, paren, params = context.partition('(')
    member_type, _, qualified_name = signature.rpartition(' ')
    if not member_type:
        # E.g.: org.Foo
        class_name, member_name = context, None
    else:
        # E.g.: "void org.Foo.bar(int, int)" or "int org.Foo.mBar".
        class_name, _, member_name = qualified_name.rpartition('.')
    # Use the enclosing class for classes and members that -whyareyoukeeping
    # cannot match, such as those created by javac and R8 (e.g. <clinit>,
    # lambda$bar$0, Foo$-CC, and Foo$$ExternalSyntheticLambda0).
    outer_class_name = re.sub(r'\$[$-].*', '', class_name)
    if (
        outer_class_name != class_name
        or not member_name
        or '$' in member_name
        or member_name == '<clinit>'
    ):
        return outer_class_name, None
    if member_name == '<init>':
        return class_name, f'<init>({params}'
    return class_name, f'{member_type} {member_name}{paren}{params}'


def _FindDisallowedReferences(stderr, disallowed_classes):
    """Returns diagnostics about references to disallowed_classes.

    Returns:
      A list of (diagnostic, context) tuples, where context is the
      "referenced from" part of the diagnostic (or None if absent).
    """
    if not disallowed_classes:
        return []
    ret = []
    for line in stderr.splitlines():
        m = _MISSING_DEFINITION_RE.search(line)
        if m and _SymbolClass(m.group(1), m.group(2)) in disallowed_classes:
            ret.append((m.group(0), m.group(3)))
    return ret


def _CreateDisallowedReferencesMessage(violations, disallowed_classes):
    lines = [
        'TraceReferences failed: DEX contains references to classes listed in '
        'disallowed_library_classes: ' + ', '.join(sorted(disallowed_classes))
    ]
    lines += [diagnostic for diagnostic, _ in violations]

    members_by_class = collections.defaultdict(set)
    for _, context in violations:
        if context:
            class_name, member = _ParseContext(context)
            members = members_by_class[class_name]
            if member:
                members.add(member)
    if not members_by_class:
        return '\n'.join(lines) + '\n'

    lines += [
        '',
        'To see why the referencing code is kept, add these rules to a',
        'ProGuard config (e.g. //base/android/proguard/chromium_apk.flags).',
        'If names are obfuscated, also set enable_proguard_obfuscation=false.',
    ]
    for class_name, members in sorted(members_by_class.items()):
        if members:
            lines.append(f'-whyareyoukeeping class {class_name} {{')
            lines += [f'    {member};' for member in sorted(members)]
            lines.append('}')
        else:
            lines.append(f'-whyareyoukeeping class {class_name}')
    return '\n'.join(lines) + '\n'


def _RunTraceReferences(error_title, r8jar, libs, dex_files, options):
    cmd = build_utils.JavaCmd(xmx='2G')

    if options.dump_inputs:
        cmd += [f'-Dcom.android.tools.r8.dumpinputtodirectory={_DUMP_DIR_NAME}']

    cmd += [
        '-cp',
        r8jar,
        'com.android.tools.r8.tracereferences.TraceReferences',
        '--map-diagnostics:MissingDefinitionsDiagnostic',
        'error',
        'warning',
        '--check',
    ]

    for path in libs:
        cmd += ['--lib', path]
    for path in dex_files:
        cmd += ['--source', path]

    failed_holder = [False]

    def stderr_filter(stderr):
        # Must happen before _SUPPRESSION_PATTERN, which filters out android.*.
        disallowed_classes = options.disallowed_library_classes
        violations = _FindDisallowedReferences(stderr, disallowed_classes)
        if violations:
            failed_holder[0] = True
            # Omit other diagnostics, since they are likely caused by the same
            # code (e.g. uses of members inherited from a disallowed class).
            return _CreateDisallowedReferencesMessage(
                violations, disallowed_classes
            )

        had_unfiltered_items = '  ' in stderr
        stderr = build_utils.FilterLines(stderr, _SUPPRESSION_PATTERN)
        if stderr:
            if 'Missing' in stderr:
                failed_holder[0] = True
                stderr = (
                    'TraceReferences failed: '
                    + error_title
                    + """
Tip: Build with:
        is_java_debug=false
        treat_warnings_as_errors=false
        enable_proguard_obfuscation=false
     and then use dexdump to see which class(s) reference them.

     E.g.:
       third_party/android_sdk/public/build-tools/*/dexdump -d \
out/Release/apks/YourApk.apk > dex.txt
"""
                    + stderr
                )
            elif had_unfiltered_items:
                # Left only with empty headings. All indented items filtered out.
                stderr = ''
        return stderr

    try:
        if options.verbose:
            stderr_filter = None
        build_utils.CheckOutput(
            cmd,
            print_stdout=True,
            stderr_filter=stderr_filter,
            fail_on_output=options.warnings_as_errors,
        )
    except build_utils.CalledProcessError as e:
        # Do not output command line because it is massive and makes the actual
        # error message hard to find.
        sys.stderr.write(e.output)
        sys.exit(1)
    return failed_holder[0]


def main():
    build_utils.InitLogging('TRACEREFS_DEBUG')

    parser = argparse.ArgumentParser()
    parser.add_argument('--tracerefs-json')
    parser.add_argument(
        '--use-build-server',
        action='store_true',
        help='Always use the build server.',
    )
    parser.add_argument('--stamp')
    parser.add_argument('--depfile')
    parser.add_argument(
        '--warnings-as-errors',
        action='store_true',
        help='Treat all warnings as errors.',
    )
    parser.add_argument(
        '--dump-inputs',
        action='store_true',
        help='Use when filing R8 bugs to capture inputs.'
        ' Stores inputs to r8inputs.zip',
    )
    parser.add_argument(
        '--verbose', action='store_true', help='Do not filter output'
    )
    parser.add_argument(
        '--disallowed-library-classes',
        action='append',
        help='GN-list of library classes that DEX files must not reference.',
    )
    args = parser.parse_args()
    args.disallowed_library_classes = frozenset(
        action_helpers.parse_gn_list(args.disallowed_library_classes)
    )

    with open(args.tracerefs_json, encoding='utf-8') as f:
        spec = json.load(f)
    r8jar = spec['r8jar']
    libs = spec['libs']

    # No need for r8jar here because GN knows about it already.
    depfile_deps = []
    depfile_deps += libs
    for job in spec['jobs']:
        depfile_deps += job['jars']

    action_helpers.write_depfile(args.depfile, args.stamp, depfile_deps)

    if server_utils.MaybeRunCommand(
        name=args.stamp,
        argv=sys.argv,
        stamp_file=args.stamp,
        use_build_server=args.use_build_server,
    ):
        return

    if args.dump_inputs:
        # Dumping inputs causes output to be emitted, avoid failing due to stdout.
        args.warnings_as_errors = False

        # Use dumpinputtodirectory instead of dumpinputtofile to avoid failing the
        # build and keep running tracereferences.
        dump_dir_name = _DUMP_DIR_NAME
        dump_dir_path = pathlib.Path(dump_dir_name)
        if dump_dir_path.exists():
            shutil.rmtree(dump_dir_path)
        # The directory needs to exist before r8 adds the zip files in it.
        dump_dir_path.mkdir()

    logging.debug('Running TraceReferences')
    error_title = 'DEX contains references to non-existent symbols after R8.'
    with tempfile.TemporaryDirectory() as tmp_dir:
        if args.disallowed_library_classes:
            # Causes TraceReferences to report all references to them as
            # missing definitions.
            libs = _RemoveClassesFromJars(
                libs, args.disallowed_library_classes, tmp_dir
            )
        for job in spec['jobs']:
            name = job['name']
            dex_files = job['jars']
            if _RunTraceReferences(error_title, r8jar, libs, dex_files, args):
                # Failed but didn't raise due to warnings_as_errors=False
                break
            error_title = (
                f'DEX within module "{name}" contains reference(s) to '
                'symbols within child splits'
            )

    logging.info('Checks completed.')
    server_utils.MaybeTouch(args.stamp)


if __name__ == '__main__':
    main()
