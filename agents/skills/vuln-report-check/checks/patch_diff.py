# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Checks code change files (`.diff` or `.patch`) attached to a bug report.

A file named `patch.diff` is applied to Chrome before testing, to make the bug
easier to hit. A file with "fix" in its name is a suggested fix for engineers
and is not used in testing. Checks the names and the basic diff format. The
agent confirms that the diff applies and that it only aids reproduction.
"""

from collections.abc import Sequence
import importlib
import os
import re
import sys

if __package__:
    common = importlib.import_module('.common', __package__)
else:
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    common = importlib.import_module('common')

# Position of this check in the overall report.
ORDER = 50
# Chrome's automated reproduction system only applies a patch with this name.
REQUIRED_REPRO_PATCH_NAME = 'patch.diff'

# Word-bounded proposed-fix classifier: matches `fix.diff`,
# `proposed_fix.patch`, `bugfix.diff`, `hotfix-uaf.patch`, `fixes_issue.diff`,
# while NOT matching `prefix.diff`, `fixture.patch`, or `suffix.diff`.
FIX_PATCH_RE = re.compile(
    r'(?:^|[^a-z])(?:bug|hot)?fix(?:es|up)?(?:[^a-z]|$)',
    re.IGNORECASE,
)

# A line starting with ``` means the diff was copied out of a Markdown page.
MARKDOWN_FENCE_RE = re.compile(r'^\s*`{3,}')
# Binary diffs cannot be read as text, so they are rejected.
BINARY_DIFF_RE = re.compile(
    r'^(?:GIT binary patch|Binary files .+ and .+ differ)$'
)


def is_proposed_fix_name(filename: str) -> bool:
    """Returns whether a file name marks the file as a suggested fix.

    The word "fix" must stand on its own, so names like `prefix.diff` or
    `fixture.patch` do not count.

    Args:
        filename: Name of the attached file.

    Returns:
        True if the name looks like a suggested fix.
    """
    return bool(FIX_PATCH_RE.search(filename))


def _proposed_fix_conflict_reason(
    submission: common.Submission,
) -> str | None:
    """Returns why a "fix" file clashes with the config, if it does.

    The `proposed_fix` field in repro.json may name a different file. When the
    config check marked that field as conflicting, a short reason is shown
    next to each fix file.

    Args:
        submission: The parsed bug report and its attachments.

    Returns:
        The reason text, or None when there is no conflict.
    """
    field = submission.fields.get('proposed_fix')
    if field and field.get('state') == common.CONFLICT:
        return 'conflicting with "proposed_fix" in repro.json'
    return None


def _diff_format_problem(item: common.FileInput) -> str | None:
    """Returns what is plainly wrong with a diff file, if anything.

    This is only a quick format check. Whether the diff really applies to
    Chrome is left to the agent, which asks the researcher to confirm it.

    Args:
        item: The attached diff file.

    Returns:
        A short problem description, or None when the format looks fine.
    """
    # Files over the size limit are not read in full, so they cannot pass.
    if item.oversize:
        return (
            f'{item.name} is {item.size_bytes} bytes (limit:'
            f' {common.MAX_FILE_BYTES} bytes).'
        )
    lines = item.content.splitlines()
    if not any(line.strip() for line in lines):
        return f'{item.name} is empty.'
    for lineno, line in enumerate(lines, 1):
        if MARKDOWN_FENCE_RE.match(line):
            return (
                f'{item.name}:{lineno}: contains pasted Markdown code fences'
                ' (```).'
            )
        if BINARY_DIFF_RE.match(line):
            return (
                f'{item.name}:{lineno}: binary diffs are not supported;'
                ' submit text unified diffs only.'
            )
    has_headers = any(ln.startswith('--- ') for ln in lines) and any(
        ln.startswith('+++ ') for ln in lines
    )
    has_hunk = any(ln.startswith('@@') for ln in lines)
    if not has_headers and not has_hunk:
        return (
            f'{item.name} has no `---`/`+++` file headers and no `@@` hunk'
            ' headers.'
        )
    return None


def _check_fake_output(
    patch_files: Sequence[common.FileInput],
) -> common.Check:
    """Flags crash text that a patch adds on purpose.

    A sanitizer (a build mode that detects memory errors) prints a crash
    report. A patch could print the same text to fake a crash, so added
    lines with such text are flagged for a human to confirm.

    Args:
        patch_files: All attached `.diff` and `.patch` files.

    Returns:
        An UNKNOWN check that lists up to three matching lines, or a routine
        check when nothing matches.
    """
    hits: list[str] = []
    for item in patch_files:
        for lineno, line in enumerate(item.content.splitlines(), 1):
            if (
                line.startswith('+')
                and not line.startswith('+++')
                and common.FAKE_OUTPUT_RE.search(line)
            ):
                hits.append(f'{item.name}:{lineno}')
    if hits:
        return common.Check(
            'no_fake_output',
            common.UNKNOWN,
            f'Crash or sanitizer text added in {", ".join(hits[:3])};'
            ' confirm it is not hard-coded fake output.',
            fix=(
                'Do not inject fake sanitizer output or crash banners via a'
                ' patch.'
            ),
            axis=common.AXIS_QUALITY,
        )
    return common.Check(
        'no_fake_output',
        common.UNKNOWN,
        'No fake crash or sanitizer output added in patch lines.',
        axis=common.AXIS_QUALITY,
        routine=True,
    )


def check(submission: common.Submission) -> list[common.Check]:
    """Checks all `.diff` and `.patch` files attached to the report.

    Files are split into suggested fixes (only noted) and reproduction
    patches (checked for name and basic diff format; the agent judges the
    rest). Checks owned by the config check are downgraded here so one
    problem is not reported twice.

    Args:
        submission: The parsed bug report and its attachments.

    Returns:
        The list of checks, or an empty list when no patch is attached.
    """
    patch_files = submission.files_by_suffix('.diff', '.patch')
    if not patch_files:
        return []

    # A diff the config declares as the proposed fix is never a reproduction
    # patch, even when its name lacks `fix`; `config.proposed_fix` owns that.
    declared_fix = common.config_value(submission, 'proposed_fix')
    fix_patches = [
        f
        for f in patch_files
        if is_proposed_fix_name(f.name) or f.name == declared_fix
    ]
    repro_patches = [f for f in patch_files if f not in fix_patches]
    checks: list[common.Check] = []
    # `config.patch` already fails both axes for a misnamed reproduction patch.
    patch_name_owned_by_config = (
        submission.fields.get('patch', {}).get('state') == common.INVALID
    )

    conflict_reason = _proposed_fix_conflict_reason(submission)
    for item in fix_patches:
        if conflict_reason is not None:
            # `config.proposed_fix` already fails both axes for this; keep one
            # owning check and only note the file here.
            checks.append(
                common.Check(
                    'patch.proposed_fix',
                    common.INFO,
                    f'{item.name} is attached, {conflict_reason}.',
                    fix=(
                        f'Set "proposed_fix": "{item.name}" in repro.json or'
                        f' remove {item.name}.'
                    ),
                    axis=common.AXIS_REPRO,
                )
            )
        else:
            checks.append(
                common.Check(
                    'patch.proposed_fix',
                    common.INFO,
                    f'{item.name} is treated as a proposed fix for human'
                    ' triage only and ignored by automated reproduction.',
                    axis=common.AXIS_REPRO,
                )
            )

    if repro_patches:
        # Chrome's automated reproduction system applies at most one patch.
        if len(repro_patches) > 1:
            joined = ', '.join(f.name for f in repro_patches)
            checks.append(
                common.Check(
                    'patch.file_name',
                    common.FAIL,
                    f'Multiple reproduction patches attached ({joined});'
                    ' automated reproduction applies at most one'
                    f' {REQUIRED_REPRO_PATCH_NAME}.',
                    fix=(
                        'Combine reproduction changes into a single'
                        f' {REQUIRED_REPRO_PATCH_NAME!r}, or rename proposed'
                        " fixes to 'fix.diff'."
                    ),
                    axis=common.AXIS_REPRO,
                )
            )
        elif repro_patches[0].name != REQUIRED_REPRO_PATCH_NAME:
            bad = repro_patches[0].name
            checks.append(
                common.Check(
                    'patch.file_name',
                    common.INFO if patch_name_owned_by_config else common.FAIL,
                    f'Reproduction patch is named {bad!r}.',
                    fix=(
                        'Rename the reproduction patch to'
                        f' {REQUIRED_REPRO_PATCH_NAME!r}, or add `fix` to the'
                        " name (e.g. 'fix.diff') if it is a proposed fix."
                    ),
                    axis=common.AXIS_REPRO,
                )
            )
        else:
            checks.append(
                common.Check(
                    'patch.file_name',
                    common.PASS,
                    f'Reproduction patch is named {REQUIRED_REPRO_PATCH_NAME}.',
                    axis=common.AXIS_REPRO,
                )
            )

        # Check the file named `patch.diff` if there is one, else the first.
        primary = next(
            (f for f in repro_patches if f.name == REQUIRED_REPRO_PATCH_NAME),
            repro_patches[0],
        )
        problem = _diff_format_problem(primary)
        if problem:
            checks.append(
                common.Check(
                    'patch.valid_diff',
                    common.FAIL,
                    problem,
                    fix=(
                        'Export a clean unified diff with'
                        ' `git diff > patch.diff`.'
                    ),
                    axis=common.AXIS_REPRO,
                )
            )
        else:
            checks.append(
                common.Check(
                    'patch.valid_diff',
                    common.UNKNOWN,
                    f'{primary.name} looks like a text diff; ask the'
                    ' researcher to confirm it applies cleanly (for example'
                    ' `git apply --check` in a Chromium checkout).',
                    axis=common.AXIS_REPRO,
                    routine=True,
                )
            )
        checks.append(
            common.Check(
                'patch.aids_reproduction',
                common.UNKNOWN,
                f'Judge whether {primary.name} only helps reproduce an'
                ' existing bug, or instead fixes it, removes a security'
                ' check, or fakes the bug (see references/patch_diff.md).',
                axis=common.AXIS_REPRO,
            )
        )

    try:
        checks.append(_check_fake_output(patch_files))
    except (RecursionError, MemoryError):
        # Hostile or huge input must not crash the tool; report "needs
        # judgment" instead.
        checks.append(
            common.Check(
                'no_fake_output',
                common.UNKNOWN,
                'Hostile or oversized input exceeded parser limits.',
                axis=common.AXIS_QUALITY,
            )
        )
    return checks


def main(argv: Sequence[str] | None = None) -> int:
    """Runs only this check from the command line.

    Args:
        argv: Command-line arguments, or None to use `sys.argv`.

    Returns:
        The process exit code.
    """
    return common.run_cli(check, argv=argv, description=__doc__)


if __name__ == '__main__':
    sys.exit(main())
