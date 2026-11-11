# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Starts the researcher's `server.py` for a short live test.

Used only with `--smoke`, after the researcher agrees. Runs `server.py` in a
temporary folder for up to 17 seconds, checks that it serves the test page,
then stops it. The researcher's code runs on the user's machine, outside any
sandbox.
"""

from collections.abc import Sequence
import http.client
import os
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request

from checks import common

# Port the researcher's server must listen on.
REQUIRED_PORT = 8000
# Total time the server gets to start and answer.
SMOKE_TIMEOUT_SECONDS = 17
# Time limit for one HTTP request, so a stuck server does not use up the
# whole budget on one URL.
PROBE_TIMEOUT_SECONDS = 2
# Only the end of the server's error output is read, to keep messages short.
STDERR_TAIL_BYTES = 4096
# Pages always tried, even when no HTML file is attached.
FALLBACK_ENTRY_PATHS = ('poc.html', 'index.html')
HTML_SUFFIXES = ('.html', '.htm')
# When several HTML files are attached, the first name in this list wins.
HTML_ENTRY_PRIORITY = (
    'poc.html',
    'poc.htm',
    'index.html',
    'index.htm',
    'exploit.html',
    'repro.html',
    'test.html',
    'crash.html',
    'main.html',
)


def select_entry_urls(
    attachment_paths: Sequence[str], port: int = REQUIRED_PORT
) -> list[str]:
    """Returns the page URLs to request from the server.

    The order copies how Chrome's automated reproduction system picks the
    page to open: a preferred attached HTML file first, then `poc.html` and
    `index.html`. Each name appears only once.

    Args:
        attachment_paths: Paths of the attached files.
        port: Port the server should listen on.

    Returns:
        Full `http://localhost` URLs, best guess first.
    """
    html_names = [
        os.path.basename(path)
        for path in attachment_paths
        if os.path.basename(path).lower().endswith(HTML_SUFFIXES)
    ]
    candidates: list[str] = []
    if html_names:
        by_lower = {name.lower(): name for name in html_names}
        preferred = next(
            (by_lower[p] for p in HTML_ENTRY_PRIORITY if p in by_lower),
            html_names[0],
        )
        candidates.append(preferred)
    for fallback in FALLBACK_ENTRY_PATHS:
        if fallback not in candidates:
            candidates.append(fallback)
    return [
        f'http://localhost:{port}/{urllib.parse.quote(name)}'
        for name in candidates
    ]


def _probe_html(url: str, timeout: float) -> tuple[bool, str]:
    """Requests one URL and checks that it returns an HTML page.

    The page counts only with HTTP status 200 and a `text/html` content
    type. Network errors are not caught here; the caller handles them.

    Args:
        url: The page to request.
        timeout: Seconds to wait for an answer.

    Returns:
        A pair (ok, message) describing what the server returned.
    """
    with urllib.request.urlopen(url, timeout=timeout) as response:
        status = response.getcode()
        content_type = response.headers.get('Content-Type', '')
        if status == 200 and 'text/html' in content_type.lower():
            return (
                True,
                (
                    f'Smoke test: GET {url} returned HTTP {status}'
                    f' ({content_type}).'
                ),
            )
        return (
            False,
            (
                f'GET {url} returned HTTP {status} with Content-Type'
                f' {content_type or "<missing>"}'
            ),
        )


def _read_stderr_tail(err_file) -> str:
    """Returns the last line the server wrote to its error output.

    Only the final `STDERR_TAIL_BYTES` bytes are read, so a server that
    prints a lot cannot make the report huge. The last line is usually the
    error that made it stop.

    Args:
        err_file: Binary file that holds the server's error output.

    Returns:
        The last non-empty line, or an empty string.
    """
    err_file.seek(0, os.SEEK_END)
    size = err_file.tell()
    err_file.seek(max(0, size - STDERR_TAIL_BYTES))
    raw = err_file.read(STDERR_TAIL_BYTES)
    lines = raw.decode(errors='replace').strip().splitlines()
    return lines[-1] if lines else ''


def run_smoke(
    server_path: str,
    server_name: str,
    attachment_paths: Sequence[str],
    *,
    timeout: float = SMOKE_TIMEOUT_SECONDS,
    max_attachment_bytes: int = common.MAX_FILE_BYTES,
) -> common.Check:
    """Runs the researcher's `server.py` and checks it serves an HTML page.

    Runs researcher code for real, outside any sandbox. To limit the risk,
    the server runs in a temporary folder with a minimal environment, for a
    short time, and its whole process group is killed at the end.

    Args:
        server_path: Path to the attached `server.py`.
        server_name: File name used to start the server.
        attachment_paths: Paths of the other attached files.
        timeout: Seconds the server gets to answer.
        max_attachment_bytes: Files larger than this are not copied.

    Returns:
        A PASS check when a page answered with HTTP 200 HTML, an UNKNOWN
        check when port 8000 is busy, or a FAIL check otherwise.
    """
    # If something already uses the port, a reply would come from that
    # program, not from the researcher's server, so skip the test.
    with socket.socket() as probe:
        if probe.connect_ex(('127.0.0.1', REQUIRED_PORT)) == 0:
            return common.Check(
                'py.server_contract',
                common.UNKNOWN,
                f'Smoke test skipped: port {REQUIRED_PORT} is already in use.',
                fix='Stop the process using port 8000 and retry --smoke.',
                axis=common.AXIS_REPRO,
            )

    urls = select_entry_urls(attachment_paths, port=REQUIRED_PORT)
    with tempfile.TemporaryDirectory() as work, tempfile.TemporaryFile() as err:
        # Copy the server and attachments into the temp folder, skipping
        # files over the size limit.
        for path in [server_path, *attachment_paths]:
            if os.path.getsize(path) <= max_attachment_bytes:
                shutil.copy(path, os.path.join(work, os.path.basename(path)))
        os.makedirs(os.path.join(work, 'gen'), exist_ok=True)
        # `-E` ignores PYTHON* environment variables and `-s` skips the
        # user's site-packages. The environment keeps only PATH, and HOME
        # points at the temp folder. A new session lets the code below kill
        # the server and any child processes it starts.
        proc = subprocess.Popen(
            [sys.executable, '-E', '-s', server_name],
            cwd=work,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.DEVNULL,
            stderr=err,
            start_new_session=True,
            env={'PATH': os.environ.get('PATH', ''), 'HOME': work},
        )
        try:
            deadline = time.monotonic() + timeout
            last_diagnostics: list[str] = []

            while time.monotonic() < deadline:
                # A server that already exited has failed; show why.
                if proc.poll() is not None:
                    tail = _read_stderr_tail(err)
                    return common.Check(
                        'py.server_contract',
                        common.FAIL,
                        f'`python3 {server_name}` exited with code'
                        f' {proc.returncode}{": " + tail if tail else ""}.',
                        fix=(
                            'Keep serving HTTP on port 8000 with no required'
                            ' arguments.'
                        ),
                        axis=common.AXIS_REPRO,
                    )

                diagnostics: list[str] = []
                for url in urls:
                    # Never wait past the overall deadline, but always give
                    # each request at least 0.1 seconds.
                    per_call = min(
                        PROBE_TIMEOUT_SECONDS,
                        max(0.1, deadline - time.monotonic()),
                    )
                    try:
                        ok, message = _probe_html(url, timeout=per_call)
                        if ok:
                            return common.Check(
                                'py.server_contract',
                                common.PASS,
                                message,
                                axis=common.AXIS_REPRO,
                            )
                        diagnostics.append(message)
                    except urllib.error.HTTPError as exc:
                        diagnostics.append(
                            f'GET {url} returned HTTP {exc.code}'
                        )
                    except (
                        urllib.error.URLError,
                        OSError,
                        ValueError,
                        http.client.HTTPException,
                    ):
                        # The server may still be starting; try again on
                        # the next round.
                        pass

                # Keep the last answers seen, to explain a failure later.
                if diagnostics:
                    last_diagnostics = diagnostics
                time.sleep(0.25)

            if last_diagnostics:
                joined = '; '.join(last_diagnostics)
                return common.Check(
                    'py.server_contract',
                    common.FAIL,
                    f'Smoke test: {joined} (expected HTTP 200 with'
                    ' Content-Type: text/html).',
                    fix=(
                        'Serve HTTP 200 with header Content-Type: text/html at'
                        ' /poc.html, /index.html, or an attached .html page.'
                    ),
                    axis=common.AXIS_REPRO,
                )

            return common.Check(
                'py.server_contract',
                common.FAIL,
                f'Smoke test: no answer on port {REQUIRED_PORT}'
                f' ({", ".join(urls)}) within {timeout:g}s.',
                fix=f'Listen on port {REQUIRED_PORT} over plain HTTP.',
                axis=common.AXIS_REPRO,
            )
        finally:
            # Always stop the server. Kill its whole process group so child
            # processes do not keep running; fall back to the main process.
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except OSError:
                proc.kill()
            proc.wait()
