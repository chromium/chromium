# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Common Gemini CLI-related utilities."""

import functools
import re
import shutil
import subprocess
import pathlib


@functools.cache
def get_gemini_executable() -> str:
    """Finds the gemini executable."""
    gemini_cmd = shutil.which('gemini')
    if gemini_cmd:
        return gemini_cmd

    gemini_cmd_path = pathlib.Path(
        '/google/bin/releases/gemini-cli/tools/gemini')
    if gemini_cmd_path.exists():
        return str(gemini_cmd_path)

    return 'gemini'


@functools.cache
def get_gemini_version() -> str | None:
    """Gets the version of the Gemini CLI."""
    try:
        result = subprocess.run(
            [get_gemini_executable(), '--version'],
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
