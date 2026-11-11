# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Checks a JavaScript test file (`poc.js`) that shows a bug in V8.

V8 is Chrome's JavaScript engine, and `d8` is its command-line tool. Checks,
without running the file, that `d8` can run it in its strict security-test
mode: the file is plain JavaScript, needs no other files, uses no web-page
features and keeps the security-mode flag first. Questions that need
knowledge of current V8 (intrinsics, other flags) are left `unknown` for the
agent, so no V8 list has to be kept in sync here. Invalid JavaScript never
fails a check, because the syntax error may itself be the bug.
"""

from collections.abc import Sequence
import dataclasses
import importlib
import os
import re
import sys

if __package__:
    common = importlib.import_module('.common', __package__)
else:
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    common = importlib.import_module('common')

ORDER = 50
REQUIRED_NAME = 'poc.js'
# Lines longer than this are skipped by the regex checks, so one huge
# hostile line cannot make a scan slow.
MAX_LINE_LENGTH = 65_536
SECURITY_MODE_FLAGS = frozenset(
    {'run-as-security-poc', 'run-as-sandbox-security-poc'}
)

CHECK_ORDER = (
    'compiles',
    'deps_available',
    'supported_apis',
    'no_fake_output',
    'js.flags_compatible',
    'js.environment_independent',
)

CHECK_AXES = {
    'compiles': common.AXIS_REPRO,
    'deps_available': common.AXIS_REPRO,
    'supported_apis': common.AXIS_REPRO,
    'no_fake_output': common.AXIS_QUALITY,
    'js.flags_compatible': common.AXIS_REPRO,
    'js.environment_independent': common.AXIS_REPRO,
}

SERVER_RE = re.compile(
    r'http\.server|socketserver|HTTPServer|serve_forever|aiohttp|flask|'
    r'run_app|socket\.socket|asyncio\.start_server'
)
MARKDOWN_FENCE_RE = re.compile(r'^\s*`{3,}')
HTML_MARKUP_RE = re.compile(r'^\s*(?:<!doctype\b|<html\b|<script\b)', re.I)
MJSUNIT_FLAGS_COMMENT_RE = re.compile(r'^[ \t]*//[ \t]*Flags:', re.I | re.M)
# One single-line string literal. The `{0,4096}` limit keeps matching fast on
# long hostile lines; longer strings are skipped.
STRING_LITERAL_RE = re.compile(r'([\'"`])((?:\\.|(?!\1)[^\\\n]){0,4096})\1')

# Calls that load another file. The `{0,256}` limit on `import ... from`
# keeps matching fast on long hostile lines.
FILE_LOADING_CALL_RE = re.compile(
    r'\b(?:load|read|readbuffer)\s*\(|\bd8\.file\.execute\s*\('
    r'|\brequire\s*\(|\bimport\s*[(\'"`]'
    r'|\bimport\b[^;\n]{0,256}?\bfrom\s*[\'"`]'
)
DOM_OR_DISABLED_API_RE = re.compile(
    r'\b(?:document|window|navigator|localStorage|sessionStorage|indexedDB|'
    r'XMLHttpRequest|WebSocket|ServiceWorker|HTMLElement|testRunner|'
    r'eventSender|internals|Mojo)\b\s*\.|'
    r'\bfetch\s*\(|\bos\.(?:system|chdir|mkdirp|rmdir|setenv|umask)\s*\(|'
    r'\bwriteFile\s*\('
)


@dataclasses.dataclass
class JsPoc:
    """Holds one standalone V8 `poc.js` file and the text the checks scan.

    Attributes:
      code_only: The source with comments and string contents blanked out,
        so code searches skip them. Line numbers match the source.
      strings: `(line, text)` for each single-line string literal outside
        comments.
    """

    name: str
    path: str
    source: str
    oversize: bool = False
    size_bytes: int = 0
    code_only: str = ''
    strings: list[tuple[int, str]] = dataclasses.field(default_factory=list)


def _string_literals(source: str) -> list[tuple[int, str]]:
    """Returns `(line, text)` for each single-line string literal.

    Comments are blanked first, so quotes inside them are not read as
    strings. A string that spans lines or is longer than the regex limit is
    skipped; the checks that use this list are hints, not proofs.
    """
    without_comments = common.strip_js_comments_and_strings(
        source, keep_strings=True
    )
    return [
        (lineno, match.group(2))
        for lineno, line in enumerate(without_comments.splitlines(), 1)
        if len(line) <= MAX_LINE_LENGTH
        for match in STRING_LITERAL_RE.finditer(line)
    ]


def _config_flags(submission: common.Submission) -> list[str] | None:
    """Returns the d8 flags from a valid `repro.json`, or None.

    Returns None when there is no valid config or the config targets another
    binary. Callers use None to tell "no config" apart from "no flags".
    """
    if common.config_value(submission, 'binary') != 'd8':
        return None
    return list(common.config_value(submission, 'flags'))


def _has_python_server(submission: common.Submission) -> bool:
    """Returns True if the submission has a Python web server file.

    A server file is `server.py`, or any `.py` file that uses a known server
    library. Such a PoC is served to the browser and belongs to another
    checker.
    """
    return any(
        f.name.endswith('.py')
        and (f.name == 'server.py' or SERVER_RE.search(f.content))
        for f in submission.files
    )


def check_compiles(poc: JsPoc) -> common.Check:
    """Checks that the file is plain JavaScript text that d8 could read.

    Checks only the size limit, an empty file, and pasted Markdown code
    fences or HTML. Never judges JavaScript grammar: invalid code may be the
    bug itself, and d8 reports real syntax errors when it runs.

    Args:
        poc: The PoC file.

    Returns:
        The `compiles` result.
    """
    if poc.oversize:
        return common.Check(
            'compiles',
            common.FAIL,
            f'{poc.name} is {poc.size_bytes} bytes (limit:'
            f' {common.MAX_FILE_BYTES} bytes).',
            fix=f'Keep {poc.name} at or below {common.MAX_FILE_BYTES} bytes.',
            axis=common.AXIS_REPRO,
        )
    if not poc.source.strip():
        return common.Check(
            'compiles',
            common.FAIL,
            f'{poc.name} is empty.',
            fix=f'Provide a valid V8 JavaScript reproduction in {poc.name}.',
            axis=common.AXIS_REPRO,
        )
    first_nonempty = poc.source.lstrip().splitlines()[0]
    if MARKDOWN_FENCE_RE.match(first_nonempty):
        return common.Check(
            'compiles',
            common.FAIL,
            f'{poc.name}:1: pasted Markdown code fences (```).',
            fix='Remove the Markdown code fences (```) from the file.',
            axis=common.AXIS_REPRO,
        )
    if HTML_MARKUP_RE.match(first_nonempty):
        return common.Check(
            'compiles',
            common.FAIL,
            f'{poc.name}:1: HTML markup saved inside a .js file.',
            fix=(
                "Rename HTML reproductions to 'poc.html' or keep pure"
                ' JavaScript.'
            ),
            axis=common.AXIS_REPRO,
        )
    return common.Check(
        'compiles',
        common.PASS,
        'Non-empty plain JavaScript (no Markdown fences or HTML).',
        axis=common.AXIS_REPRO,
    )


def check_deps_available(
    poc: JsPoc, other_attachments: Sequence[str]
) -> common.Check:
    """Fails if the script loads other files.

    d8 gets only the single poc.js file, so `load()`, `read()`,
    `d8.file.execute()`, `require()` or an `import` of another file fails
    there.

    Args:
        poc: The PoC file.
        other_attachments: Names of the other attached files. A string in the
            script that names one of them counts as a file load.

    Returns:
        The `deps_available` result.
    """
    hits: list[int] = []
    overlong_lines: list[int] = []
    for lineno, line in enumerate(poc.code_only.splitlines(), 1):
        if len(line) > MAX_LINE_LENGTH:
            overlong_lines.append(lineno)
            continue
        if FILE_LOADING_CALL_RE.search(line):
            hits.append(lineno)
    # Catches loaders the call pattern misses, such as `new Worker('w.js')`.
    hits += [
        lineno
        for lineno, _ in common.attached_file_references(
            poc.strings, other_attachments
        )
    ]
    if hits:
        return common.Check(
            'deps_available',
            common.FAIL,
            f'External script/file loader at {common.where(poc.name, hits)} (d8'
            ' stages only a single poc.js file).',
            fix=(
                'Inline all helper code (including minimal WasmModuleBuilder'
                ' helpers) directly into poc.js without load()/import/read().'
            ),
            axis=common.AXIS_REPRO,
        )
    if overlong_lines:
        return common.Check(
            'deps_available',
            common.UNKNOWN,
            f'{poc.name}:{overlong_lines[0]}: line exceeds {MAX_LINE_LENGTH}'
            ' bytes; too long to analyze safely.',
            axis=common.AXIS_REPRO,
        )
    return common.Check(
        'deps_available',
        common.PASS,
        'Self-contained single-file script with no external file loaders.',
        axis=common.AXIS_REPRO,
    )


def check_supported_apis(poc: JsPoc) -> common.Check:
    """Fails if the script uses browser-only or disabled d8 features.

    d8 has no web page, so `document`, `window`, `fetch()` and similar do not
    exist. `--run-as-security-poc` turns off some d8 helpers, such as
    `os.system()` and `writeFile()`. A clean result is still `unknown`,
    because a text search cannot prove every global exists in d8.

    Args:
        poc: The PoC file.

    Returns:
        The `supported_apis` result.
    """
    hits = [
        lineno
        for lineno, line in enumerate(poc.code_only.splitlines(), 1)
        if len(line) <= MAX_LINE_LENGTH and DOM_OR_DISABLED_API_RE.search(line)
    ]
    if hits:
        return common.Check(
            'supported_apis',
            common.FAIL,
            'Browser DOM or disabled d8 API at'
            f' {common.where(poc.name, hits)}.',
            fix=(
                'Remove DOM/Web APIs (or move browser reproductions to poc.html'
                ' / server.py) and avoid os.* / writeFile() disabled under'
                ' --run-as-security-poc.'
            ),
            axis=common.AXIS_REPRO,
        )
    return common.Check(
        'supported_apis',
        common.UNKNOWN,
        'No DOM globals or disabled d8 shell calls found; confirm referenced'
        ' globals exist under `d8 --run-as-security-poc`.',
        axis=common.AXIS_REPRO,
        routine=True,
    )


def check_flags_compatible(
    poc: JsPoc, repro_flags: list[str] | None
) -> common.Check:
    """Checks that the security-mode flag comes first in the d8 flags.

    d8 aborts unless `--run-as-security-poc` (or its sandbox variant) is the
    very first argument. Whether the other flags are allowed in security
    mode changes with V8, so the agent settles that question. d8 ignores
    `// Flags:` comments inside poc.js (a V8 test-suite habit), so the
    message reminds the researcher to copy them into "flags".

    Args:
        poc: The PoC file.
        repro_flags: d8 flags from `repro.json`, or None if there is no
            valid d8 config.

    Returns:
        The `js.flags_compatible` result.
    """
    if repro_flags is None:
        note = (
            ' (`// Flags:` comments inside poc.js are ignored by d8; copy'
            ' needed flags to "flags").'
            if MJSUNIT_FLAGS_COMMENT_RE.search(poc.source)
            else '.'
        )
        return common.Check(
            'js.flags_compatible',
            common.UNKNOWN,
            f'Attach a d8 {common.CONFIG_FILE_NAME} to check "flags" against'
            f' `--run-as-security-poc`{note}',
            axis=common.AXIS_REPRO,
        )
    misplaced = [
        (index, flag)
        for index, flag in enumerate(repro_flags)
        if index > 0
        and flag.partition('=')[0].lstrip('-').replace('_', '-')
        in SECURITY_MODE_FLAGS
    ]
    if misplaced:
        index, flag = misplaced[0]
        return common.Check(
            'js.flags_compatible',
            common.FAIL,
            f'{common.excerpt(flag, 60)} is at position {index + 1} in'
            ' "flags"; d8 aborts unless it is the very first argument.',
            fix=f'Move {common.excerpt(flag, 60)} to the start of "flags".',
            axis=common.AXIS_REPRO,
        )
    return common.Check(
        'js.flags_compatible',
        common.UNKNOWN,
        'Confirm every entry in "flags" is accepted by'
        ' `d8 --run-as-security-poc`.',
        axis=common.AXIS_REPRO,
        routine=True,
    )


def check_fake_output(poc: JsPoc) -> common.Check:
    """Flags crash or success text written directly in string literals.

    Hard-coded sanitizer output or a success message can look like a real
    crash. The result is only `unknown`, because a text match cannot tell
    real use from fake output.

    Args:
        poc: The PoC file.

    Returns:
        The `no_fake_output` result.
    """
    hits = [
        line for line, text in poc.strings if common.FAKE_OUTPUT_RE.search(text)
    ]
    if hits:
        return common.Check(
            'no_fake_output',
            common.UNKNOWN,
            f'Crash or success text at {common.where(poc.name, hits)}; confirm'
            ' it is not hard-coded fake output.',
            fix='Remove hard-coded sanitizer output or success messages.',
            axis=common.AXIS_QUALITY,
        )
    return common.Check(
        'no_fake_output',
        common.UNKNOWN,
        'No sanitizer or success strings found; confirm output reflects real'
        ' execution.',
        axis=common.AXIS_QUALITY,
        routine=True,
    )


def check_environment_independent(poc: JsPoc) -> common.Check:
    """Fails if a string literal holds an absolute path from a local machine.

    Such a path does not exist on the test machine, so the script must stay
    self-contained.

    Args:
        poc: The PoC file.

    Returns:
        The `js.environment_independent` result.
    """
    abs_lines = [
        line
        for line, text in poc.strings
        if common.ABSOLUTE_PATH_RE.match(text)
    ]
    if abs_lines:
        return common.Check(
            'js.environment_independent',
            common.FAIL,
            f'Hard-coded host path at {common.where(poc.name, abs_lines)}.',
            fix='Remove local machine paths; keep the script self-contained.',
            axis=common.AXIS_REPRO,
        )
    return common.Check(
        'js.environment_independent',
        common.PASS,
        'No hard-coded host paths found.',
        axis=common.AXIS_REPRO,
    )


def check(submission: common.Submission) -> list[common.Check]:
    """Runs all poc.js checks and returns them in a fixed order.

    Returns an empty list when another checker owns the submission: when it
    has a Python server or an HTML file, or a `manifest.json` without a
    `poc.js`. The main file is the `poc` named in `repro.json`, else
    `poc.js`, else the first `.js` file.

    Args:
        submission: The report and its attached files.

    Returns:
        The check results, sorted by `CHECK_ORDER`.
    """
    if _has_python_server(submission) or submission.files_by_suffix(
        '.html', '.htm'
    ):
        return []
    lower_names = {f.name.lower() for f in submission.files}
    exact_names = {f.name for f in submission.files}
    if 'manifest.json' in lower_names and REQUIRED_NAME not in exact_names:
        return []

    js_files = submission.files_by_suffix('.js', '.mjs')
    if not js_files:
        return []
    config_poc = common.config_value(submission, 'poc')
    primary = next(
        (item for item in js_files if item.name == config_poc),
        next(
            (item for item in js_files if item.name == REQUIRED_NAME),
            js_files[0],
        ),
    )
    poc = JsPoc(
        name=primary.name,
        path=primary.path,
        source=primary.content,
        oversize=primary.oversize,
        size_bytes=primary.size_bytes,
    )
    compiles_check = check_compiles(poc)
    checks = [compiles_check]
    # A file that is not plain JavaScript text gives the other checks nothing
    # meaningful to search, so they all stay `unknown`.
    if compiles_check.status == common.FAIL:
        checks += [
            common.Check(
                id_,
                common.UNKNOWN,
                'Cannot check until the file compiles.',
                axis=CHECK_AXES[id_],
            )
            for id_ in CHECK_ORDER
            if id_ != 'compiles'
        ]
    else:
        poc.code_only = common.strip_js_comments_and_strings(poc.source)
        poc.strings = _string_literals(poc.source)
        checks += [
            check_deps_available(
                poc,
                [
                    f.name
                    for f in submission.files
                    if f.name != poc.name
                    and not common.is_metadata_file(f.name)
                ],
            ),
            check_supported_apis(poc),
            check_fake_output(poc),
            check_flags_compatible(poc, _config_flags(submission)),
            check_environment_independent(poc),
        ]
    checks.sort(key=lambda c: CHECK_ORDER.index(c.id))
    return checks


def main(argv: Sequence[str] | None = None) -> int:
    """Runs the poc.js checks from the command line.

    Returns:
        The process exit code.
    """
    return common.run_cli(check, argv=argv, description=__doc__)


if __name__ == '__main__':
    sys.exit(main())
