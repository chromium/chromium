# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A promptfoo provider for the Gemini CLI."""

import json
import logging
import pathlib
import subprocess
import sys
import textwrap
import threading
import time
from typing import Any
from collections.abc import Collection

DEFAULT_TIMEOUT_SECONDS = 600
DEFAULT_COMMAND = ['gemini', '-y']

DEFAULT_EXTENSIONS = [
    'build_information',
    'depot_tools',
    'landmines',
]


def _stream_reader(stream, output_list: list[str], width):
    """Reads a stream line-by-line and appends to a list."""
    try:
        for line in iter(stream.readline, ''):
            output_list.append(line)
            wrapped_text = '\n'.join(
                textwrap.wrap(line.rstrip('\r\n'), width=width))
            sys.stderr.write(wrapped_text + '\n')
    except OSError:
        # Stream may be closed unexpectedly
        pass
    finally:
        stream.close()


def _install_extensions(extensions: Collection[str] | None = None, ) -> None:
    # The installation script should identify the working tree as the "repo
    # root", so use the copy in the working tree with the CWD set
    # appropriately for subprocesses like `git`.
    logging.info('Installing extensions: %s', extensions)
    command = [
        sys.executable,
        pathlib.Path('agents', 'extensions', 'install.py'),
        'add',
        *extensions,
    ]
    subprocess.check_call(command)


def call_api(prompt: str, options: dict[str, Any],
             context: dict[str, Any]) -> dict[str, Any]:
    """A flexible promptfoo provider that runs a command-line tool.

    This provider streams the tool's output and captures artifacts with a
    reliable timeout.
    """
    provider_config = options.get('config', {})
    provider_vars = context.get('vars', {})
    logging.basicConfig(
        level=logging.DEBUG
        if provider_vars.get('verbose', False) else logging.INFO,
        format='%(message)s',
    )

    command = provider_config.get('command', DEFAULT_COMMAND)
    if not isinstance(command, list):
        return {
            'error': f"'command' must be a list of strings, but got: {command}"
        }
    system_prompt = provider_config.get('system_prompt', '')
    try:
        timeout_seconds = int(
            provider_config.get('timeoutSeconds', DEFAULT_TIMEOUT_SECONDS))
    except (ValueError, TypeError):
        timeout_seconds = DEFAULT_TIMEOUT_SECONDS
    process = None
    combined_output: list[str] = []

    logging.debug('options: %s', json.dumps(options, indent=2))
    logging.debug('context: %s', json.dumps(context, indent=2))

    extensions = provider_config.get('extensions', DEFAULT_EXTENSIONS)
    _install_extensions(extensions)
    try:
        start_time = time.time()
        process = subprocess.Popen(  # pylint: disable=consider-using-with
            command,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            universal_newlines=True,
        )
        if process.stdin:
            process.stdin.write(f'{system_prompt}\n\n{prompt}')
            process.stdin.close()
        logging.info('--- Streaming Output (Timeout: %ss) ---',
                     timeout_seconds)
        console_width = int(provider_vars.get('console_width', 80))
        output_thread = threading.Thread(
            target=_stream_reader,
            args=(process.stdout, combined_output, console_width),
        )
        output_thread.start()
        process.wait(timeout=timeout_seconds)
        output_thread.join(timeout=5)
        elapsed_time = time.time() - start_time
        logging.info('\n--- End of Stream ---')

        full_output = ''.join(combined_output)
        metrics = {
            'system_prompt': system_prompt,
            'user_prompt': prompt,
            'full_output': full_output,
            'duration': elapsed_time,
        }
        if process.returncode != 0:
            error_message = (
                f"Command '{' '.join(command)}' failed with return code "
                f'{process.returncode}.\n'
                f'Output:\n{full_output}')
            return {'error': error_message, 'metrics': metrics}
        return {
            'output': full_output.strip(),
            'metrics': metrics,
        }
    except subprocess.TimeoutExpired:
        if process:
            process.kill()
            output_thread.join(timeout=5)
        metrics = {
            'system_prompt': system_prompt,
            'user_prompt': prompt,
            'full_output': ''.join(combined_output),
        }
        return {
            'error': f'Command timed out after {timeout_seconds} seconds.',
            'metrics': metrics,
        }
    except FileNotFoundError:
        metrics = {
            'system_prompt': system_prompt,
            'user_prompt': prompt,
        }
        return {
            'error': f"Command not found: '{command[0]}'. Please ensure it is "
            'in your PATH.',
            'metrics': metrics,
        }
    except Exception as e:
        if process:
            process.kill()
        metrics = {
            'system_prompt': system_prompt,
            'user_prompt': prompt,
            'full_output': ''.join(combined_output),
        }
        return {
            'error': f'An unexpected error occurred: {e}',
            'metrics': metrics,
        }
