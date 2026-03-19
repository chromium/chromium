# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Common Gemini CLI-related utilities."""

import functools
import os
import pathlib
import re
import shutil
import subprocess
import sys
from contextlib import contextmanager


@contextmanager
def powershell_path_context():
    """
    Temporarily adds .PS1 to PATHEXT on Windows to allow shutil.which() to find
    PowerShell scripts without explicit extensions.
    """
    is_windows = sys.platform == 'win32'
    original_pathext = os.environ.get('PATHEXT', '')

    # Entry: Only modify if we are on Windows and .PS1 isn't already there
    if is_windows and '.PS1' not in original_pathext.upper():
        # Prefer .PS1 over .JS if .JS is present.
        start = original_pathext.upper().find('.JS')
        if start >= 0:
            os.environ['PATHEXT'] = (original_pathext[:start] + '.PS1;' +
                                     original_pathext[start:])
        else:
            os.environ['PATHEXT'] = original_pathext + ';.PS1'

    try:
        yield
    finally:
        if is_windows:
            os.environ['PATHEXT'] = original_pathext


@functools.cache
def get_gemini_command(use_alias=False) -> list[str]:
    """Returns the command prefix to run the gemini executable.
    Order of preference is
      1. alias if use_alias is true
      2. which gemini
      3. binfs path
      4 'gemini' string
    """
    if use_alias and sys.platform != 'win32':
        shell_exe = os.environ.get('SHELL', '/bin/bash')
        try:
            # Use shell -i to ensure aliases are loaded from interactive config
            result = subprocess.run([shell_exe, '-i', '-c', 'alias gemini'],
                                    capture_output=True,
                                    text=True,
                                    check=False)
            if result.returncode == 0:
                output = result.stdout.strip()
                # Parse output like:
                # bash: alias gemini='/path/to/exe'
                # zsh:  gemini='/path/to/exe' OR gemini=/path/to/exe
                match = re.search(r'(?:alias )?gemini=[\'"]?([^\'"]+)[\'"]?',
                                  output)
                if match:
                    return [match.group(1).strip()]
        except Exception:
            # No alias configured
            pass
    with powershell_path_context():
        gemini_cmd = shutil.which('gemini')
        if gemini_cmd:
            if sys.platform == 'win32' and gemini_cmd.strip()[-4:].lower(
            ) == '.ps1':
                return ['powershell', '-File', gemini_cmd]
            return [gemini_cmd]

    gemini_cmd_path = pathlib.Path(
        '/google/bin/releases/gemini-cli/tools/gemini')
    if gemini_cmd_path.exists():
        return [str(gemini_cmd_path)]

    return ['gemini']


@functools.cache
def get_gemini_version(use_alias=False) -> str | None:
    """Gets the version of the Gemini CLI."""
    try:
        result = subprocess.run(
            get_gemini_command(use_alias=use_alias) + ['--version'],
            check=True,
            capture_output=True,
            text=True,
        )
        output = result.stdout.strip()
        match = re.search(r'\d+\.\d+\.\d+', output)
        if match:
            return match.group(0)
        return None
    except (subprocess.CalledProcessError, FileNotFoundError):
        return None
