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
