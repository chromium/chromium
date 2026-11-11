# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Checks a test web page (`poc.html`) that shows the bug in Chrome.

Chrome's test system opens the page as a local file with no internet. Reads
the page, without running it, and checks that it works that way: it needs no
other files, loads nothing from the internet, and uses only real browser
features. Never fails a page for HTML or JavaScript syntax, because broken
markup or script may be the bug itself.
"""

from collections.abc import Sequence
import dataclasses
import datetime
from html import parser as html_parser
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
REQUIRED_NAME = 'poc.html'
# Automated reproduction also runs `index.html`; `poc.html` wins if both exist.
ALTERNATE_NAME = 'index.html'
# Lines longer than this skip the regexes below, because very long lines can
# make regex matching slow. Such lines give "needs judgment" instead.
MAX_LINE_LENGTH = 65_536

CHECK_ORDER = (
    'compiles',
    'deps_available',
    'supported_apis',
    'no_fake_output',
    'html.no_external_network',
    'html.file_origin_safe',
    'html.mojojs_flag',
    'html.no_user_interaction',
    'html.environment_independent',
)

CHECK_AXES = {
    'compiles': common.AXIS_REPRO,
    'deps_available': common.AXIS_REPRO,
    'supported_apis': common.AXIS_REPRO,
    'no_fake_output': common.AXIS_QUALITY,
    'html.no_external_network': common.AXIS_REPRO,
    'html.file_origin_safe': common.AXIS_REPRO,
    'html.mojojs_flag': common.AXIS_REPRO,
    'html.no_user_interaction': common.AXIS_REPRO,
    'html.environment_independent': common.AXIS_REPRO,
}

# Signs that an attached Python file is a local web server. If one is found,
# the page is served over http:// and a different checker handles it.
SERVER_RE = re.compile(
    r'http\.server|socketserver|HTTPServer|serve_forever|aiohttp|flask|'
    r'run_app|socket\.socket|asyncio\.start_server'
)
MARKDOWN_FENCE_RE = re.compile(r'^\s*`{3,}', re.MULTILINE)
# URLs that go to the internet, including protocol-relative `//host` URLs.
EXTERNAL_SCHEME_RE = re.compile(
    r'^(?:https?|wss?|ftp)://|^//[A-Za-z0-9._-]+', re.IGNORECASE
)
# XML namespace names (for example in `xmlns` for SVG) look like w3.org URLs,
# but the browser never downloads them, so they are not network use.
W3C_NAMESPACE_RE = re.compile(
    r'^https?://www\.w3\.org/(?:1999|2000|1998|TR)/', re.IGNORECASE
)
# URLs whose content lives inside the page itself, so nothing is loaded.
INLINE_SCHEME_RE = re.compile(
    r'^(?:data:|blob:|about:|javascript:|mailto:|#)', re.IGNORECASE
)
# Paths to Mojo JavaScript bindings. Mojo is Chrome's system for talking
# between its internal processes; pages can only use it with MojoJS enabled.
MOJO_URL_RE = re.compile(
    r'(?:^|/|\b)gen/|\.mojom(?:-lite|-webui|-shared|\.m)?\.js\b|'
    r'mojo/public/js/',
    re.IGNORECASE,
)
MOJO_JS_API_RE = re.compile(
    r'\b(?:Mojo\.(?:bindInterface|createMessagePipe|createDataPipe|'
    r'createSharedBuffer)|MojoHandle|MojoWatcher|mojo\.internal)\b'
)
# The `Mojo` JS global exists only when Blink's MojoJS feature is enabled.
# The reproduction runner merges comma-separated `--enable-blink-features`
# values, so MojoJS may appear anywhere in the list.
MOJOJS_FLAG_RE = re.compile(
    r'--enable-blink-features(?:=|\s+)\S*(?<![A-Za-z0-9])MojoJS'
    r'(?![A-Za-z0-9])'
)
# Globals that exist only in Chrome's web test shell, not in normal Chrome.
HARNESS_ONLY_API_RE = re.compile(
    r'\b(?:testRunner|eventSender|internals|GCController|'
    r'accessibilityController|gamepadController|layoutTestController|'
    r'textInputController)\b'
)
URL_IN_TEXT_RE = re.compile(
    r'(?:https?|wss?|ftp)://[^\s\'"<>`)};]+', re.IGNORECASE
)
# Loopback hosts stay on the reproduction machine, so they are never "external
# network". Subdomain labels may contain JS template parts, including whole
# expressions (`t${n + 1}.localhost`). The lookahead rejects look-alikes such as
# `localhost.evil.example` and userinfo tricks such as `localhost@evil.example`,
# whose real host is on the internet. The `{0,200}` cap keeps matching fast on
# hostile input.
LOOPBACK_URL_RE = re.compile(
    r'(?:(?:https?|wss?|ftp):)?//'
    r'(?:(?:(?:[\w*-]|\$\{[^}\n]{0,200}\})+\.)*localhost'
    r'|127(?:\.\d{1,3}){3}|\[::1\])'
    r'(?![\w.@-])',
    re.IGNORECASE,
)
# `file://` URLs that point into a home folder, temp folder or drive letter on
# the researcher's own machine.
FILE_URL_HOST_PATH_RE = re.compile(
    r'file://(?:/home/|/Users/|/root/|/tmp/|/[A-Za-z]:/)', re.IGNORECASE
)
# JS string literals, matched after comments are removed. Unclosed quoted
# strings do not match, so broken code yields fewer strings, never an error.
JS_STRING_RE = re.compile(
    r'"(?:\\[\s\S]|[^"\\\n])*"'
    r"|'(?:\\[\s\S]|[^'\\\n])*'"
    r'|`(?:\\[\s\S]|[^`\\])*`'
)
# The next patterns find calls that load a URL given as a plain string. Group 2
# is the URL.
FETCH_CALL_RE = re.compile(r'\bfetch\s*\(\s*([\'"`])([^\'"`]*)\1')
XHR_OPEN_RE = re.compile(
    r'\.open\s*\(\s*[\'"][A-Za-z]+[\'"]\s*,\s*([\'"`])([^\'"`]*)\1'
)
SW_REGISTER_RE = re.compile(
    r'\bserviceWorker\s*\.\s*register\s*\(\s*([\'"`])([^\'"`]*)\1'
)
WORKER_CTOR_RE = re.compile(
    r'\bnew\s+(?:Shared)?Worker\s*\(\s*([\'"`])([^\'"`]*)\1'
)
# The `{0,256}` and `{1,2048}` caps keep matching fast on hostile input.
ES_IMPORT_RE = re.compile(
    r'\bimport\b(?:\s*\(|[^;\'"`]{0,256}?\bfrom)?\s*'
    r'([\'"`])([^\'"`]{1,2048})\1'
)
# Storage features that need an http:// origin and fail on a file:// page.
COOKIE_OR_CACHE_ORIGIN_RE = re.compile(
    r'\b(?:document\.cookie|caches\.open|navigator\.storage\.getDirectory)\b'
)
# `<script type=...>` values that hold data, not JavaScript. The browser does
# not run these blocks, so the JavaScript checks skip them.
NON_JS_SCRIPT_TYPES = frozenset(
    {
        'application/json',
        'application/ld+json',
        'importmap',
        'speculationrules',
        'text/plain',
        'text/html',
        'text/template',
        'x-shader/x-vertex',
        'x-shader/x-fragment',
    }
)
# For each tag, the attributes that make the browser load another file.
SUBRESOURCE_ATTRS: dict[str, tuple[str, ...]] = {
    'script': ('src',),
    'link': ('href',),
    'img': ('src',),
    'object': ('data',),
    'embed': ('src',),
    'video': ('src', 'poster'),
    'audio': ('src',),
    'source': ('src',),
}
FRAME_TAGS = frozenset({'iframe', 'frame'})


@dataclasses.dataclass
class ScriptBlock:
    """One inline `<script>` block from the page.

    `lineno` is the line where the block starts in the HTML file, so problems
    found inside the script are reported with real file line numbers.
    """

    lineno: int
    script_type: str
    code: str


@dataclasses.dataclass
class HtmlPoc:
    """The test page being checked, plus what was learned from reading it.

    `tags` and `scripts` start empty and are filled in by `check_compiles`.
    `parse_error` is set when hostile or deeply nested input made the parser
    crash, so no other check can trust what was read.
    """

    name: str
    path: str
    source: str
    all_html_names: list[str]
    oversize: bool = False
    size_bytes: int = 0
    parse_error: str | None = None
    scripts: list[ScriptBlock] = dataclasses.field(default_factory=list)
    tags: list[tuple[int, str, dict[str, str]]] = dataclasses.field(
        default_factory=list
    )


class HtmlCollector(html_parser.HTMLParser):
    """Collects tags and inline scripts, with line numbers.

    Only reads the page text and never runs the researcher's code. Never
    judges whether the markup is valid. Line numbers start at 1 so they match
    what an editor shows.
    """

    def __init__(self) -> None:
        """Sets up empty lists for the parts found while reading."""
        # Keep `&amp;` and similar as written, so the text stays as in the file.
        super().__init__(convert_charrefs=False)
        self.tags: list[tuple[int, str, dict[str, str]]] = []
        self.scripts: list[ScriptBlock] = []
        self._script_start: int | None = None
        self._script_type: str = ''
        self._script_chunks: list[str] = []

    def handle_starttag(
        self, tag: str, attrs: list[tuple[str, str | None]]
    ) -> None:
        """Records an opening tag and starts collecting inline script text.

        Attribute names are lowercased, like the browser does. A `<script>`
        with `src` loads its code from another file, so it has no inline text.
        """
        lineno = self.getpos()[0]
        attr_map = {k.lower(): v or '' for k, v in attrs}
        self.tags.append((lineno, tag.lower(), attr_map))
        if tag.lower() == 'script' and 'src' not in attr_map:
            self._script_start = lineno
            self._script_type = attr_map.get('type', '').strip().lower()
            self._script_chunks = []

    def handle_endtag(self, tag: str) -> None:
        """Saves the collected script text when `</script>` closes a block."""
        if tag.lower() == 'script':
            self._save_script()

    def handle_data(self, data: str) -> None:
        """Adds text to the current script block, if one is open."""
        if self._script_start is not None:
            self._script_chunks.append(data)

    def close(self) -> None:
        """Finishes reading and keeps the text of an unclosed `<script>`.

        An unclosed script tag may be the bug itself, so its text is still
        checked like any other script block.
        """
        super().close()
        self._save_script()

    def _save_script(self) -> None:
        """Stores the open script block, if any, and resets the state."""
        if self._script_start is None:
            return
        self.scripts.append(
            ScriptBlock(
                lineno=self._script_start,
                script_type=self._script_type,
                code=''.join(self._script_chunks),
            )
        )
        self._script_start = None
        self._script_type = ''
        self._script_chunks = []


def _classify_url(url: str) -> str:
    """Returns the kind of a URL, which tells where it would load from.

    The order of tests matters. Inline URLs are checked first, and loopback
    (this machine) before external, so `http://localhost` is not external.

    Returns:
        One of 'inline', 'mojojs', 'loopback', 'external', 'host_path' or
        'local_relative' (a file next to the page).
    """
    cleaned = url.strip()
    if not cleaned or INLINE_SCHEME_RE.match(cleaned):
        return 'inline'
    if MOJO_URL_RE.search(cleaned):
        return 'mojojs'
    if LOOPBACK_URL_RE.match(cleaned):
        return 'loopback'
    if EXTERNAL_SCHEME_RE.match(cleaned):
        return 'external'
    if FILE_URL_HOST_PATH_RE.match(cleaned):
        return 'host_path'
    return 'local_relative'


def _has_python_server(submission: common.Submission) -> bool:
    """Returns whether the submission includes a Python web server file.

    A file named `server.py`, or a `.py` file using common server code, means
    the page is served over http:// instead of opened as a local file.
    """
    return any(
        f.name.endswith('.py')
        and (f.name == 'server.py' or SERVER_RE.search(f.content))
        for f in submission.files
    )


def _executable_scripts(poc: HtmlPoc) -> list[ScriptBlock]:
    """Returns the inline script blocks that the browser runs as JavaScript.

    Blocks with data types such as JSON or shaders are left out, so their
    text is not checked as code.
    """
    return [
        block
        for block in poc.scripts
        if block.script_type not in NON_JS_SCRIPT_TYPES
    ]


def _script_strings(poc: HtmlPoc) -> list[tuple[int, str]]:
    """Returns `(line, text)` for every string literal in the inline scripts.

    Comments are removed first, so quotes inside comments are not counted.
    The text keeps escapes as written; checks only search it for hints.
    """
    strings: list[tuple[int, str]] = []
    for block in _executable_scripts(poc):
        code = common.strip_js_comments_and_strings(
            block.code, keep_strings=True
        )
        # Count newlines between matches, not from the start each time, so
        # a large script stays linear.
        line = block.lineno
        pos = 0
        for match in JS_STRING_RE.finditer(code):
            line += code.count('\n', pos, match.start())
            pos = match.start()
            strings.append((line, match.group(0)[1:-1]))
    return strings


def check_compiles(poc: HtmlPoc) -> common.Check:
    """Checks the page size and reads its tags and inline scripts.

    Runs first and fills in `poc.tags` and `poc.scripts`, which the other
    checks use. Never fails for HTML or JavaScript syntax: an unclosed tag
    or invalid script may be the bug itself. A parser crash on hostile input
    gives "needs judgment" and sets `poc.parse_error`.

    Args:
        poc: The page to read.

    Returns:
        The `compiles` check.
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
    lines = poc.source.lstrip().splitlines()
    # A page that starts with ``` was likely pasted from a chat or Markdown
    # file, and the browser would show the fence as text.
    if lines and MARKDOWN_FENCE_RE.match(lines[0]):
        return common.Check(
            'compiles',
            common.FAIL,
            f'{poc.name}:1: pasted Markdown code fences (```).',
            fix='Remove the Markdown code fences (```) from the HTML file.',
            axis=common.AXIS_REPRO,
        )
    collector = HtmlCollector()
    try:
        collector.feed(poc.source)
        collector.close()
    # Hostile or deeply nested input can exhaust Python's limits, and some
    # broken markup makes Python's HTML parser raise AssertionError. Report
    # that as "needs judgment" instead of crashing the whole tool.
    except (RecursionError, MemoryError, AssertionError) as exc:
        poc.parse_error = (
            f'{poc.name} could not be read by the HTML parser'
            f' ({type(exc).__name__}); skipping static checks.'
        )
        return common.Check(
            'compiles',
            common.UNKNOWN,
            poc.parse_error,
            axis=common.AXIS_REPRO,
        )
    poc.tags = collector.tags
    poc.scripts = collector.scripts
    return common.Check(
        'compiles',
        common.PASS,
        f'{poc.name} read for static checks; HTML and JavaScript syntax are'
        ' not judged.',
        axis=common.AXIS_REPRO,
    )


def check_deps_available(
    poc: HtmlPoc,
    js_strings: Sequence[tuple[int, str]],
    other_attachments: Sequence[str],
) -> common.Check:
    """Checks that the page needs no other attached or local files.

    The standalone setup copies only `poc.html` to the test machine, so any
    other file next to it is missing there. No server runs either, so calls
    to this machine (localhost) get no answer.

    Args:
        poc: The parsed page.
        js_strings: `(line, text)` strings found in inline scripts.
        other_attachments: Names of the other files in the submission.

    Returns:
        FAIL if the page uses a missing file or address, else PASS.
    """
    problems: list[str] = []
    for lineno, tag, attrs in poc.tags:
        # Module scripts are reported by `check_file_origin_safe` instead.
        if (
            tag == 'script'
            and attrs.get('type', '').strip().lower() == 'module'
        ):
            continue
        for attr in SUBRESOURCE_ATTRS.get(tag, ()):
            if attr not in attrs:
                continue
            kind = _classify_url(attrs[attr])
            if kind == 'local_relative':
                problems.append(
                    f'{poc.name}:{lineno} loads local {attrs[attr]!r}'
                )
    # Mojo bindings come from the build's gen/ folder, which every run gets,
    # so a reference to them never needs the attached copy.
    non_mojo_strings = [
        (lineno, text)
        for lineno, text in js_strings
        if not MOJO_URL_RE.search(text)
    ]
    for lineno, name in common.attached_file_references(
        non_mojo_strings, other_attachments
    ):
        problems.append(f'{poc.name}:{lineno} uses attached file {name!r}')
    # Runs only without `server.py`, so nothing listens on the loopback
    # address inside the reproduction VM.
    for lineno, _, attrs in poc.tags:
        for val in attrs.values():
            if _classify_url(val) == 'loopback':
                problems.append(
                    f'{poc.name}:{lineno} calls {val.strip()!r} but no'
                    ' server.py is attached'
                )
    for block in _executable_scripts(poc):
        # Strings stay visible because loopback URLs are usually inside them.
        code = common.strip_js_comments_and_strings(
            block.code, keep_strings=True
        )
        for offset, line in enumerate(code.splitlines()):
            # Skip very long lines to keep regex matching fast.
            if len(line) > MAX_LINE_LENGTH:
                continue
            hit = LOOPBACK_URL_RE.search(line)
            if hit:
                problems.append(
                    f'{poc.name}:{block.lineno + offset} calls'
                    f' {hit.group(0)!r} but no server.py is attached'
                )
    if problems:
        return common.Check(
            'deps_available',
            common.FAIL,
            # Show at most 3 problems to keep the message short.
            '; '.join(problems[:3])
            + ' (standalone HTML stages only poc.html).',
            fix=(
                'Inline all scripts, styles, and media directly into poc.html'
                ' (e.g. inline <script>/<style> or data:/Blob URLs), or switch'
                " to 'server.py' on port 8000."
            ),
            axis=common.AXIS_REPRO,
        )
    return common.Check(
        'deps_available',
        common.PASS,
        'Self-contained HTML with no external local companion files.',
        axis=common.AXIS_REPRO,
    )


def check_no_external_network(poc: HtmlPoc) -> common.Check:
    """Checks that the page loads nothing from the internet.

    The reproduction machine has no internet access, so remote scripts or
    files never load there. Lines too long to scan safely give "needs
    judgment" instead of PASS.
    """
    hits: list[str] = []
    for lineno, _, attrs in poc.tags:
        for attr, val in attrs.items():
            # XML namespace values are names, not downloads.
            if attr == 'xmlns' or attr.startswith('xmlns:'):
                if W3C_NAMESPACE_RE.match(val.strip()):
                    continue
            if _classify_url(val) == 'external':
                hits.append(f'{poc.name}:{lineno} ({val.strip()})')
    overlong_lines: list[int] = []
    for block in _executable_scripts(poc):
        for offset, raw_line in enumerate(block.code.splitlines()):
            lineno = block.lineno + offset
            if len(raw_line) > MAX_LINE_LENGTH:
                overlong_lines.append(lineno)
                continue
            for match in URL_IN_TEXT_RE.finditer(raw_line):
                url = match.group(0)
                if W3C_NAMESPACE_RE.match(url):
                    continue
                # Match on the raw line: `URL_IN_TEXT_RE` stops at `}`, which
                # cuts a templated host like `http://${sub}.localhost` before
                # `localhost`.
                if LOOPBACK_URL_RE.match(raw_line, match.start()):
                    continue
                hits.append(f'{poc.name}:{lineno} ({url})')
    if hits:
        # Drop repeats but keep the original order.
        unique = list(dict.fromkeys(hits))
        return common.Check(
            'html.no_external_network',
            common.FAIL,
            f'External network URL(s) in {", ".join(unique[:3])} (the'
            ' reproduction VM has no internet access).',
            fix=(
                'Inline remote scripts and assets into poc.html, or serve local'
                " HTTP responses with 'server.py' on port 8000."
            ),
            axis=common.AXIS_REPRO,
        )
    if overlong_lines:
        return common.Check(
            'html.no_external_network',
            common.UNKNOWN,
            f'{poc.name}:{overlong_lines[0]}: script line exceeds'
            f' {MAX_LINE_LENGTH} bytes; too long to analyze safely.',
            axis=common.AXIS_REPRO,
        )
    return common.Check(
        'html.no_external_network',
        common.PASS,
        'No external network URLs found.',
        axis=common.AXIS_REPRO,
    )


def check_file_origin_safe(poc: HtmlPoc) -> common.Check:
    """Checks for features that break when Chrome opens the page as a file.

    The test system opens the page as `file:///tmp/poc.html`. On a file://
    page, frames, fetch, workers, module imports, cookies and similar
    features that need a normal http:// origin do not work.
    """
    problems: list[str] = []
    overlong_lines: list[int] = []
    for lineno, tag, attrs in poc.tags:
        if tag in FRAME_TAGS and 'src' in attrs:
            if _classify_url(attrs['src']) == 'local_relative':
                problems.append(
                    f'{poc.name}:{lineno} uses <{tag} src={attrs["src"]!r}>'
                )
        if (
            tag == 'script'
            and attrs.get('type', '').strip().lower() == 'module'
            and 'src' in attrs
            and _classify_url(attrs['src']) == 'local_relative'
        ):
            problems.append(
                f'{poc.name}:{lineno} loads module script {attrs["src"]!r}'
                ' over file://'
            )

    url_patterns = (
        (FETCH_CALL_RE, 'fetch'),
        (XHR_OPEN_RE, 'XMLHttpRequest'),
        (SW_REGISTER_RE, 'serviceWorker.register'),
        (WORKER_CTOR_RE, 'Worker'),
        (ES_IMPORT_RE, 'ES module import'),
    )
    for block in _executable_scripts(poc):
        for offset, raw_line in enumerate(block.code.splitlines()):
            lineno = block.lineno + offset
            if len(raw_line) > MAX_LINE_LENGTH:
                overlong_lines.append(lineno)
                continue
            for regex, label in url_patterns:
                for match in regex.finditer(raw_line):
                    target = match.group(2)
                    if _classify_url(target) == 'local_relative':
                        problems.append(
                            f'{poc.name}:{lineno} uses {label}({target!r})'
                            ' on file:// origin'
                        )
        # Comments and strings are blanked so that, for example, the words
        # "document.cookie" in a comment are not reported.
        stripped = common.strip_js_comments_and_strings(block.code)
        for offset, clean_line in enumerate(stripped.splitlines()):
            if len(clean_line) > MAX_LINE_LENGTH:
                continue
            hit = COOKIE_OR_CACHE_ORIGIN_RE.search(clean_line)
            if hit:
                lineno = block.lineno + offset
                problems.append(
                    f'{poc.name}:{lineno} uses {hit.group(0)} (requires'
                    ' http:// origin)'
                )

    if problems:
        unique = list(dict.fromkeys(problems))
        return common.Check(
            'html.file_origin_safe',
            common.FAIL,
            '; '.join(unique[:3]) + '.',
            fix=(
                "Switch to 'server.py' on port 8000 for an"
                ' http://localhost:8000 origin, or use inline'
                ' <iframe srcdoc=...>, data:, or Blob URLs.'
            ),
            axis=common.AXIS_REPRO,
        )
    if overlong_lines:
        return common.Check(
            'html.file_origin_safe',
            common.UNKNOWN,
            f'{poc.name}:{overlong_lines[0]}: script line exceeds'
            f' {MAX_LINE_LENGTH} bytes; too long to analyze safely.',
            axis=common.AXIS_REPRO,
        )
    return common.Check(
        'html.file_origin_safe',
        common.PASS,
        'No relative fetch/XHR/import/ServiceWorker/iframe dependencies on'
        ' file:// origin.',
        axis=common.AXIS_REPRO,
    )


def _mojojs_lines(poc: HtmlPoc) -> tuple[list[int], list[int]]:
    """Returns the lines where the page uses Mojo.

    Looks for Mojo binding URLs in tag attributes and scripts, and for Mojo
    JavaScript calls outside comments and strings.

    Returns:
        A tuple `(lines using Mojo, script lines too long to scan)`.
    """
    hits: list[int] = []
    overlong_lines: list[int] = []
    for lineno, _, attrs in poc.tags:
        for val in attrs.values():
            if _classify_url(val) == 'mojojs':
                hits.append(lineno)
    for block in _executable_scripts(poc):
        stripped = common.strip_js_comments_and_strings(block.code)
        for offset, line in enumerate(stripped.splitlines()):
            if len(line) > MAX_LINE_LENGTH:
                overlong_lines.append(block.lineno + offset)
                continue
            if MOJO_JS_API_RE.search(line):
                hits.append(block.lineno + offset)
        for offset, raw_line in enumerate(block.code.splitlines()):
            if len(raw_line) > MAX_LINE_LENGTH:
                continue
            if MOJO_URL_RE.search(raw_line):
                hits.append(block.lineno + offset)
    return hits, overlong_lines


def check_mojojs_flag(
    poc: HtmlPoc, flags: Sequence[str] | None
) -> common.Check:
    """Checks that `flags` turn on MojoJS when the page uses Mojo.

    Without `--enable-blink-features=MojoJS` the `Mojo` global does not exist,
    so the page cannot reach the bug.

    Args:
        poc: The parsed page.
        flags: The "flags" list from repro.json, or None when no valid config
            was given, which makes the result "needs judgment".

    Returns:
        PASS when Mojo is unused or enabled, FAIL when used but not enabled.
    """
    hits, overlong_lines = _mojojs_lines(poc)
    if not hits:
        if overlong_lines:
            return common.Check(
                'html.mojojs_flag',
                common.UNKNOWN,
                f'{poc.name}:{overlong_lines[0]}: script line exceeds'
                f' {MAX_LINE_LENGTH} bytes; too long to analyze safely.',
                axis=common.AXIS_REPRO,
            )
        return common.Check(
            'html.mojojs_flag',
            common.PASS,
            'No MojoJS bindings used.',
            axis=common.AXIS_REPRO,
        )
    where = common.where(poc.name, hits)
    if flags is None:
        return common.Check(
            'html.mojojs_flag',
            common.UNKNOWN,
            f'MojoJS used at {where}; confirm "flags" in repro.json includes'
            ' --enable-blink-features=MojoJS.',
            axis=common.AXIS_REPRO,
        )
    if any(MOJOJS_FLAG_RE.search(flag) for flag in flags):
        return common.Check(
            'html.mojojs_flag',
            common.PASS,
            f'MojoJS used at {where} and enabled in "flags".',
            axis=common.AXIS_REPRO,
        )
    return common.Check(
        'html.mojojs_flag',
        common.FAIL,
        f'MojoJS used at {where}, but "flags" in repro.json does not enable'
        ' it.',
        fix=(
            'Add "--enable-blink-features=MojoJS" to "flags"; without it the'
            ' `Mojo` global is undefined.'
        ),
        axis=common.AXIS_REPRO,
    )


def check_supported_apis(poc: HtmlPoc) -> common.Check:
    """Checks that the page uses no web-test-only globals.

    Globals such as `testRunner` or `internals` exist only in Chrome's web
    test shell, not in the normal Chrome build used for reproduction. When
    none are found, the result is still "needs judgment" because flag-gated
    experimental web features cannot be checked here.
    """
    problems: list[str] = []
    for block in _executable_scripts(poc):
        stripped = common.strip_js_comments_and_strings(block.code)
        for offset, line in enumerate(stripped.splitlines()):
            if len(line) > MAX_LINE_LENGTH:
                continue
            match = HARNESS_ONLY_API_RE.search(line)
            if match:
                lineno = block.lineno + offset
                problems.append(
                    f'{poc.name}:{lineno} uses test-shell global'
                    f' {match.group(0)!r}'
                )
    if problems:
        unique = list(dict.fromkeys(problems))
        return common.Check(
            'supported_apis',
            common.FAIL,
            '; '.join(unique[:3]) + '.',
            fix=(
                'Rewrite using standard Web Platform APIs supported in desktop'
                ' chrome (or document content_shell / web_tests for manual'
                ' triage).'
            ),
            axis=common.AXIS_REPRO,
        )
    return common.Check(
        'supported_apis',
        common.UNKNOWN,
        'No layout-test globals found; confirm any experimental Web API'
        ' matches "flags" in repro.json.',
        axis=common.AXIS_REPRO,
        routine=True,
    )


def check_fake_output(
    poc: HtmlPoc, js_strings: Sequence[tuple[int, str]]
) -> common.Check:
    """Checks for crash or success text written into the page itself.

    A page that prints a fake crash report or sanitizer message can mislead
    the reviewer. Matching text may also be harmless, so a hit asks for
    human judgment instead of failing.
    """
    hits = [
        line
        for line, text in js_strings
        if len(text) <= MAX_LINE_LENGTH and common.FAKE_OUTPUT_RE.search(text)
    ]
    for lineno, line in enumerate(poc.source.splitlines(), 1):
        if len(line) <= MAX_LINE_LENGTH and common.FAKE_OUTPUT_RE.search(line):
            hits.append(lineno)
    if hits:
        return common.Check(
            'no_fake_output',
            common.UNKNOWN,
            f'Crash or success text at {common.where(poc.name, hits)}; confirm'
            ' it is not hard-coded fake output.',
            fix='Remove hard-coded crash banners or sanitizer strings.',
            axis=common.AXIS_QUALITY,
        )
    return common.Check(
        'no_fake_output',
        common.UNKNOWN,
        'No sanitizer or success text found; confirm the page does not'
        ' fabricate crash output.',
        axis=common.AXIS_QUALITY,
        routine=True,
    )


def check_user_interaction(poc: HtmlPoc) -> common.Check:
    """Checks for code that needs a person to click, answer or edit the URL.

    The test system loads the page with no one at the keyboard, so the bug
    must trigger on its own. Text matches can be harmless, so hits ask for
    human judgment instead of failing.
    """
    hits: list[str] = []
    overlong_lines: list[int] = []
    for n, line in enumerate(poc.source.splitlines(), 1):
        if len(line) > MAX_LINE_LENGTH:
            overlong_lines.append(n)
            continue
        if common.INTERACTION_RE.search(line):
            hits.append(f'{poc.name}:{n}')
    if hits:
        return common.Check(
            'html.no_user_interaction',
            common.UNKNOWN,
            f'Possible user interaction at {", ".join(hits[:3])}; confirm the'
            ' trigger runs on load with no manual clicks, prompts, or URL'
            ' hash/query.',
            axis=common.AXIS_REPRO,
        )
    if overlong_lines:
        return common.Check(
            'html.no_user_interaction',
            common.UNKNOWN,
            f'{poc.name}:{overlong_lines[0]}: line exceeds {MAX_LINE_LENGTH}'
            ' bytes; confirm no manual interaction is required.',
            axis=common.AXIS_REPRO,
        )
    return common.Check(
        'html.no_user_interaction',
        common.PASS,
        'No manual click, prompt, or URL query/hash patterns found.',
        axis=common.AXIS_REPRO,
    )


def check_environment_independent(
    poc: HtmlPoc,
    js_strings: Sequence[tuple[int, str]],
    today: datetime.datetime,
) -> common.Check:
    """Checks for paths and tokens that only work on the researcher's setup.

    Paths to the researcher's own files do not exist on the test machine.
    Origin Trial tokens (keys that turn on trial Chrome features for a time)
    stop working when they expire, so tokens expiring soon are rejected too.
    """
    problems: list[str] = []
    path_lines = [
        line
        for line, text in js_strings
        if common.ABSOLUTE_PATH_RE.match(text)
        or FILE_URL_HOST_PATH_RE.match(text)
    ]
    for lineno, _, attrs in poc.tags:
        for val in attrs.values():
            if (
                common.ABSOLUTE_PATH_RE.match(val)
                or _classify_url(val) == 'host_path'
            ):
                path_lines.append(lineno)
    if path_lines:
        problems.append(
            'hard-coded local host path at'
            f' {common.where(poc.name, path_lines)}'
        )
    for expiry in common.origin_trial_expiries(poc.source):
        if expiry - today < common.EXPIRY_MARGIN:
            problems.append(f'Origin Trial token expires {expiry:%Y-%m-%d}')
    if problems:
        return common.Check(
            'html.environment_independent',
            common.FAIL,
            '; '.join(problems) + '.',
            fix=(
                'Remove local host paths and replace expired/expiring Origin'
                ' Trial tokens with Chrome runtime flags in repro.json.'
            ),
            axis=common.AXIS_REPRO,
        )
    return common.Check(
        'html.environment_independent',
        common.PASS,
        'No hard-coded host paths or expiring Origin Trial tokens.',
        axis=common.AXIS_REPRO,
    )


def check(submission: common.Submission) -> list[common.Check]:
    """Runs all checks for an HTML test page and returns them in fixed order.

    If a Python server is attached, the page is served over http:// and the
    server checker covers it. Then only the MojoJS flag check runs here, and
    only if the page uses Mojo.

    Args:
        submission: All files of the report.

    Returns:
        The list of checks, or an empty list when there is no HTML file.
    """
    html_files = [
        item
        for item in submission.files_by_suffix('.html', '.htm')
        if not common.is_metadata_file(item.name)
    ]
    if not html_files:
        return []
    config_poc = common.config_value(submission, 'poc')
    flags = common.config_value(submission, 'flags')
    # Pick the page the same way Chrome's automated reproduction system does:
    # the file named in the config, then `poc.html`, then `index.html`, then
    # the first HTML file.
    primary = next(
        (item for item in html_files if item.name == config_poc),
        next(
            (item for item in html_files if item.name == REQUIRED_NAME),
            next(
                (item for item in html_files if item.name == ALTERNATE_NAME),
                html_files[0],
            ),
        ),
    )
    poc = HtmlPoc(
        name=primary.name,
        path=primary.path,
        source=primary.content,
        all_html_names=[item.name for item in html_files],
        oversize=primary.oversize,
        size_bytes=primary.size_bytes,
    )
    if _has_python_server(submission):
        # `poc_py` owns server bundles. A served page that uses Mojo still
        # needs MojoJS in "flags" so humans can reproduce it; pages without
        # Mojo emit nothing here to keep server results unchanged.
        served_compiles = check_compiles(poc)
        if served_compiles.status != common.PASS or not _mojojs_lines(poc)[0]:
            return []
        return [check_mojojs_flag(poc, flags)]
    compiles_check = check_compiles(poc)
    checks = [compiles_check]
    # If the page cannot be read, the other checks would give wrong answers,
    # so they all become "needs judgment" with the reason.
    if compiles_check.status != common.PASS:
        blocked = poc.parse_error or 'Cannot check until the file compiles.'
        checks += [
            common.Check(id_, common.UNKNOWN, blocked, axis=CHECK_AXES[id_])
            for id_ in CHECK_ORDER
            if id_ != 'compiles'
        ]
    else:
        try:
            js_strings = _script_strings(poc)
            checks += [
                check_deps_available(
                    poc,
                    js_strings,
                    [
                        f.name
                        for f in submission.files
                        if f.name != poc.name
                        and not common.is_metadata_file(f.name)
                    ],
                ),
                check_supported_apis(poc),
                check_fake_output(poc, js_strings),
                check_no_external_network(poc),
                check_file_origin_safe(poc),
                check_mojojs_flag(poc, flags),
                check_user_interaction(poc),
                check_environment_independent(
                    poc, js_strings, submission.today
                ),
            ]
        # Hostile input that exhausts Python's limits turns every check into
        # "needs judgment" instead of crashing the tool.
        except (RecursionError, MemoryError):
            checks = [
                common.Check(
                    id_,
                    common.UNKNOWN,
                    'Hostile or deeply nested input exceeded parser limits.',
                    axis=CHECK_AXES[id_],
                )
                for id_ in CHECK_ORDER
            ]
    checks.sort(key=lambda c: CHECK_ORDER.index(c.id))
    return checks


def main(argv: Sequence[str] | None = None) -> int:
    """Runs this checker alone from the command line.

    Returns:
        The process exit code.
    """
    return common.run_cli(check, argv=argv, description=__doc__)


if __name__ == '__main__':
    sys.exit(main())
