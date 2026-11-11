# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Checks a small Python web server (`server.py`) used to show the bug.

Some bugs need a real web server, for example to send special headers. Chrome's
test system starts `server.py` with no extra options and opens a page from it
on port 8000. Reads the file without running it and checks that it will work
that way and does nothing beyond serving pages.
"""

import ast
from collections.abc import Callable, Iterator, Sequence
import dataclasses
import datetime
import importlib
import os
import re
import sys
import types

if __package__:
    from . import common  # pylint: disable=g-import-not-at-top
else:
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    common = importlib.import_module('common')

ORDER = 50
REQUIRED_NAME = 'server.py'
REQUIRED_PORT = 8000
TARGET_PYTHON = (3, 11)
FALLBACK_ENTRY_NAMES = ('poc.html', 'index.html')
HTML_ENTRY_SUFFIXES = ('.html', '.htm')
HTML_SUFFIXES = ('.html', '.htm', '.xhtml', '.svg', '.js', '.mjs', '.xml')

CHECK_ORDER = (
    'compiles',
    'deps_available',
    'supported_apis',
    'no_fake_output',
    'py.file_name',
    'py.server_contract',
    'py.no_logging',
    'py.no_user_interaction',
    'py.environment_independent',
)

CHECK_AXES = {
    'compiles': common.AXIS_REPRO,
    'deps_available': common.AXIS_REPRO,
    'supported_apis': common.AXIS_REPRO,
    'no_fake_output': common.AXIS_QUALITY,
    'py.file_name': common.AXIS_REPRO,
    'py.server_contract': common.AXIS_REPRO,
    'py.no_logging': common.AXIS_REPRO,
    'py.no_user_interaction': common.AXIS_REPRO,
    'py.environment_independent': common.AXIS_REPRO,
}

SERVER_RE = re.compile(
    r'http\.server|socketserver|HTTPServer|serve_forever|aiohttp|flask|'
    r'run_app|socket\.socket|asyncio\.start_server'
)

FORBIDDEN_MODULES = {
    'subprocess': 'starts other programs',
    'ctypes': 'gives native memory access',
    'webbrowser': 'opens a browser',
    'pty': 'starts other programs',
    'selenium': 'drives a browser',
    'playwright': 'drives a browser',
    'pyppeteer': 'drives a browser',
    'pychrome': 'drives a browser through DevTools',
}
FORBIDDEN_OS_CALL_RE = re.compile(
    r'^os\.(system|popen|exec\w*|spawn\w*|posix_spawnp?|startfile)$'
)
FORBIDDEN_STRING_RE = re.compile(
    r'--remote-debugging-(?:port|pipe)|/proc/[^\s]*/mem|devtools/browser'
)
OUTPUT_CALLS = frozenset(
    {
        'traceback.print_exc',
        'traceback.print_exception',
        'traceback.print_stack',
        'pprint.pprint',
    }
)
LOG_FILE_RE = re.compile(r'log|report|result|output', re.I)
LOG_ENDPOINT_RE = re.compile(r'^/(?:log|report|exfil|leak|results?)\b')
# Modules that read command-line arguments.
ARG_MODULES = frozenset({'argparse', 'getopt', 'optparse'})


@dataclasses.dataclass
class Poc:
    """One server script plus the other files submitted with it.

    The parse fields (`tree`, `syntax_error`, `parse_exhausted`) start empty and
    are filled in by `check_poc`. The script is only parsed, never run.
    """

    name: str
    path: str
    source: str
    attachments: dict[str, str]
    attachments_paths: list[str] = dataclasses.field(default_factory=list)
    oversize: bool = False
    size_bytes: int = 0
    tree: ast.Module | None = None
    syntax_error: SyntaxError | None = None
    parse_exhausted: str | None = None


def _require_tree(poc: Poc) -> ast.Module:
    """Returns the parsed syntax tree of the script.

    Checks that read the tree must only run after parsing worked. Raises
    instead of letting a check silently read `None`.

    Raises:
      ValueError: The script was not parsed yet, or parsing failed.
    """
    if poc.tree is None:
        raise ValueError(f'{poc.name} was not parsed into an AST')
    return poc.tree


def _dotted(node: ast.AST) -> str | None:
    """Returns a name like `os.path.join` from the tree as a string.

    Returns None when the expression is not a plain chain of names, for example
    `foo().bar`, because then the real target cannot be known without running
    the code.
    """
    parts = []
    while isinstance(node, ast.Attribute):
        parts.append(node.attr)
        node = node.value
    if isinstance(node, ast.Name):
        parts.append(node.id)
        return '.'.join(reversed(parts))
    return None


def _reassigned_names(tree: ast.AST) -> set[str]:
    """Returns every name the script assigns to or uses as an argument.

    If the script reuses a name such as `print` or `os` for its own value, a
    call to that name is no longer the built-in or module. Callers skip those
    names to avoid false alarms.
    """
    names = set()
    for node in ast.walk(tree):
        if isinstance(node, ast.Name) and isinstance(node.ctx, ast.Store):
            names.add(node.id)
        elif isinstance(node, ast.arg):
            names.add(node.arg)
    return names


def _module_aliases(tree: ast.AST) -> dict[str, str]:
    """Maps each imported name to the full module path it stands for.

    For example `import subprocess as sp` maps `sp` to `subprocess`, and
    `from os import system` maps `system` to `os.system`, so checks see
    through renamed imports. Relative imports (`from . import x`) are skipped
    because they point at the script's own files, not at real modules.
    """
    aliases = {}
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            for alias in node.names:
                if alias.asname:
                    aliases[alias.asname] = alias.name
                else:
                    top = alias.name.split('.')[0]
                    aliases[top] = top
        elif (
            isinstance(node, ast.ImportFrom) and node.level == 0 and node.module
        ):
            for alias in node.names:
                aliases[alias.asname or alias.name] = (
                    f'{node.module}.{alias.name}'
                )
    return aliases


def _resolved_dotted_names(tree: ast.AST) -> Iterator[tuple[int, str]]:
    """Yields each use of an imported name, written with its full module path.

    For example, after `import os as o`, the code `o.system` is reported as
    `os.system`. Names the script later reassigns are skipped, since they no
    longer point at the module.

    Yields:
      `(line_number, full_dotted_name)` pairs.
    """
    aliases = _module_aliases(tree)
    shadowed = _reassigned_names(tree)
    for node in ast.walk(tree):
        if not isinstance(node, (ast.Attribute, ast.Name)):
            continue
        dotted = _dotted(node)
        if not dotted:
            continue
        head, _, rest = dotted.partition('.')
        if head in aliases and head not in shadowed:
            yield node.lineno, aliases[head] + ('.' + rest if rest else '')


def _string_literals(tree: ast.AST) -> list[tuple[int, str]]:
    """Returns every text string written in the script, with its line number.

    Many checks look for hints inside strings, such as file names, HTML or
    header names, without running the script.
    """
    return [
        (node.lineno, node.value)
        for node in ast.walk(tree)
        if isinstance(node, ast.Constant) and isinstance(node.value, str)
    ]


def _opened_files(tree: ast.AST) -> list[tuple[int, str, str]]:
    """Returns `open(...)` calls whose file name is written as plain text.

    Calls with a computed file name are skipped, since the name is unknown
    without running the script. For `open(os.path.join(..., 'x'))` only the
    last part (`'x'`) is used.

    Returns:
      `(line_number, path, mode)` tuples. Mode defaults to `'r'` (read).
    """
    opened = []
    for node in ast.walk(tree):
        if not isinstance(node, ast.Call) or not node.args:
            continue
        func = node.func
        name = (
            func.id if isinstance(func, ast.Name) else getattr(func, 'attr', '')
        )
        if name != 'open':
            continue
        target = node.args[0]
        if (
            isinstance(target, ast.Call)
            and getattr(target.func, 'attr', '') == 'join'
        ):
            target = target.args[-1] if target.args else None
        if not (
            isinstance(target, ast.Constant) and isinstance(target.value, str)
        ):
            continue
        mode = 'r'
        mode_node = (
            node.args[1]
            if len(node.args) > 1
            else next((k.value for k in node.keywords if k.arg == 'mode'), None)
        )
        if isinstance(mode_node, ast.Constant) and isinstance(
            mode_node.value, str
        ):
            mode = mode_node.value
        opened.append((node.lineno, target.value, mode))
    return opened


def _load_smoke_module() -> types.ModuleType | None:
    """Loads the optional smoke test module (`smoke/server.py`), if present.

    The smoke test is the only part that really starts the server, and only
    when the user asks for it with `--smoke`. The module lives in a separate
    optional folder, so it may be missing.

    Returns:
      The module, or None if it is not installed.
    """
    root_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    if root_dir not in sys.path:
        sys.path.insert(0, root_dir)
    try:
        return importlib.import_module('smoke.server')
    except ImportError:
        return None


def check_compiles(poc: Poc) -> common.Check:
    """Checks that the script is not too big and is valid Python 3.11.

    The reproduction machine runs Python 3.11, so newer syntax would fail
    there. The parser's 3.11 mode misses a few newer forms, such as the
    relaxed f-string quoting of Python 3.12, so those are not reported here.
    The fix text guesses the common cause, such as pasted Markdown code
    fences or old Python 2 code.
    """
    # Too-large files are never parsed, so report the size first.
    if poc.oversize:
        return common.Check(
            'compiles',
            common.FAIL,
            f'{poc.name} is {poc.size_bytes} bytes (limit:'
            f' {common.MAX_FILE_BYTES} bytes).',
            fix=f'Keep {poc.name} at or below {common.MAX_FILE_BYTES} bytes.',
            axis=common.AXIS_REPRO,
        )
    # The parser ran out of memory or nesting depth on this input. That is
    # not proof of a bug in the script, so a human needs to look.
    if poc.parse_exhausted is not None:
        return common.Check(
            'compiles',
            common.UNKNOWN,
            f'Parser depth or memory limit reached ({poc.parse_exhausted});'
            ' inspect manually.',
            axis=common.AXIS_REPRO,
        )
    err = poc.syntax_error
    if err is None:
        return common.Check(
            'compiles',
            common.PASS,
            'Valid Python 3.11 syntax.',
            axis=common.AXIS_REPRO,
        )
    hint = 'Fix the Python syntax error.'
    if '```' in poc.source:
        hint = 'Remove the Markdown code fences (```) pasted into the file.'
    elif 'Missing parentheses' in str(err.msg):
        hint = 'Port the Python 2 code to Python 3.11.'
    elif 'supported in Python' in str(err.msg):
        hint = (
            'Use Python 3.11-compatible syntax (the reproduction VM runs Python'
            ' 3.11).'
        )
    return common.Check(
        'compiles',
        common.FAIL,
        f'{poc.name}:{err.lineno}: {err.msg}.',
        fix=hint,
        axis=common.AXIS_REPRO,
    )


def check_deps(poc: Poc) -> common.Check:
    """Checks that every import and every file the script reads will be there.

    On the reproduction machine only the standard library and the attached
    files exist, all in one flat folder. So a third-party import, a missing
    file, or a file read from a subfolder would fail there.
    """
    tree = _require_tree(poc)
    problems = []
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            modules = [alias.name for alias in node.names]
        elif isinstance(node, ast.ImportFrom):
            modules = [node.module or node.names[0].name]
            if node.level:
                modules = [m.split('.')[0] for m in modules]
        else:
            continue
        for module in modules:
            top = module.split('.')[0]
            # An attached `helper.py` makes `import helper` work.
            if top in sys.stdlib_module_names or f'{top}.py' in poc.attachments:
                continue
            problems.append(
                f'{poc.name}:{node.lineno} imports {module!r}, which is neither in'
                ' the Python standard library nor attached'
            )
    # Files the script writes itself (mode w, a or x) do not need to be
    # attached, so reads of them are fine.
    written = {
        path for _, path, mode in _opened_files(tree) if set(mode) & set('wax')
    }
    for line, path, mode in _opened_files(tree):
        rel = path.replace('\\', '/').removeprefix('./')
        if set(mode) & set('wax') or common.ABSOLUTE_PATH_RE.match(path):
            continue
        # Reads from `./gen` are accepted without an attachment, as the pass
        # message below says.
        if rel.startswith('gen/') or path in written or rel in poc.attachments:
            continue
        if '/' in rel:
            problems.append(
                f'{poc.name}:{line} reads {path!r} from a subfolder; attach it flat'
                ' and read ./' + rel.rsplit('/', 1)[-1]
            )
        else:
            problems.append(
                f'{poc.name}:{line} reads {path!r}, which is not attached'
            )
    if problems:
        return common.Check(
            'deps_available',
            common.FAIL,
            '; '.join(problems[:3]) + '.',
            fix=(
                'Use only the Python standard library, inline helper modules, and'
                ' attach every file the script reads.'
            ),
            axis=common.AXIS_REPRO,
        )
    return common.Check(
        'deps_available',
        common.PASS,
        'Standard library imports only; every file read is attached or under'
        ' ./gen.',
        axis=common.AXIS_REPRO,
    )


def check_supported_apis(poc: Poc) -> common.Check:
    """Checks for forbidden modules, calls and strings.

    The script must only serve web pages; Chrome's automated reproduction
    system starts the browser itself. So modules that start programs, drive a
    browser or touch raw memory are not allowed. Even with no problems the
    result is "needs judgment": whether each called API really exists is left
    to the agent, since a list of real names would need constant upkeep.
    """
    tree = _require_tree(poc)
    problems = []
    for node in ast.walk(tree):
        names = []
        if isinstance(node, ast.Import):
            names = [alias.name for alias in node.names]
        elif (
            isinstance(node, ast.ImportFrom) and node.level == 0 and node.module
        ):
            names = [node.module]
        for name in names:
            top = name.split('.')[0]
            if top in FORBIDDEN_MODULES:
                problems.append(
                    f'{poc.name}:{node.lineno} imports {top} ({FORBIDDEN_MODULES[top]})'
                )
    for line, full in _resolved_dotted_names(tree):
        if FORBIDDEN_OS_CALL_RE.match(full):
            problems.append(f'{poc.name}:{line} calls {full}')
    for line, text in _string_literals(tree):
        if FORBIDDEN_STRING_RE.search(text):
            problems.append(
                f'{poc.name}:{line} references browser debugging or /proc mem'
            )
    if problems:
        return common.Check(
            'supported_apis',
            common.FAIL,
            # Drop repeated messages but keep their order; show at most 3.
            '; '.join(list(dict.fromkeys(problems))[:3]) + '.',
            fix=(
                'The script must only serve HTTP; the reproduction harness'
                ' launches the browser.'
            ),
            axis=common.AXIS_REPRO,
        )
    return common.Check(
        'supported_apis',
        common.UNKNOWN,
        'No forbidden module, call, or string found; confirm every standard'
        ' library API the script calls exists.',
        axis=common.AXIS_REPRO,
        routine=True,
    )


def check_fake_output(poc: Poc) -> common.Check:
    """Checks for crash or success text that may be typed in by hand.

    A script that just prints a fake sanitizer crash report or "exploit
    worked" message proves nothing. Real pages can contain such words too, so
    a match only asks a human to confirm ("needs judgment"); it never fails.
    """
    tree = _require_tree(poc)
    hits = [
        line
        for line, text in _string_literals(tree)
        if common.FAKE_OUTPUT_RE.search(text)
    ]
    if hits:
        return common.Check(
            'no_fake_output',
            common.UNKNOWN,
            f'Crash or success text at {common.where(poc.name, hits)}; confirm it'
            ' is not hard-coded fake output.',
            fix='Remove hard-coded crash or success text.',
            axis=common.AXIS_QUALITY,
        )
    html_hits: list[str] = []
    for name, text in poc.attachments.items():
        for n, line in enumerate(text.splitlines(), 1):
            if common.FAKE_OUTPUT_RE.search(line):
                html_hits.append(f'{name}:{n}')
    if html_hits:
        return common.Check(
            'no_fake_output',
            common.UNKNOWN,
            f'Crash or success wording in {", ".join(html_hits[:3])}; confirm it is'
            ' real page copy rather than fake output.',
            axis=common.AXIS_QUALITY,
        )
    return common.Check(
        'no_fake_output',
        common.PASS,
        'No hard-coded crash or success text.',
        axis=common.AXIS_QUALITY,
    )


def check_file_name(poc: Poc) -> common.Check:
    """Checks that the server script is named `server.py`.

    Chrome's test system starts the server by this exact file name.
    """
    if poc.name == REQUIRED_NAME:
        return common.Check(
            'py.file_name',
            common.PASS,
            f'Script is named {REQUIRED_NAME}.',
            axis=common.AXIS_REPRO,
        )
    return common.Check(
        'py.file_name',
        common.FAIL,
        f'Script is named {poc.name!r}.',
        fix=f'Rename the script to {REQUIRED_NAME!r}.',
        axis=common.AXIS_REPRO,
    )


def _port_candidates(tree: ast.AST) -> list[tuple[int, int]]:
    """Returns the port numbers the script seems to listen on.

    Looks at `(host, port)` pairs, `port=` arguments and the default of a
    `--port`/`-p` option. Simple constants like `PORT = 8000` are followed
    once. The result is a best guess from reading the code, not from running
    it.

    Returns:
      `(line_number, port)` pairs for valid port numbers (1-65535).
    """
    # Remember simple `NAME = <number>` assignments so `('', PORT)` resolves.
    constants: dict[str, int] = {}
    for node in ast.walk(tree):
        if (
            isinstance(node, ast.Assign)
            and isinstance(node.value, ast.Constant)
            and isinstance(node.value.value, int)
        ):
            for target in node.targets:
                if isinstance(target, ast.Name):
                    constants[target.id] = node.value.value

    def value(node: ast.AST) -> int | None:
        """Returns the number a port expression stands for, or None."""
        if isinstance(node, ast.Constant) and isinstance(node.value, int):
            return node.value
        if isinstance(node, ast.Name):
            return constants.get(node.id)
        return None

    ports = []
    for node in ast.walk(tree):
        candidates = []
        # A 2-item tuple starting with a string looks like `('', 8000)`.
        if (
            isinstance(node, ast.Tuple)
            and len(node.elts) == 2
            and isinstance(node.elts[0], ast.Constant)
            and isinstance(node.elts[0].value, str)
        ):
            candidates.append(node.elts[1])
        if isinstance(node, ast.Call):
            candidates += [k.value for k in node.keywords if k.arg == 'port']
            literal_args = [
                a.value for a in node.args if isinstance(a, ast.Constant)
            ]
            if '--port' in literal_args or '-p' in literal_args:
                candidates += [
                    k.value for k in node.keywords if k.arg == 'default'
                ]
        for candidate in candidates:
            port = value(candidate)
            if isinstance(port, int) and 0 < port < 65536:
                ports.append((node.lineno, port))
    return ports


def _reads_args(tree: ast.AST) -> list[int]:
    """Returns lines that read command-line arguments.

    Covers `sys.argv` and imports of argument parsing modules. Whether the
    script still runs with no arguments is left to the agent, since working
    that out from the code would need a hand-written evaluator.
    """
    lines = [
        line
        for line, full in _resolved_dotted_names(tree)
        if full == 'sys.argv' or full.startswith('sys.argv.')
    ]
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            modules = [alias.name for alias in node.names]
        elif isinstance(node, ast.ImportFrom) and node.level == 0:
            modules = [node.module or '']
        else:
            continue
        if any(m.split('.')[0] in ARG_MODULES for m in modules):
            lines.append(node.lineno)
    return sorted(set(lines))


def _contains_any_substring(
    texts: Sequence[str], needles: Sequence[str]
) -> bool:
    """Returns True if any of the needles appears inside any of the texts."""
    for text in texts:
        for needle in needles:
            if needle in text:
                return True
    return False


def check_server_contract(
    poc: Poc, smoke: bool, stated_port: int | None = None
) -> common.Check:
    """Checks that the server works the way the test system starts it.

    Chrome's test system starts `server.py` with no arguments and opens a page
    from it on port 8000. Checks the port and that some HTML entry page is
    served. Scripts that read arguments get "needs judgment" for the agent to
    settle. By default only reads the code; `--smoke` really starts the
    server to confirm.

    Args:
      poc: The parsed server script.
      smoke: Whether to run the opt-in smoke test that starts the server.
      stated_port: `server.port` from repro.json. The script must listen on
        it. Whether that port is 8000 is checked by `config.server_port`, so
        a wrong but matching port is reported only once.

    Returns:
      The `py.server_contract` result.
    """
    tree = _require_tree(poc)
    problems, hints = [], []
    expected_port = REQUIRED_PORT if stated_port is None else stated_port
    ports = _port_candidates(tree)
    if ports and expected_port not in {p for _, p in ports}:
        line, port = ports[0]
        stated = (
            f' (repro.json says {stated_port})'
            if stated_port is not None
            else ''
        )
        problems.append(f'{poc.name}:{line} listens on port {port}{stated}')
    elif not ports:
        hints.append('could not find the listening port')
    arg_lines = _reads_args(tree)
    if arg_lines:
        hints.append(
            'reads command-line arguments at'
            f' {common.where(poc.name, arg_lines)}; confirm the server starts'
            ' with none, or run with --smoke'
        )
    literals = [s for _, s in _string_literals(tree)]
    lower_literals = [s.lower() for s in literals]
    # Any of these is a sign that an entry page will be served: an attached
    # HTML file, a written poc.html/index.html, those names in the code, or
    # HTML text built inside the script.
    has_html_attachment = any(
        name.lower().endswith(HTML_ENTRY_SUFFIXES) for name in poc.attachments
    )
    writes_fallback_html = any(
        os.path.basename(path).lower() in FALLBACK_ENTRY_NAMES
        and bool(set(mode) & set('wax'))
        for _, path, mode in _opened_files(tree)
    )
    serves_entry_literal = _contains_any_substring(
        literals, FALLBACK_ENTRY_NAMES
    )
    serves_dynamic_html = _contains_any_substring(
        lower_literals, ('text/html', '<!doctype', '<html', '<script', '<body')
    )
    serves_directory = 'SimpleHTTPRequestHandler' in poc.source
    if (
        not has_html_attachment
        and not writes_fallback_html
        and not serves_entry_literal
        and not serves_dynamic_html
    ):
        problems.append(
            'no .html/.htm page is attached and the script never serves /poc.html'
            ' or /index.html'
        )
    elif (
        not has_html_attachment
        and not writes_fallback_html
        and not serves_entry_literal
        and not serves_directory
    ):
        hints.append(
            'confirm GET /poc.html or /index.html returns HTTP 200 text/html'
        )
    if 'ssl' in poc.source:
        hints.append(
            f'uses TLS; plain HTTP must also be served on {REQUIRED_PORT}'
        )
    if problems:
        return common.Check(
            'py.server_contract',
            common.FAIL,
            '; '.join(problems) + '.',
            fix=(
                f'Listen on port {REQUIRED_PORT} with no required arguments and'
                ' serve HTTP 200 text/html at /poc.html, /index.html, or an'
                ' attached .html file.'
            ),
            axis=common.AXIS_REPRO,
        )
    if smoke:
        # Only this opt-in path starts the researcher's server; all other
        # checks just read the code.
        smoke_mod = _load_smoke_module()
        if smoke_mod is None:
            return common.Check(
                'py.server_contract',
                common.UNKNOWN,
                'Smoke test module (smoke/server.py) is not installed.',
                axis=common.AXIS_REPRO,
            )
        return smoke_mod.run_smoke(poc.path, poc.name, poc.attachments_paths)
    if hints:
        # Upper-cases only the first letter; str.capitalize() would lower-case
        # the rest, mangling words such as `GET`.
        text = '; '.join(hints)
        return common.Check(
            'py.server_contract',
            common.UNKNOWN,
            text[0].upper() + text[1:] + '.',
            axis=common.AXIS_REPRO,
        )
    return common.Check(
        'py.server_contract',
        common.UNKNOWN,
        f'Port {REQUIRED_PORT}, no argument reads, entry page served (static'
        ' read); run with --smoke to confirm.',
        axis=common.AXIS_REPRO,
        routine=True,
    )


def check_no_logging(poc: Poc) -> common.Check:
    """Checks that the script does not print, log, or write log files.

    The fix text asks to remove all output; overriding the server's
    `log_message` with `pass` is fine. Paths like `/log` or `/report` in the
    code only get "needs judgment", since they may be normal page routes.
    """
    tree = _require_tree(poc)
    lines = []
    # Skip `print` if the script defines its own `print`.
    if 'print' not in _reassigned_names(tree):
        lines += [
            node.lineno
            for node in ast.walk(tree)
            if isinstance(node, ast.Call)
            and isinstance(node.func, ast.Name)
            and node.func.id == 'print'
        ]
    for line, full in _resolved_dotted_names(tree):
        if (
            full.split('.')[0] == 'logging'
            or full.startswith(('sys.stdout', 'sys.stderr', 'sys.__std'))
            or full in OUTPUT_CALLS
        ):
            lines.append(line)
    lines += [
        line
        for line, path, mode in _opened_files(tree)
        if set(mode) & set('wa') and LOG_FILE_RE.search(path)
    ]
    if lines:
        return common.Check(
            'py.no_logging',
            common.FAIL,
            f'Output or logging at {common.where(poc.name, lines)}.',
            fix=(
                'Remove all print(), logging, sys.stdout/sys.stderr writes, and log'
                ' files (overriding log_message with `pass` is fine).'
            ),
            axis=common.AXIS_REPRO,
        )
    endpoints = [
        line for line, s in _string_literals(tree) if LOG_ENDPOINT_RE.match(s)
    ]
    if endpoints:
        return common.Check(
            'py.no_logging',
            common.UNKNOWN,
            f'Possible logging endpoint at {common.where(poc.name, endpoints)};'
            ' confirm it does not record or print messages.',
            axis=common.AXIS_REPRO,
        )
    return common.Check(
        'py.no_logging',
        common.PASS,
        'No printing or logging.',
        axis=common.AXIS_REPRO,
    )


def check_user_interaction(poc: Poc) -> common.Check:
    """Checks for pages that may need a click, a permission prompt or URL edits.

    Chrome's automated reproduction system just loads the page; nobody clicks
    or edits the URL. Looks at attached pages and HTML written inside the
    script. Matches only get "needs judgment", since a handler may also run on
    load by itself.
    """
    tree = _require_tree(poc)
    pages = {
        name: text
        for name, text in poc.attachments.items()
        if name.lower().endswith(HTML_SUFFIXES)
    }
    inline = '\n'.join(
        s for _, s in _string_literals(tree) if '<script' in s or '<html' in s
    )
    if inline:
        pages[poc.name + ' (inline HTML)'] = inline
    if not pages:
        return common.Check(
            'py.no_user_interaction',
            common.UNKNOWN,
            'No poc.html or inline HTML provided; attach poc.html to check it.',
            axis=common.AXIS_REPRO,
        )
    hits: list[str] = []
    for name, text in pages.items():
        for n, line in enumerate(text.splitlines(), 1):
            if common.INTERACTION_RE.search(line):
                hits.append(f'{name}:{n}')
    if hits:
        return common.Check(
            'py.no_user_interaction',
            common.UNKNOWN,
            f'Possible user interaction at {", ".join(hits[:3])}; confirm the'
            ' trigger runs on load with no clicks, prompts, or URL hash/query.',
            axis=common.AXIS_REPRO,
        )
    return common.Check(
        'py.no_user_interaction',
        common.PASS,
        'No clicks, prompts, or URL parameters.',
        axis=common.AXIS_REPRO,
    )


def check_environment(poc: Poc, today: datetime.datetime) -> common.Check:
    """Checks that the files do not depend on the researcher's own machine.

    Fails on hard-coded absolute paths and on Origin Trial tokens (keys that
    turn on test-only web features) that are expired or about to expire.
    Checks of the `Host` header, file hashes, and built-in TLS certificates
    only get "needs judgment", since they may break on the test machine.
    """
    tree = _require_tree(poc)
    problems, hints = [], []
    literals = _string_literals(tree)
    abs_lines = [
        line for line, s in literals if common.ABSOLUTE_PATH_RE.match(s)
    ]
    if abs_lines:
        problems.append(
            f'hard-coded absolute paths at {common.where(poc.name, abs_lines)}'
        )
    texts = [poc.source, *poc.attachments.values()]
    for text in texts:
        for expiry in common.origin_trial_expiries(text):
            # Tokens that expire within `EXPIRY_MARGIN` also count, not just
            # ones that already expired.
            if expiry - today < common.EXPIRY_MARGIN:
                problems.append(f'Origin Trial token expires {expiry:%Y-%m-%d}')
    # Patch naming (`patch.diff` vs `*fix*`) and unified-diff structure are
    # checked separately by `checks/patch_diff.py`.
    if 'hashlib' in poc.source and 'poc.html' in poc.source:
        hints.append('script hashes poc.html; remove hash or size checks')
    if re.search(r"""['"]Host['"]""", poc.source):
        hints.append(
            'script checks the Host header; allow localhost:8000 and 127.0.0.1'
        )
    if 'BEGIN CERTIFICATE' in ''.join(texts):
        hints.append('embedded TLS certificate; confirm it is not near expiry')
    if problems:
        return common.Check(
            'py.environment_independent',
            common.FAIL,
            '; '.join(problems) + '.',
            fix=(
                'Use relative paths or tempfile.gettempdir(), and remove expired'
                ' Origin Trial tokens.'
            ),
            axis=common.AXIS_REPRO,
        )
    if hints:
        return common.Check(
            'py.environment_independent',
            common.UNKNOWN,
            '; '.join(hints).capitalize() + '.',
            axis=common.AXIS_REPRO,
        )
    return common.Check(
        'py.environment_independent',
        common.PASS,
        'No absolute paths, expiring tokens, Host, or hash checks.',
        axis=common.AXIS_REPRO,
    )


def _safe_ast_check(
    check_id: str, fn: Callable[[], common.Check]
) -> common.Check:
    """Runs one check and turns a parser overload into "needs judgment".

    Very deep or huge scripts can make the tree walk run out of memory or
    nesting depth. Instead of crashing the whole tool, reports that check as
    unknown and marks it for a human to look at.
    """
    try:
        return fn()
    except (MemoryError, RecursionError) as exc:
        return common.Check(
            check_id,
            common.UNKNOWN,
            f'Analysis depth limit exceeded ({type(exc).__name__}); inspect'
            ' manually.',
            axis=CHECK_AXES[check_id],
            manual=True,
        )


def check_poc(
    poc: Poc,
    *,
    smoke: bool = False,
    today: datetime.datetime | None = None,
    stated_port: int | None = None,
) -> list[common.Check]:
    """Parses one server script and runs every check on it.

    The script is parsed as Python 3.11 (never run). If parsing fails, the
    checks that need the parsed tree are reported as unknown instead of
    being skipped, so the report always lists every check in the same order.
    """
    today = today or datetime.datetime.now(datetime.timezone.utc)
    if not poc.oversize:
        # Parse as Python 3.11, the version on the reproduction machine, or
        # the checker's own version if that is older.
        target_py = min(TARGET_PYTHON, sys.version_info[:2])
        try:
            poc.tree = ast.parse(
                poc.source, filename=poc.name, feature_version=target_py
            )
        except (SyntaxError, ValueError) as exc:
            poc.syntax_error = (
                exc
                if isinstance(exc, SyntaxError)
                else SyntaxError(str(exc), (poc.name, 1, 0, ''))
            )
        except (MemoryError, RecursionError) as exc:
            # Hostile or very deep input can exhaust the parser. Record it so
            # the result is "inspect manually" instead of a crash.
            poc.parse_exhausted = type(exc).__name__

    checks = [check_compiles(poc), check_file_name(poc)]
    if poc.tree is None:
        blocked = (
            'Cannot check due to parser depth limit.'
            if poc.parse_exhausted is not None
            else 'Cannot check until the file compiles.'
        )
        checks += [
            common.Check(id_, common.UNKNOWN, blocked, axis=CHECK_AXES[id_])
            for id_ in CHECK_ORDER
            if id_ not in ('compiles', 'py.file_name')
        ]
    else:
        scheduled: list[tuple[str, Callable[[], common.Check]]] = [
            ('deps_available', lambda: check_deps(poc)),
            ('supported_apis', lambda: check_supported_apis(poc)),
            ('no_fake_output', lambda: check_fake_output(poc)),
            (
                'py.server_contract',
                lambda: check_server_contract(poc, smoke, stated_port),
            ),
            ('py.no_logging', lambda: check_no_logging(poc)),
            ('py.no_user_interaction', lambda: check_user_interaction(poc)),
            (
                'py.environment_independent',
                lambda: check_environment(poc, today),
            ),
        ]
        checks += [
            _safe_ast_check(check_id, run_fn) for check_id, run_fn in scheduled
        ]
    checks.sort(key=lambda c: CHECK_ORDER.index(c.id))
    return checks


def load_pocs(submission: common.Submission) -> list[Poc]:
    """Finds the Python server scripts in a submission and bundles their files.

    A `.py` file counts as a server if it is named `server.py` or its text
    looks like a web server. Every other non-metadata file is attached to it
    as a companion, such as the HTML page it serves.
    """
    servers = [
        f
        for f in submission.files
        if f.name.endswith('.py')
        and (f.name == REQUIRED_NAME or SERVER_RE.search(f.content))
    ]
    pocs = []
    for server in servers:
        others = [
            f
            for f in submission.files
            if f.path != server.path and not common.is_metadata_file(f.name)
        ]
        pocs.append(
            Poc(
                name=server.name,
                path=server.path,
                source=server.content,
                attachments={f.name: f.content for f in others},
                attachments_paths=[f.path for f in others],
                oversize=server.oversize,
                size_bytes=server.size_bytes,
            )
        )
    return pocs


def check(submission: common.Submission) -> list[common.Check]:
    """Runs the server script checks on every server script in a submission.

    If repro.json names this script as the server, its stated port is passed
    on so the script is checked against it.
    """
    server = common.config_value(submission, 'server')
    results: list[common.Check] = []
    for poc in load_pocs(submission):
        stated_port = (
            server['port']
            if server is not None and server['file'] == poc.name
            else None
        )
        results.extend(
            check_poc(
                poc,
                smoke=submission.smoke,
                today=submission.today,
                stated_port=stated_port,
            )
        )
    return results


def main(argv: Sequence[str] | None = None) -> int:
    """Runs the server script checks from the command line.

    Returns:
      The process exit code.
    """
    return common.run_cli(
        check, argv=argv, description=__doc__, allow_smoke=True
    )


if __name__ == '__main__':
    sys.exit(main())
