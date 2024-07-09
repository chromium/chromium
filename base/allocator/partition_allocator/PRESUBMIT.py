# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Chromium presubmit script for base/allocator/partition_allocator.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'

# This is the base path of the partition_alloc directory when stored inside the
# chromium repository. PRESUBMIT.py is executed from chromium.
_PARTITION_ALLOC_BASE_PATH = 'base/allocator/partition_allocator/src/'


# Filter for C/C++ files.
def c_cpp_files(file):
    return file.LocalPath().endswith(('.h', '.hpp', '.c', '.cc', '.cpp'))

# Filter for GN files.
def gn_files(file):
    return file.LocalPath().endswith(('.gn', '.gni'))


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

# In .gn and .gni files, check there are no unexpected dependencies on files
# located outside of the partition_alloc repository.
#
# This is important, because partition_alloc has no CQ bots on its own, but only
# through the chromium's CQ.
#
# Only //build_overrides/ is allowed, as it provides embedders, a way to
# overrides the default build settings and forward the dependencies to
# partition_alloc.
def CheckNoExternalImportInGn(input_api, output_api):
    # Match and capture <path> from import("<path>").
    import_re = input_api.re.compile(r'^ *import\("([^"]+)"\)')

    errors = []
    for f in input_api.AffectedSourceFiles(gn_files):
        for line_number, line in f.ChangedContents():
            match = import_re.search(line)
            if not match:
                continue
            import_path = match.group(1)
            if import_path.startswith('//build_overrides/'):
                continue
            if not import_path.startswith('//'):
                continue;
            errors.append(output_api.PresubmitError(
                '%s:%d\nPartitionAlloc disallow external import: %s' %
                (f.LocalPath(), line_number + 1, import_path)))
    return errors;

# partition_alloc still supports C++17, because Skia still uses C++17.
def CheckCpp17CompatibleHeaders(input_api, output_api):
    CPP_20_HEADERS = [
        "barrier",
        "bit",
        "compare",
        "format",
        "numbers",
        "ranges",
        "semaphore",
        "source_location",
        "span",
        "stop_token",
        "syncstream",
        "version",
    ]

    CPP_23_HEADERS = [
        "expected",
        "flat_map",
        "flat_set",
        "generator",
        "mdspan",
        "print",
        "spanstream",
        "stacktrace",
        "stdatomic.h",
        "stdfloat",
    ]

    errors = []
    for f in input_api.AffectedSourceFiles(c_cpp_files):
        # for line_number, line in f.ChangedContents():
        for line_number, line in enumerate(f.NewContents()):
            for header in CPP_20_HEADERS:
                if not "#include <%s>" % header in line:
                    continue
                errors.append(
                    output_api.PresubmitError(
                        '%s:%d\nPartitionAlloc disallows C++20 headers: <%s>'
                        % (f.LocalPath(), line_number + 1, header)))
            for header in CPP_23_HEADERS:
                if not "#include <%s>" % header in line:
                    continue
                errors.append(
                    output_api.PresubmitError(
                        '%s:%d\nPartitionAlloc disallows C++23 headers: <%s>'
                        % (f.LocalPath(), line_number + 1, header)))
    return errors

def CheckCpp17CompatibleKeywords(input_api, output_api):
    CPP_20_KEYWORDS = [
        "concept",
        "consteval",
        "constinit",
        "co_await",
        "co_return",
        "co_yield",
        "requires",
    ]
    # Note: C++23 doesn't introduce new keywords.

    errors = []
    for f in input_api.AffectedSourceFiles(c_cpp_files):
        for line_number, line in f.ChangedContents():
            for keyword in CPP_20_KEYWORDS:
                if not keyword in line:
                    continue
                # Skip if part of a comment
                if '//' in line and line.index('//') < line.index(keyword):
                    continue

                # Make sure there are word separators around the keyword:
                regex = r'\b%s\b' % keyword
                if not input_api.re.search(regex, line):
                    continue

                errors.append(
                    output_api.PresubmitError(
                        '%s:%d\nPartitionAlloc disallows C++20 keywords: %s'
                        % (f.LocalPath(), line_number + 1, keyword)))
    return errors
