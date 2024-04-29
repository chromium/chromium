# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Chromium presubmit script for base/allocator/partition_allocator.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'

_PARTITION_ALLOC_BASE_PATH = 'base/allocator/partition_allocator/src/'


# This is adapted from Chromium's PRESUBMIT.py. The differences are:
# - Base path: It is relative to the partition_alloc's source directory instead
#              of chromium.
# - Stricter: A single format is allowed: `PATH_ELEM_FILE_NAME_H_`.
def CheckForIncludeGuards(input_api, output_api):
    """Check that header files have proper include guards"""

    def guard_for_file(file):
        local_path = file.LocalPath()
        if input_api.is_windows:
            local_path = local_path.replace('\\', '/')
        assert local_path.startswith(_PARTITION_ALLOC_BASE_PATH)
        guard = input_api.os_path.normpath(
            local_path[len(_PARTITION_ALLOC_BASE_PATH):])
        guard = guard + '_'
        guard = guard.upper()
        guard = input_api.re.sub(r'[+\\/.-]', '_', guard)
        return guard

    def is_partition_alloc_header_file(f):
        # We only check header files.
        return f.LocalPath().endswith('.h')

    errors = []

    for f in input_api.AffectedSourceFiles(is_partition_alloc_header_file):
        expected_guard = guard_for_file(f)

        # Unlike the Chromium's top-level PRESUBMIT.py, we enforce a stricter
        # rule which accepts only `PATH_ELEM_FILE_NAME_H_` per coding style.
        guard_name_pattern = input_api.re.escape(expected_guard)
        guard_pattern = input_api.re.compile(r'#ifndef\s+(' +
                                             guard_name_pattern + ')')

        guard_name = None
        guard_line_number = None
        seen_guard_end = False
        for line_number, line in enumerate(f.NewContents()):
            if guard_name is None:
                match = guard_pattern.match(line)
                if match:
                    guard_name = match.group(1)
                    guard_line_number = line_number
                continue

            # The line after #ifndef should have a #define of the same name.
            if line_number == guard_line_number + 1:
                expected_line = '#define %s' % guard_name
                if line != expected_line:
                    errors.append(
                        output_api.PresubmitPromptWarning(
                            'Missing "%s" for include guard' % expected_line,
                            ['%s:%d' % (f.LocalPath(), line_number + 1)],
                            'Expected: %r\nGot: %r' % (expected_line, line)))

            if not seen_guard_end and line == '#endif  // %s' % guard_name:
                seen_guard_end = True
                continue

            if seen_guard_end:
                if line.strip() != '':
                    errors.append(
                        output_api.PresubmitPromptWarning(
                            'Include guard %s not covering the whole file' %
                            (guard_name), [f.LocalPath()]))
                    break  # Nothing else to check and enough to warn once.

        if guard_name is None:
            errors.append(
                output_api.PresubmitPromptWarning(
                    'Missing include guard in %s\n'
                    'Recommended name: %s\n' %
                    (f.LocalPath(), expected_guard)))

    return errors

def CheckBuildConfigMacrosWithoutInclude(input_api, output_api):
    # Excludes OS_CHROMEOS, which is not defined in build_config.h.
    macro_re = input_api.re.compile(
        r'^\s*#(el)?if.*\bdefined\(((COMPILER_|ARCH_CPU_|WCHAR_T_IS_)[^)]*)')
    include_re = input_api.re.compile(
        r'^#include\s+"partition_alloc/build_config.h"',
        input_api.re.MULTILINE)
    extension_re = input_api.re.compile(r'\.[a-z]+$')
    errors = []
    config_h_file = input_api.os_path.join('build', 'build_config.h')
    for f in input_api.AffectedFiles(include_deletes=False):
        # The build-config macros are allowed to be used in build_config.h
        # without including itself.
        if f.LocalPath() == config_h_file:
            continue
        if not f.LocalPath().endswith(
            ('.h', '.c', '.cc', '.cpp', '.m', '.mm')):
            continue

        found_line_number = None
        found_macro = None
        all_lines = input_api.ReadFile(f, 'r').splitlines()
        for line_num, line in enumerate(all_lines):
            match = macro_re.search(line)
            if match:
                found_line_number = line_num
                found_macro = match.group(2)
                break
        if not found_line_number:
            continue

        found_include_line = -1
        for line_num, line in enumerate(all_lines):
            if include_re.search(line):
                found_include_line = line_num
                break
        if found_include_line >= 0 and found_include_line < found_line_number:
            continue

        if not f.LocalPath().endswith('.h'):
            primary_header_path = extension_re.sub('.h', f.AbsoluteLocalPath())
            try:
                content = input_api.ReadFile(primary_header_path, 'r')
                if include_re.search(content):
                    continue
            except IOError:
                pass
        errors.append('%s:%d %s macro is used without first including '
            'partition_alloc/build_config.h.' %
            (f.LocalPath(), found_line_number, found_macro))
    if errors:
        return [output_api.PresubmitPromptWarning('\n'.join(errors))]
    return []
