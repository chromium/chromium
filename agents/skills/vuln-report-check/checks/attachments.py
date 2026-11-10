# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Checks the files attached to a bug report.

Ensure no file is too big, no file is an archive such as `.zip`, and the
file to run has an allowed name: `poc.html`, `index.html`, `poc.js`, or
`server.py`.
"""

from collections.abc import Sequence
import importlib
import sys

common = importlib.import_module(
    f'{__package__}.common' if __package__ else 'common'
)

ORDER = 10
# Chrome's automated reproduction system does not unpack archives, so these
# endings always fail.
ARCHIVE_SUFFIXES = (
    '.zip',
    '.tar',
    '.gz',
    '.tgz',
    '.7z',
    '.rar',
    '.bz2',
    '.xz',
)
# File endings that could be the file to run (a page, a script, or a server).
ENTRY_SUFFIXES = ('.html', '.htm', '.xhtml', '.svg', '.js', '.mjs', '.py')
# Automated reproduction also runs `index.html`, so it is a valid name; when
# both are attached `poc.html` wins (see `_PREFERRED_HTML`).
CANONICAL_ENTRIES = ('poc.html', 'index.html', 'poc.js')
_PREFERRED_HTML = 'poc.html'
CANONICAL_SERVER = 'server.py'


def _fail_both(check_id: str, message: str, fix: str) -> list[common.Check]:
    """Builds the same failure for both answers: report quality and repro.

    A bad attachment makes the report incomplete and also stops automated
    reproduction, so one problem is reported on both axes.

    Args:
        check_id: Id of the quality result. The repro result adds `.repro`.
        message: What is wrong.
        fix: How the researcher can fix it.

    Returns:
        Two failed results, one per axis.
    """
    return [
        common.Check(
            id=check_id,
            status=common.FAIL,
            message=message,
            fix=fix,
            axis=common.AXIS_QUALITY,
        ),
        common.Check(
            id=f'{check_id}.repro',
            status=common.FAIL,
            message=message,
            fix=fix,
            axis=common.AXIS_REPRO,
        ),
    ]


def _excerpt_join(items: Sequence[str]) -> str:
    """Joins file names into one short, safe line of text.

    File names come from the researcher, so they are untrusted. `excerpt`
    cuts the text to a safe length before it goes into a message.
    """
    return common.excerpt(', '.join(items))


def _manual_route(layout_message: str, reason: str) -> list[common.Check]:
    """Accepts the files but marks the bug for a person to reproduce.

    Used for setups the automated reproduction system cannot run, such as
    Android or an unpacked browser extension. This is not a failure.

    Args:
        layout_message: Message for the quality result.
        reason: Why the bug needs manual checking, shown in the repro result.

    Returns:
        A passing quality result and a passing repro result marked manual.
    """
    return [
        common.Check(
            id='attachments.layout',
            status=common.PASS,
            message=layout_message,
            axis=common.AXIS_QUALITY,
        ),
        common.Check(
            id='attachments.entry_poc',
            status=common.PASS,
            message=f'{reason} routes to manual verification.',
            fix=common.MANUAL_SUGGESTION,
            axis=common.AXIS_REPRO,
            manual=True,
        ),
    ]


def _entry_checks(entry: str) -> list[common.Check]:
    """Checks that the file to run has a name automated reproduction accepts.

    The automated reproduction system only runs files with fixed names:
    `poc.html` or `index.html` in Chrome, `poc.js` in d8 (V8's command-line
    JavaScript shell), or `server.py` for PoCs that need a web server.

    Args:
        entry: Name of the file that would be run.

    Returns:
        Passing results if the name is allowed, otherwise failures on both
        axes with a rename hint.
    """
    safe_entry = common.excerpt(entry)
    if entry == CANONICAL_SERVER:
        return [
            common.Check(
                id='attachments.layout',
                status=common.PASS,
                message='Valid `server.py` attachment layout.',
                axis=common.AXIS_QUALITY,
            ),
            common.Check(
                id='attachments.entry_poc',
                status=common.PASS,
                message='`server.py` entry target verified.',
                axis=common.AXIS_REPRO,
            ),
        ]
    if entry not in CANONICAL_ENTRIES and not entry.lower().endswith('.py'):
        # `.htm` looks like HTML but is not one of the names that is run.
        htm_note = (
            ' (.htm is not supported for standalone HTML reproduction)'
            if entry.lower().endswith('.htm')
            else ''
        )
        return _fail_both(
            'attachments.entry_name',
            f'Non-canonical PoC filename `{safe_entry}`{htm_note}.',
            'Rename the entry file to `poc.html` or `index.html` (Chrome) or'
            ' `poc.js` (d8), or serve multi-file PoCs via `server.py`.',
        )
    if entry.lower().endswith('.py'):
        return _fail_both(
            'attachments.entry_name',
            f'Server script is named `{safe_entry}`.',
            'Rename the server script to `server.py`.',
        )
    return [
        common.Check(
            id='attachments.layout',
            status=common.PASS,
            message=f'Canonical entry `{safe_entry}` attached.',
            axis=common.AXIS_QUALITY,
        ),
        common.Check(
            id='attachments.entry_poc',
            status=common.PASS,
            message=(
                f'Entry `{safe_entry}` matches automated reproduction matrix.'
            ),
            axis=common.AXIS_REPRO,
        ),
    ]


def _run_check(submission: common.Submission) -> list[common.Check]:
    """Checks file sizes, archives, and which file will be run.

    Checks go from simple to detailed and stop at the first failure: size,
    then archives, then manual setups, then the file to run. File contents
    are only read as data here; researcher code is never run.

    Args:
        submission: The bug report and its attached files.

    Returns:
        The results of this check. Empty if there is nothing to check.
    """
    # Files over the size limit were cut short when read, so the other checks
    # could not see all of them. Python files are left out of this rule.
    oversize_names = [
        file_input.name
        for file_input in submission.files
        if (
            file_input.oversize or file_input.size_bytes > common.MAX_FILE_BYTES
        )
        and not file_input.name.lower().endswith('.py')
    ]
    if oversize_names:
        return _fail_both(
            'attachments.size',
            f'Attachment(s) exceed {common.MAX_FILE_BYTES} bytes and were'
            f' truncated: {_excerpt_join(oversize_names)}.',
            f'Minimize attachments to {common.MAX_FILE_BYTES} bytes or less so'
            ' validation can inspect the full file.',
        )

    bad_names = [
        file_input.name
        for file_input in submission.files
        if file_input.name.lower().endswith(ARCHIVE_SUFFIXES)
    ]
    if bad_names:
        return _fail_both(
            'attachments.no_archives',
            'Archive attachment(s) are not extracted by automated reproduction:'
            f' {_excerpt_join(bad_names)}.',
            'Attach uncompressed flat files directly (e.g. `poc.html`,'
            ' `poc.js`, or `server.py` + `poc.html`).',
        )

    config = common.load_repro_config(submission)
    env = common.config_value(submission, 'environment')
    safe_env = common.excerpt(str(env)) if env else ''
    # `repro.json` and similar files describe the bug; they are not part of
    # the PoC, so they are left out.
    names = [
        file_input.name
        for file_input in submission.files
        if not common.is_metadata_file(file_input.name)
    ]
    if not names:
        if env in common.MANUAL_ENVIRONMENTS:
            return _manual_route(
                'Manual environment attachments accepted.',
                f'Environment `{safe_env}`',
            )
        return []

    # `manifest.json` means an unpacked browser extension. It and the manual
    # environments (for example Android) need a person to reproduce.
    if 'manifest.json' in names or env in common.MANUAL_ENVIRONMENTS:
        reason = (
            f'Environment `{safe_env}`'
            if env in common.MANUAL_ENVIRONMENTS
            else 'Unpacked extension layout'
        )
        return _manual_route('Manual attachment layout accepted.', reason)

    if config is not None and config.valid:
        # The config names the entry, so extra pages, frames, and scripts are
        # content, not rival PoCs. A missing or unattached entry is reported
        # once, by `config.entry`.
        server = config.values['server']
        entry = server['file'] if server is not None else config.values['poc']
        if entry is None or entry not in names:
            return [
                common.Check(
                    id='attachments.layout',
                    status=common.PASS,
                    message='Attachment layout follows repro.json.',
                    axis=common.AXIS_QUALITY,
                )
            ]
        return _entry_checks(entry)

    # Without a valid config `config.present` / `config.valid` already fail, so
    # only name a clear entry here and never report competing candidates.
    candidate_names = [
        name for name in names if name.lower().endswith(ENTRY_SUFFIXES)
    ]
    if CANONICAL_SERVER in candidate_names:
        return _entry_checks(CANONICAL_SERVER)
    canonical = [name for name in candidate_names if name in CANONICAL_ENTRIES]
    # Same choice as automated reproduction: `poc.html` beats `index.html`.
    if _PREFERRED_HTML in canonical and 'index.html' in canonical:
        canonical.remove('index.html')
    if len(canonical) == 1:
        return _entry_checks(canonical[0])
    if len(candidate_names) == 1:
        sole_entry = candidate_names[0]
        # A lone Python script with no config is not a `server.py` setup, so
        # it goes to a person instead of failing on its name.
        if config is None and sole_entry.lower().endswith('.py'):
            safe_entry = common.excerpt(sole_entry)
            return _manual_route(
                f'Standalone Python script `{safe_entry}` accepted.',
                f'Standalone Python script `{safe_entry}`',
            )
        return _entry_checks(sole_entry)
    return [
        common.Check(
            id='attachments.layout',
            status=common.PASS,
            message='Companion attachments valid.',
            axis=common.AXIS_QUALITY,
        )
    ]


def check(submission: common.Submission) -> list[common.Check]:
    """Runs the attachment check without letting it crash.

    Deeply nested input can hit Python's recursion limit. Instead of a crash,
    that case becomes an `unknown` ("needs judgment") result for a person.

    Args:
        submission: The bug report and its attached files.

    Returns:
        The attachment results, or one `unknown` result on a recursion error.
    """
    try:
        return _run_check(submission)
    except RecursionError:
        return [
            common.Check(
                id='attachments.recursion_limit',
                status=common.UNKNOWN,
                axis=common.AXIS_QUALITY,
                manual=True,
                message=(
                    'Attachment check hit Python recursion limit; review'
                    ' manually.'
                ),
            )
        ]


def main(argv: Sequence[str] | None = None) -> int:
    """Runs only this check from the command line.

    Args:
        argv: Command-line arguments. `None` means use `sys.argv`.

    Returns:
        The process exit code.
    """
    return common.run_cli(
        check, argv=argv, description=__doc__, positional_report=False
    )


if __name__ == '__main__':
    sys.exit(main())
