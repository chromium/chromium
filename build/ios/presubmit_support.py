# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit helpers for ios

See https://www.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

from . import update_bundle_filelist


def CheckBundleData(input_api, output_api, base, globroot='//'):
    root = input_api.change.RepositoryRoot()
    filelist = input_api.os_path.join(
        input_api.PresubmitLocalPath(), base + '.filelist'
    )
    globlist = input_api.os_path.join(
        input_api.PresubmitLocalPath(), base + '.globlist'
    )
    if globroot.startswith('//'):
        globroot = input_api.os_path.join(
            input_api.change.RepositoryRoot(), globroot[2:]
        )
    else:
        globroot = input_api.os_path.join(
            input_api.PresubmitLocalPath(), globroot
        )
    if (
        update_bundle_filelist.process_filelist(
            filelist, globlist, globroot, check=True, verbose=input_api.verbose
        )
        == 0
    ):
        return []
    else:
        script = input_api.os_path.join(
            input_api.change.RepositoryRoot(),
            'build',
            'ios',
            'update_bundle_filelist.py',
        )

        return [
            output_api.PresubmitError(
                'Filelist needs to be re-generated. '
                'Please run \'python3 %s %s %s %s\' '
                'and include the changes in this CL'
                % (script, filelist, globlist, globroot)
            )
        ]


def CheckNotFatalUntilAdoption(input_api, output_api, path_filter=None):
    """Encourages base::NotFatalUntil in iOS-facing code."""

    def FileFilter(affected_file):
        if affected_file.Action() == 'D':
            return False
        if path_filter and not path_filter(affected_file):
            return False
        return input_api.FilterSourceFile(
            affected_file,
            files_to_check=[r'.*\.(cc|h|mm)$'],
            files_to_skip=input_api.DEFAULT_FILES_TO_SKIP,
        )

    # Regex for standard CHECKs (excluding CHECK_DEREF)
    check_re = input_api.re.compile(
        r'\b(CHECK|CHECK_EQ|CHECK_NE|CHECK_LT|CHECK_LE|CHECK_GT|'
        r'CHECK_GE|PCHECK|NOTREACHED)\s*\('
    )
    nfu_re = input_api.re.compile(r'base::NotFatalUntil')

    problems = []

    for f in input_api.AffectedSourceFiles(FileFilter):
        changed_lines = list(f.ChangedContents())
        if not changed_lines:
            continue

        new_contents = f.NewContents()
        for line_num, line in changed_lines:
            # Strip comments to avoid false positives.
            clean_line = input_api.re.sub(r'//.*', '', line)

            if check_re.search(clean_line) and not nfu_re.search(clean_line):
                # Check if this line or the next few lines
                # contain NotFatalUntil. This handles multi-line macros.
                context = '\n'.join(new_contents[line_num - 1 : line_num + 3])
                if nfu_re.search(context):
                    continue

                # Check if this is a "promotion" (removing NotFatalUntil from an
                # existing CHECK).
                is_promotion = False
                if f.OldContents():
                    old_window_start = max(0, line_num - 3)
                    old_window_end = line_num + 2
                    old_context = '\n'.join(
                        f.OldContents()[old_window_start:old_window_end]
                    )
                    if check_re.search(old_context) and nfu_re.search(
                        old_context
                    ):
                        is_promotion = True

                if not is_promotion:
                    problems.append(
                        '%s:%d: %s' % (f.LocalPath(), line_num, line.strip())
                    )

    if not problems:
        return []

    return [
        output_api.PresubmitPromptWarning(
            'Consider using base::NotFatalUntil for new CHECKs in iOS-facing '
            'code to allow safer rollout. //ios/web_view is used as a shared '
            'framework by other embedders which '
            'require a buffer period to update and handle new invariants '
            'before they become fatal crashes '
            '(see style guide: https://chromium.googlesource.com/chromium/'
            'src/+/HEAD/styleguide/c++/checks.md#notfataluntil).\n'
            'Affected lines:\n' + '\n'.join(problems)
        )
    ]


def CheckDiscourageCheckDeref(input_api, output_api, path_filter=None):
    """Discourages CHECK_DEREF in iOS-facing code."""

    def FileFilter(affected_file):
        if affected_file.Action() == 'D':
            return False
        if path_filter and not path_filter(affected_file):
            return False
        return input_api.FilterSourceFile(
            affected_file,
            files_to_check=[r'.*\.(cc|h|mm)$'],
            files_to_skip=input_api.DEFAULT_FILES_TO_SKIP,
        )

    check_deref_re = input_api.re.compile(r'\bCHECK_DEREF\s*\(')

    problems = []

    for f in input_api.AffectedSourceFiles(FileFilter):
        for line_num, line in f.ChangedContents():
            # Strip comments to avoid false positives.
            clean_line = input_api.re.sub(r'//.*', '', line)

            if check_deref_re.search(clean_line):
                problems.append(
                    '%s:%d: %s' % (f.LocalPath(), line_num, line.strip())
                )

    if not problems:
        return []

    return [
        output_api.PresubmitPromptWarning(
            'Avoid using CHECK_DEREF in iOS/web_view code. Its design is '
            'incompatible with non-fatal rollouts (continuing on null triggers '
            'a hardware crash). Instead, use a standard CHECK with a milestone '
            'or a manual pointer check followed by a safe fallback (e.g., an '
            'early return).\n'
            'Affected lines:\n' + '\n'.join(problems)
        )
    ]
