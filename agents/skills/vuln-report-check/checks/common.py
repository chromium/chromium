# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Shared helpers that every check uses.

Reads the bug report, the attached files and `repro.json`, and turns the
results of all checks into the final answers and printed text. Uses only
Python's standard library, so it runs on any machine.
"""

import argparse
import base64
import binascii
from collections.abc import Callable, Iterable, Iterator, Mapping, Sequence
import dataclasses
import datetime
import json
import os
import re
import sys
from typing import Any

# Alias both top-level `common` (standalone `python3 checks/foo.py`) and
# package-qualified `checks.common` (`python3 check.py`) to one module object so
# `Check` / `Submission` class identities never split across imports.
sys.modules.setdefault('common', sys.modules[__name__])
sys.modules.setdefault('checks.common', sys.modules[__name__])

MIN_PYTHON = (3, 10)
VERSION = '0.9.2'
# Largest attachment size the checker reads. Bigger files are cut off and
# flagged as oversize, so a huge file cannot slow the checker down.
MAX_FILE_BYTES = 1_000_000
# Logs, crash traces and long explanations belong in attachments. Length is
# counted in UTF-16 code units, the way web forms and the issue tracker count
# characters.
REPORT_MAX_CHARS = 8_000
# UTF-8 needs at most 3 bytes per UTF-16 code unit, so reading this many bytes
# always covers a report that fits the character limit.
_REPORT_MAX_READ_BYTES = 3 * REPORT_MAX_CHARS
EXPIRY_MARGIN = datetime.timedelta(days=7)

# Check statuses.
PASS = 'pass'
FAIL = 'fail'
UNKNOWN = 'unknown'
INFO = 'info'
STATUSES = (PASS, FAIL, UNKNOWN, INFO)

# Verdict axes.
AXIS_QUALITY = 'report_quality'
AXIS_REPRO = 'auto_repro'
AXES = (AXIS_QUALITY, AXIS_REPRO)

# Two-axis verdict values.
QUALITY_OK = 'OK'
QUALITY_NOT_OK = 'NOT_OK'

REPRO_READY = 'READY'
REPRO_NOT_READY = 'NOT_READY'
REPRO_MANUAL = 'MANUAL'
# No auto-repro check failed, but at least one is still `unknown`, so the
# script alone cannot promise automated reproduction. The agent (or `--smoke`)
# must settle every open item before telling the researcher `READY`.
REPRO_NEEDS_JUDGMENT = 'NEEDS_JUDGMENT'

# Checks whose `unknown` result means the whole file could not be analyzed, so
# every other result for it is unreliable. These route `auto_repro` to `MANUAL`.
# Any other `unknown` (including one check skipping an overlong line) means
# only that single question is open and yields `NEEDS_JUDGMENT`.
ANALYSIS_BLOCKING_CHECK_IDS = frozenset({'compiles'})
INTERNAL_CHECK_PREFIX = 'internal.'

# Structured field states.
VALUE = 'VALUE'
NONE = 'NONE'
MISSING = 'MISSING'
INVALID = 'INVALID'
CONFLICT = 'CONFLICT'
FIELD_STATES = (VALUE, NONE, MISSING, INVALID, CONFLICT)

# `repro.json` (version 1) tells the test system how to run the PoC. Every key
# in `CONFIG_KEYS` must be present; `null` means "none". Attaching
# `vrp-flag.json` marks a VRP flag report; it needs no config key.
CONFIG_FILE_NAME = 'repro.json'
VRP_FLAG_FILE_NAME = 'vrp-flag.json'
CONFIG_VERSION = 1
# Every key is defined in `references/repro_config.md`. The order here is also
# the order of the suggested template.
CONFIG_KEYS = (
    'version',
    'environment',
    'binary',
    'build',
    'flags',
    'poc',
    'server',
    'patch',
    'proposed_fix',
    'operating_system',
    'arch',
    'chrome_version',
    'crash_log',
    'preconditions',
    'user_interaction',
)
# Files that carry metadata for the checker, never PoC content.
METADATA_FILE_NAMES = frozenset({CONFIG_FILE_NAME, VRP_FLAG_FILE_NAME})
DESKTOP_ENVIRONMENTS = ('chrome-desktop', 'd8')
MANUAL_ENVIRONMENTS = (
    'content_shell',
    'chrome-android',
    'android-webview',
    'chrome-ios',
    'chromeos',
    'chrome-extension',
    'fuzzer',
)
ENVIRONMENTS = DESKTOP_ENVIRONMENTS + MANUAL_ENVIRONMENTS

# Shared detection patterns used across PoC and report checks.
FAKE_OUTPUT_RE = re.compile(
    r'AddressSanitizer|(?:ERROR|SUMMARY): \w*Sanitizer|heap-use-after-free|'
    r'heap-buffer-overflow|stack-buffer-overflow|SEGV on unknown address|'
    r'crash (?:was )?reproduced|exploit (?:succeeded|successful|worked)|'
    r'vulnerability (?:confirmed|triggered|reproduced)|'
    r'\[\+\]\s*(?:success|exploit|crash)',
    re.I,
)
INTERACTION_RE = re.compile(
    r'addEventListener\(\s*[\'"](?:click|dblclick|mousedown|mouseup|'
    r'pointerdown|pointerup|touchstart|touchend|keydown|keyup|keypress)|'
    r'\bon(?:click|dblclick|mousedown|pointerdown|touchstart|keydown)\s*=|'
    r'userActivation|requestPermission|getUserMedia|show\w*FilePicker|'
    r'requestFullscreen|location\.(?:hash|search)|\bprompt\(|\bconfirm\('
)
ABSOLUTE_PATH_RE = re.compile(
    r'^(?:[A-Za-z]:[\\/]|/(?:home|Users|usr|opt|root|mnt|tmp|var|src|'
    r'chromium\w*)/)'
)
# Matches JS comments and string literals. Unclosed strings stop at the end of
# the line, and an unclosed block comment stops at the end of the text, so a
# broken file cannot make one match run away.
_JS_COMMENT_OR_STRING_RE = re.compile(
    r'//[^\n]*'
    r'|/\*[\s\S]*?(?:\*/|\Z)'
    r'|"(?:\\[\s\S]|[^"\\\n])*"?'
    r"|'(?:\\[\s\S]|[^'\\\n])*'?"
    r'|`(?:\\[\s\S]|[^`\\])*`?'
)

MANUAL_SUGGESTION = (
    'If this also reproduces in desktop `chrome` or `d8` on Linux, describe'
    ' that setup in repro.json so it gets verified automatically and'
    ' faster.'
)


@dataclasses.dataclass
class Check:
    """One result from one check: an id, a status and a message.

    Attributes:
      axis: Which final answer this result counts toward (report quality or
        auto-repro).
      manual: True if this result sends the PoC to manual (human)
        verification.
      routine: True for a simple open item that the AI agent reviews later,
        so it is counted instead of listed.
    """

    id: str
    status: str
    message: str
    fix: str = ''
    axis: str = AXIS_QUALITY
    manual: bool = False
    routine: bool = False

    def to_dict(self) -> dict[str, str]:
        """Turns this check into a plain dict for the JSON output.

        Only the public fields are included. `manual` and `routine` are
        internal flags used to compute the verdict, so they are left out.
        """
        return {
            'id': self.id,
            'status': self.status,
            'axis': self.axis,
            'message': self.message,
            'fix': self.fix,
        }


@dataclasses.dataclass
class FileInput:
    """One attachment file passed on the command line."""

    path: str
    name: str
    content: str
    size_bytes: int
    oversize: bool = False


@dataclasses.dataclass(frozen=True)
class ReproConfig:
    """A parsed `repro.json` with per-key JSON type validation.

    Attributes:
      raw: The whole top-level JSON object (empty when `error` is set).
      values: Required keys whose JSON type is correct, keyed by name.
      missing: Required keys absent from the file.
      wrong_type: Required keys present with the wrong JSON type.
      problems: Human-readable reasons the file is not a valid version 1 config.
      error: Set when the file is not a JSON object at all.
    """

    raw: Mapping[str, Any]
    values: Mapping[str, Any]
    missing: tuple[str, ...] = ()
    wrong_type: tuple[str, ...] = ()
    problems: tuple[str, ...] = ()
    error: str | None = None

    @property
    def valid(self) -> bool:
        """Returns whether the config is a JSON object with no problems."""
        return self.error is None and not self.problems


@dataclasses.dataclass
class Submission:
    """Normalized report and attachment inputs shared by all check modules."""

    report: str | None = None
    report_source: str | None = None
    report_oversize: bool = False
    files: list[FileInput] = dataclasses.field(default_factory=list)
    smoke: bool = False
    today: datetime.datetime = dataclasses.field(
        default_factory=lambda: datetime.datetime.now(datetime.timezone.utc)
    )
    fields: dict[str, dict[str, Any]] = dataclasses.field(default_factory=dict)
    suggested_repro_config: dict[str, Any] | None = None
    # Parsed config per attachment path, so every module shares one parse.
    repro_config_cache: dict[str, tuple[str, ReproConfig]] = dataclasses.field(
        default_factory=dict, repr=False, compare=False
    )

    @property
    def attachments(self) -> dict[str, str]:
        """Maps each attached file name to its text content."""
        return {item.name: item.content for item in self.files}

    @property
    def attachment_names(self) -> list[str]:
        """Lists the names of the attached files, in command-line order."""
        return [item.name for item in self.files]

    @property
    def attachment_paths(self) -> list[str]:
        """Lists the paths of the attached files, in command-line order."""
        return [item.path for item in self.files]

    def files_by_suffix(self, *suffixes: str) -> list[FileInput]:
        """Returns the attached files whose names end with any of `suffixes`.

        The match ignores upper and lower case, so `POC.HTML` counts as `.html`.
        """
        lowered = tuple(s.lower() for s in suffixes)
        return [f for f in self.files if f.name.lower().endswith(lowered)]

    def set_field(self, key: str, state: str, value: Any = None) -> None:
        """Saves what the checker found for one report field.

        `state` says whether the field had a value, said "none", was missing,
        was invalid or conflicted (`VALUE`, `NONE`, `MISSING`, etc.).
        """
        self.fields[key] = {'state': state, 'value': value}

    @classmethod
    def from_inputs(
        cls,
        file_paths: Sequence[str] = (),
        *,
        report_path: str | None = None,
        stdin: Any = None,
        smoke: bool = False,
        today: datetime.datetime | None = None,
        read_fn: Callable[[str], FileInput] | None = None,
    ) -> 'Submission':
        """Reads the report and the attached files into a new `Submission`.

        The report can come from a file or from stdin (when the path is `-`).
        Reading is capped, so a very long report is cut off and marked as
        oversize instead of being loaded in full. Files are only read, never
        run.
        """
        reader = read_fn or read_file_input
        report_text: str | None = None
        report_source: str | None = None
        report_oversize = False
        if report_path is not None:
            if report_path == '-':
                stream = stdin if stdin is not None else sys.stdin
                # Read raw bytes when possible so the byte cap is exact. One
                # extra byte is read so a too-long report can be detected.
                raw_buf = getattr(stream, 'buffer', None)
                if raw_buf is not None:
                    raw_text = raw_buf.read(_REPORT_MAX_READ_BYTES + 1).decode(
                        'utf-8', errors='replace'
                    )
                else:
                    raw_text = stream.read(_REPORT_MAX_READ_BYTES + 1)
                report_text, report_oversize = _bounded_report_text(raw_text)
                report_source = '<stdin>'
            else:
                report_file = (
                    read_fn(report_path)
                    if read_fn is not None
                    else read_file_input(
                        report_path, max_bytes=_REPORT_MAX_READ_BYTES + 1
                    )
                )
                report_text, report_oversize = _bounded_report_text(
                    report_file.content
                )
                report_source = os.path.basename(report_path)
        files = [reader(path) for path in file_paths]
        kwargs: dict[str, Any] = {
            'report': report_text,
            'report_source': report_source,
            'report_oversize': report_oversize,
            'files': files,
            'smoke': smoke,
        }
        if today is not None:
            kwargs['today'] = today
        return cls(**kwargs)


def _utf16_length(text: str) -> int:
    """Returns the length of `text` in UTF-16 code units."""
    return len(text.encode('utf-16-le', errors='surrogatepass')) // 2


def _bounded_report_text(text: str) -> tuple[str, bool]:
    """Cuts the report down to `REPORT_MAX_CHARS` if it is too long.

    Length is counted in UTF-16 code units, so an emoji or other character
    outside the basic range counts as two, as it does in web forms.

    Returns:
      `(text, too_long)`: the text (cut if needed) and whether it was too long.
    """
    if _utf16_length(text) <= REPORT_MAX_CHARS:
        return text, False
    return text[:REPORT_MAX_CHARS], True


def read_file_input(path: str, max_bytes: int = MAX_FILE_BYTES) -> FileInput:
    """Reads a file as text, keeping at most `max_bytes` bytes.

    A file that is too big is not an error: it is cut off and marked
    `oversize` so a check can report it. Bytes that are not valid UTF-8 are
    replaced instead of raising. The file is only read, never run.
    """
    size = os.path.getsize(path)
    oversize = size > max_bytes
    with open(path, 'rb') as handle:
        raw = handle.read(max_bytes)
    content = raw.decode('utf-8', errors='replace')
    return FileInput(
        path=path,
        name=os.path.basename(path),
        content=content,
        size_bytes=size,
        oversize=oversize,
    )


def where(name: str, lines: Iterable[int], limit: int = 3) -> str:
    """Formats line numbers as `file:l1, file:l2 (+N more)`.

    Duplicates are removed and only the first `limit` lines are shown, so
    messages stay short even when a problem appears many times.
    """
    unique = sorted(set(lines))
    shown = ', '.join(f'{name}:{n}' for n in unique[:limit])
    more = len(unique) - limit
    return shown + (f' (+{more} more)' if more > 0 else '')


def excerpt(text: str, limit: int = 120) -> str:
    """Makes a short, safe one-line quote of `text` for messages.

    Newlines become spaces and unprintable characters become `?`, so text
    from a researcher's file cannot break or mess up the printed output.
    The result is at most `limit` characters, ending in `…` if it was cut.
    """
    if limit <= 0:
        return ''
    collapsed = re.sub(r'[\r\n]+', ' ', text)
    # Only look at one character past the limit, so a huge input is not
    # cleaned in full just to be cut off.
    window = collapsed[: limit + 1]
    sanitized = ''.join(ch if ch.isprintable() else '?' for ch in window)
    if len(sanitized) <= limit:
        return sanitized
    return sanitized[: limit - 1] + '…'


def _blank_js_comment_or_string(
    match: re.Match[str], keep_strings: bool = False
) -> str:
    """Blanks out one JS comment or the inside of one JS string.

    Serves as a `re.sub` callback so later scans do not mistake text inside a
    comment or a string for real code. Newlines are kept and string quotes
    stay in place, so line numbers and the shape of the code do not change.

    Args:
      match: One match of `_JS_COMMENT_OR_STRING_RE`.
      keep_strings: True to return string literals unchanged.

    Returns:
      The replacement text for the match.
    """
    token = match.group(0)
    if token.startswith('//'):
        return ''
    if token.startswith('/*'):
        # Keep the comment's newlines so the following lines do not move.
        return '\n' * token.count('\n') or ' '
    if keep_strings:
        return token
    quote = token[0]
    # A string cut off at the end of a line has no closing quote.
    closed = len(token) >= 2 and token[-1] == quote
    inner = token[1:-1] if closed else token[1:]
    blanked = (
        '\n'.join(' ' * len(part) for part in inner.split('\n'))
        if '\n' in inner
        else ' ' * len(inner)
    )
    return f'{quote}{blanked}{quote}' if closed else f'{quote}{blanked}'


def strip_js_comments_and_strings(
    source: str, *, keep_strings: bool = False
) -> str:
    """Blanks JavaScript comments and, by default, string contents.

    Scans that look for code patterns call this first, so text inside a
    comment or a string never counts as code. Every newline is kept, so line
    numbers in the result match the original source. The scan is a single
    bounded regex pass; it does not parse JavaScript and never fails on
    invalid code.

    Args:
      source: JavaScript source text.
      keep_strings: True to blank only comments and leave strings as is.

    Returns:
      The source with comments (and string contents) replaced by blanks.
    """
    return _JS_COMMENT_OR_STRING_RE.sub(
        lambda match: _blank_js_comment_or_string(match, keep_strings), source
    )


def _config_type_ok(key: str, value: Any) -> bool:
    """Checks that a `repro.json` value has the allowed JSON type.

    Python treats `True`/`False` as numbers, so booleans are rejected
    explicitly where a number is expected (`version`, `server.port`).
    """
    if key == 'version':
        return isinstance(value, int) and not isinstance(value, bool)
    if key in ('environment', 'build', 'arch', 'chrome_version'):
        return isinstance(value, str)
    if key == 'flags':
        return isinstance(value, list) and all(
            isinstance(v, str) for v in value
        )
    if key == 'operating_system':
        return (
            isinstance(value, list)
            and bool(value)
            and all(isinstance(v, str) for v in value)
        )
    if key == 'server':
        if value is None:
            return True
        return (
            isinstance(value, dict)
            and isinstance(value.get('file'), str)
            and isinstance(value.get('port'), int)
            and not isinstance(value.get('port'), bool)
        )
    # Nullable strings: binary, poc, patch, proposed_fix, crash_log,
    # preconditions, user_interaction.
    return value is None or isinstance(value, str)


def parse_json_object(text: str) -> tuple[dict[str, Any] | None, str | None]:
    """Parses JSON text from an attachment and checks it is a JSON object.

    The text comes from the researcher, so errors never raise: very deep
    nesting (which would crash Python's parser) and bad JSON both become a
    short reason string instead.

    Args:
      text: Attachment content, already capped at `MAX_FILE_BYTES` on read.

    Returns:
      `(object, None)` on success, or `(None, reason)` when the text is not
      JSON, nests too deeply, or is not a JSON object.
    """
    try:
        data = json.loads(text)
    except RecursionError:
        return None, 'JSON nests too deeply to parse'
    except ValueError as exc:
        return None, f'not valid JSON ({excerpt(str(exc), 80)})'
    if not isinstance(data, dict):
        return (
            None,
            f'top level must be a JSON object, got {type(data).__name__}',
        )
    return data, None


def load_repro_config(submission: Submission) -> ReproConfig | None:
    """Reads and checks the attached `repro.json`, if there is one.

    The result is cached on `submission` so every check uses the same parse.
    Hostile input is limited: attachments are capped at `MAX_FILE_BYTES` on
    read, oversize files are rejected, and deep nesting is caught.

    Args:
      submission: The submission whose attachments may include the config.

    Returns:
      A `ReproConfig` (check `.valid`), or None when no config is attached.
    """
    item = next(
        (f for f in submission.files if f.name == CONFIG_FILE_NAME), None
    )
    if item is None:
        return None
    # Reuse the cached parse only if the file content is still the same.
    cached = submission.repro_config_cache.get(item.path)
    if cached is not None and cached[0] == item.content:
        return cached[1]
    if item.oversize or item.size_bytes > MAX_FILE_BYTES:
        config = ReproConfig(
            raw={},
            values={},
            error=f'{CONFIG_FILE_NAME} exceeds {MAX_FILE_BYTES} bytes',
        )
    else:
        data, error = parse_json_object(item.content)
        if data is None:
            config = ReproConfig(raw={}, values={}, error=error)
        else:
            missing = tuple(k for k in CONFIG_KEYS if k not in data)
            wrong_type = tuple(
                k
                for k in CONFIG_KEYS
                if k in data and not _config_type_ok(k, data[k])
            )
            problems: list[str] = []
            if missing:
                problems.append(f'missing key(s): {", ".join(missing)}')
            if wrong_type:
                problems.append(f'wrong type for: {", ".join(wrong_type)}')
            version = data.get('version')
            # Only complain about the version number if the key exists and
            # has the right type; otherwise it is already reported above.
            if (
                'version' not in missing + wrong_type
                and version != CONFIG_VERSION
            ):
                problems.append(
                    f'unsupported version {excerpt(str(version), 20)} (expected'
                    f' {CONFIG_VERSION})'
                )
            config = ReproConfig(
                raw=data,
                values={
                    k: data[k]
                    for k in CONFIG_KEYS
                    if k in data and k not in wrong_type
                },
                missing=missing,
                wrong_type=wrong_type,
                problems=tuple(problems),
            )
    submission.repro_config_cache[item.path] = (item.content, config)
    return config


def config_value(submission: Submission, key: str) -> Any:
    """Returns one value from `repro.json`, or None.

    Values are only used when the whole config is valid, so checks never act
    on a half-broken file.
    """
    config = load_repro_config(submission)
    if config is None or not config.valid:
        return None
    return config.values.get(key)


def is_metadata_file(name: str) -> bool:
    """Returns whether a file is checker metadata rather than part of the PoC.

    `repro.json` and `vrp-flag.json` describe the submission, so checks
    that look at PoC files skip them.
    """
    return name in METADATA_FILE_NAMES


def attached_file_references(
    strings: Iterable[tuple[int, str]], names: Sequence[str]
) -> list[tuple[int, str]]:
    """Finds string literals in the PoC that name another attached file.

    Chrome's automated reproduction system runs the PoC file alone. So a PoC
    that loads another attachment at runtime (`s.src = './helper.js'`, a
    `Worker`, markup passed to `document.write`) breaks, even though that
    file was attached.

    Args:
      strings: `(line, text)` string literals from the PoC source.
      names: Other attachment names. The PoC's own name must be excluded.

    Returns:
      One `(line, name)` per literal that mentions an attachment.
    """
    # The name must stand alone: `helper.js` should not match inside
    # `my-helper.js` or `helper.json`.
    patterns = [
        (name, re.compile(rf'(?<![\w.-]){re.escape(name)}(?![\w-])'))
        for name in names
    ]
    hits: list[tuple[int, str]] = []
    for line, text in strings:
        for name, pattern in patterns:
            if pattern.search(text):
                hits.append((line, name))
                break
    return hits


def origin_trial_expiries(text: str) -> Iterator[datetime.datetime]:
    """Finds Origin Trial tokens in `text` and yields their expiry times (UTC).

    An Origin Trial token turns on an experimental Chrome feature for a site
    until it expires. Tokens are found by looking for long base64 strings and
    decoding them. Anything that does not decode as a token is skipped.
    """
    # Only long base64 runs can be tokens; shorter ones are ignored.
    for match in re.finditer(r'[A-Za-z0-9+/]{100,}={0,2}', text):
        try:
            raw = base64.b64decode(match.group(0))
        except (binascii.Error, ValueError):
            continue
        # Token layout, as read here: 1 version byte (2 or 3), a 64-byte
        # signature, a 4-byte big-endian payload length, then JSON payload.
        if len(raw) < 69 or raw[0] not in (2, 3):
            continue
        size = int.from_bytes(raw[65:69], 'big')
        try:
            payload = json.loads(raw[69 : 69 + size])
            yield datetime.datetime.fromtimestamp(
                int(payload['expiry']), datetime.timezone.utc
            )
        except (ValueError, KeyError, TypeError, OverflowError):
            continue


def compute_verdict(
    submission: Submission, checks: Sequence[Check]
) -> dict[str, str]:
    """Decides the two final answers: report quality and auto-repro status.

    The two answers are independent. `report_quality` is `NOT_OK` if any
    report-quality check failed. `auto_repro` says whether Chrome's automated
    reproduction system can likely run the PoC: `MANUAL`, `NOT_READY`,
    `NEEDS_JUDGMENT` or `READY`. The order of the tests below sets which
    answer wins when several apply.
    """
    del submission  # Reserved for submission-level overrides while keeping API.
    has_quality_fail = any(
        c.axis == AXIS_QUALITY and c.status == FAIL for c in checks
    )
    report_quality = QUALITY_NOT_OK if has_quality_fail else QUALITY_OK

    has_manual = any(c.manual for c in checks)
    has_repro_checks = any(c.axis == AXIS_REPRO for c in checks)
    has_repro_fail = any(
        c.axis == AXIS_REPRO and c.status == FAIL for c in checks
    )
    # A checker crash (`internal.*`) or a file that could not be analyzed at
    # all makes every other result unreliable, so a human must verify.
    unanalyzable = any(
        c.id.startswith(INTERNAL_CHECK_PREFIX)
        or (c.id in ANALYSIS_BLOCKING_CHECK_IDS and c.status == UNKNOWN)
        for c in checks
    )
    # Any other open question only needs a judgment call, not a human repro.
    has_open_repro_item = any(
        c.axis == AXIS_REPRO and c.status == UNKNOWN for c in checks
    )

    if has_manual or not has_repro_checks:
        auto_repro = REPRO_MANUAL
    elif has_repro_fail:
        auto_repro = REPRO_NOT_READY
    elif unanalyzable:
        auto_repro = REPRO_MANUAL
    elif has_open_repro_item:
        auto_repro = REPRO_NEEDS_JUDGMENT
    else:
        auto_repro = REPRO_READY

    return {
        'report_quality': report_quality,
        'auto_repro': auto_repro,
    }


def build_result(
    submission: Submission, checks: Sequence[Check]
) -> dict[str, Any]:
    """Builds the full result dict: verdict, summary, findings and checks.

    Two checks may be added here because they depend on the whole run, not
    on one module: a too-long report fails `report.size`, and a run where no
    auto-repro check applied is routed to manual verification.
    """
    final_checks = list(checks)
    # A report over the limit is not accepted at all, so it fails both answers.
    # The text is measured here too, so a report passed in directly (not read
    # from a file) is held to the same limit.
    report_too_long = submission.report_oversize or (
        submission.report is not None
        and _utf16_length(submission.report) > REPORT_MAX_CHARS
    )
    if report_too_long and not any(c.id == 'report.size' for c in final_checks):
        source = submission.report_source or '<report>'
        message = (
            f'Report {source} is longer than {REPORT_MAX_CHARS} characters,'
            ' the limit for Chrome VRP reports.'
        )
        fix = (
            f'Shorten the report to at most {REPORT_MAX_CHARS} characters;'
            ' put logs, crash traces and long explanations in attachments.'
        )
        final_checks[0:0] = [
            Check(
                id='report.size',
                status=FAIL,
                axis=AXIS_QUALITY,
                message=message,
                fix=fix,
            ),
            Check(
                id='report.size.repro',
                status=FAIL,
                axis=AXIS_REPRO,
                message=message,
                fix=fix,
            ),
        ]
    # No attached file is a supported PoC type, so nothing can be checked
    # for automated reproduction; tell the researcher it goes to a human.
    if not any(c.manual for c in final_checks) and not any(
        c.axis == AXIS_REPRO for c in final_checks
    ):
        final_checks.append(
            Check(
                id='auto_repro.supported_target',
                status=INFO,
                axis=AXIS_REPRO,
                manual=True,
                message=(
                    'No supported automated PoC entry (server.py, poc.html,'
                    ' poc.js) matched; routed to manual verification.'
                ),
                fix=MANUAL_SUGGESTION,
            )
        )

    verdict = compute_verdict(submission, final_checks)
    failed = [c for c in final_checks if c.status == FAIL]
    unknowns = [c for c in final_checks if c.status == UNKNOWN]
    findings = build_findings(final_checks)

    # Pick one short summary line; the first matching case wins.
    if failed:
        must_fix = [f for f in findings if f['severity'] == 'must_fix']
        steps = ' '.join(
            f'{n}. {f["problem"]} Fix: {f["fix"]}'
            if f['fix']
            else f'{n}. {f["problem"]}'
            for n, f in enumerate(must_fix, 1)
        )
        summary = f'{len(must_fix)} problem(s) to fix: {steps}'
    elif verdict['auto_repro'] == REPRO_MANUAL:
        summary = (
            f'Report quality: {verdict["report_quality"]}; auto-repro: MANUAL'
            ' (routed to human verification).'
        )
    elif verdict['auto_repro'] == REPRO_NEEDS_JUDGMENT:
        open_ids = list(
            dict.fromkeys(c.id for c in unknowns if c.axis == AXIS_REPRO)
        )
        summary = (
            f'Report quality: {verdict["report_quality"]}; auto-repro:'
            f' NEEDS_JUDGMENT ({len(open_ids)} item(s) must be settled before'
            f' auto-repro is READY: {", ".join(open_ids)}).'
        )
    elif unknowns:
        summary = (
            f'No check failed ({len(unknowns)} check(s) unknown; review before'
            ' submitting).'
        )
    else:
        summary = (
            f'Report quality: {verdict["report_quality"]}; auto-repro:'
            f' {verdict["auto_repro"]}.'
        )

    reviewed: list[str] = []
    if submission.report_source is not None:
        reviewed.append(submission.report_source)
    reviewed.extend(submission.attachment_names)

    routine_reviews = count_routine_reviews(final_checks)

    return {
        'verdict': verdict,
        'fields': dict(submission.fields),
        'summary': summary,
        'files_reviewed': reviewed,
        'suggested_repro_config': submission.suggested_repro_config,
        'checker_version': VERSION,
        'findings': findings,
        'routine_reviews': routine_reviews,
        'checks': [c.to_dict() for c in final_checks],
    }


def exit_code_for_result(result: Mapping[str, Any]) -> int:
    """Picks the process exit code for a result.

    Returns `2` if the checker itself hit an internal error, `1` if any check
    failed, and `0` otherwise (including `MANUAL`). Works with or without the
    `checks` list; without it, the `findings` list is used instead.
    """
    if 'checks' in result:
        if any(c['id'].startswith('internal.') for c in result['checks']):
            return 2
        return 1 if any(c['status'] == FAIL for c in result['checks']) else 0
    findings = result.get('findings', ())
    if any(f['id'].startswith('internal.') for f in findings):
        return 2
    return 1 if any(f['severity'] == 'must_fix' for f in findings) else 0


# Worst-to-best order used when two results for the same check are merged.
_STATUS_RANK = {PASS: 0, INFO: 1, UNKNOWN: 2, FAIL: 3}
# A check id ending in `.repro` is the auto-repro copy of the same check, so
# both copies are merged into one finding.
_REPRO_TWIN_SUFFIX = '.repro'


def count_routine_reviews(checks: Sequence[Check]) -> int:
    """Counts open routine items that are left for the AI agent to review.

    Twin checks (`x` and `x.repro`) count once, using the worse status, so
    the same question is not counted twice.
    """
    merged: dict[str, Check] = {}
    for c in checks:
        key = c.id.removesuffix(_REPRO_TWIN_SUFFIX)
        curr = merged.get(key)
        if curr is None or _STATUS_RANK.get(c.status, 0) > _STATUS_RANK.get(
            curr.status, 0
        ):
            merged[key] = c
    return sum(1 for c in merged.values() if c.status == UNKNOWN and c.routine)


def build_findings(checks: Sequence[Check]) -> list[dict[str, Any]]:
    """Turns check results into a short list of findings for the researcher.

    Twin checks (`x` and `x.repro`) are merged into one finding with the
    worse status, and `affects` lists every axis where it did not pass.
    Findings are ordered must_fix, then needs_look, then tip. Passed checks
    and routine open items (left for the AI agent) are left out.
    """
    merged_items: dict[str, Check] = {}
    merged_axes: dict[str, list[str]] = {}

    for c in checks:
        key = c.id.removesuffix(_REPRO_TWIN_SUFFIX)
        curr = merged_items.get(key)
        if curr is None or _STATUS_RANK.get(c.status, 0) > _STATUS_RANK.get(
            curr.status, 0
        ):
            merged_items[key] = c
        if c.status != PASS:
            axes_list = merged_axes.setdefault(key, [])
            if c.axis not in axes_list:
                axes_list.append(c.axis)

    must_fix: list[dict[str, Any]] = []
    needs_look: list[dict[str, Any]] = []
    tips: list[dict[str, Any]] = []

    for key, item in merged_items.items():
        if item.status == PASS:
            continue
        affects = merged_axes.get(key, [item.axis])
        if item.status == FAIL:
            must_fix.append(
                {
                    'id': key,
                    'severity': 'must_fix',
                    'affects': affects,
                    'problem': item.message,
                    'fix': item.fix,
                }
            )
        elif item.status == UNKNOWN:
            if item.routine:
                continue
            needs_look.append(
                {
                    'id': key,
                    'severity': 'needs_look',
                    'affects': affects,
                    'problem': item.message,
                    'fix': item.fix,
                }
            )
        elif item.status == INFO:
            tips.append(
                {
                    'id': key,
                    'severity': 'tip',
                    'affects': affects,
                    'problem': item.message,
                    'fix': item.fix,
                }
            )

    return must_fix + needs_look + tips


def format_text(result: Mapping[str, Any], *, verbose: bool = False) -> str:
    """Turns a result dict into the text printed in the terminal.

    Problems and tips come only from `findings`, so the text always matches
    the JSON output. Passed checks are just counted unless `verbose` is set.
    """
    findings = result.get('findings', ())
    must_fix = [f for f in findings if f['severity'] == 'must_fix']
    needs_look = [f for f in findings if f['severity'] == 'needs_look']
    tips = [f for f in findings if f['severity'] == 'tip']
    routine_count = int(result.get('routine_reviews', 0))

    verdict = result['verdict']
    lines = [
        (
            f'Report quality: {verdict["report_quality"]} | Auto-repro:'
            f' {verdict["auto_repro"]}'
        ),
        f'Checked: {", ".join(result.get("files_reviewed", ())) or "none"}',
    ]
    if must_fix:
        lines += ['', f'Must fix ({len(must_fix)}):']
        for number, item in enumerate(must_fix, 1):
            lines.append(f'  {number}. {item["problem"]}')
            if item['fix']:
                lines.append(f'     Fix: {item["fix"]}')
    if needs_look or routine_count > 0:
        lines += ['', f'Needs a closer look ({len(needs_look)}):']
        lines += [f'  - {item["problem"]}' for item in needs_look]
        if routine_count > 0:
            lines += [
                f'  ({routine_count} routine content checks will be reviewed'
                ' by the AI.)'
            ]
    if tips:
        lines += ['', f'Tips ({len(tips)}):']
        lines += [
            f'  - {item["problem"]} {item["fix"]}'.rstrip() for item in tips
        ]
    if result.get('suggested_repro_config'):
        lines += [
            '',
            f'Attach this {CONFIG_FILE_NAME} (replace every <...>'
            ' placeholder):',
        ]
        lines += [
            f'  {line}'
            for line in json.dumps(
                result['suggested_repro_config'], indent=2
            ).splitlines()
        ]
    checks = result.get('checks', ())
    # Checks may be `Check` objects or plain dicts, so handle both.
    passed = [
        c
        for c in checks
        if (c.status if isinstance(c, Check) else c['status']) == PASS
    ]
    if verbose:
        lines += ['', f'Passed ({len(passed)}):']
        lines += [
            f'  - {c.message if isinstance(c, Check) else c["message"]}'
            for c in passed
        ]
    else:
        noun = 'check' if len(passed) == 1 else 'checks'
        lines += [
            '',
            f'{len(passed)} {noun} passed (run with --verbose to list them).',
        ]
    return '\n'.join(lines)


def run_cli(
    check_fn: Callable[[Submission], list[Check]],
    argv: Sequence[str] | None = None,
    *,
    description: str = 'Checks vulnerability report and PoC inputs.',
    allow_smoke: bool = False,
    positional_report: bool = False,
) -> int:
    """Runs the command-line tool: reads inputs, runs checks, prints results.

    Used both by each single check script and by the main script that runs
    all checks. Prints text, or JSON with `--json`. Files are only read; no
    researcher code is run here.

    Returns:
      The exit code: `0` OK, `1` a check failed, `2` the checker could not
      run (old Python, unreadable input, or an internal error).
    """
    if sys.version_info < MIN_PYTHON:
        sys.stderr.write(
            f'Python {".".join(map(str, MIN_PYTHON))}+ is required.\n'
        )
        return 2

    first_line = (description or 'Checks inputs.').strip().splitlines()[0]
    parser = argparse.ArgumentParser(description=first_line)
    parser.add_argument(
        '--report',
        default=None,
        help='path to report Markdown/text file, or - for stdin',
    )
    parser.add_argument('files', nargs='*', help='attached PoC and asset files')
    parser.add_argument('--json', action='store_true', help='print JSON output')
    parser.add_argument(
        '--verbose',
        action='store_true',
        help='list passed checks in text output',
    )
    if allow_smoke:
        parser.add_argument(
            '--smoke',
            action='store_true',
            help='run opt-in local smoke check where supported',
        )
    args = parser.parse_args(argv)

    report_path = args.report
    file_paths = list(args.files)
    # Some scripts take the report as the first plain argument instead of
    # through `--report`.
    if positional_report and report_path is None and file_paths:
        report_path = file_paths.pop(0)

    if report_path is None and not file_paths:
        parser.error('provide --report <file|-> and/or at least one attachment')

    missing = [
        p
        for p in ([report_path] if report_path and report_path != '-' else [])
        + file_paths
        if not os.path.isfile(p)
    ]
    if missing:
        parser.error(f'not a file: {", ".join(missing)}')

    try:
        submission = Submission.from_inputs(
            file_paths,
            report_path=report_path,
            smoke=bool(getattr(args, 'smoke', False)),
        )
    except OSError as exc:
        # Report a read error as a clean message and exit code 2, not a crash.
        sys.stderr.write(f'error: cannot read input: {exc}\n')
        return 2
    checks = check_fn(submission)
    result = build_result(submission, checks)
    exit_code = exit_code_for_result(result)
    if args.json:
        output_dict = dict(result)
        if not args.verbose:
            output_dict.pop('checks', None)
        print(json.dumps(output_dict, indent=2))
    else:
        print(format_text(result, verbose=args.verbose))
    return exit_code
