# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A promptfoo provider for the Gemini CLI."""

import json
import logging
import os
import pathlib
import subprocess
import sys
import textwrap
import threading
import time
from collections.abc import Collection
from typing import Any

import constants

sys.path.append(str(constants.CHROMIUM_SRC))
from agents.testing import checkout_helpers

DEFAULT_TIMEOUT_SECONDS = 600
DEFAULT_EXTENSIONS = [
    'build-information',
    'depot-tools',
    'landmines',
    'test-landmines',
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


def _get_env_with_overrides(
        home: pathlib.Path | None = None,
        sandbox_flags: list[str] | None = None,
        sandbox_image: str | None = None) -> dict[str, str]:
    """Returns a copy of the environment with the given overrides."""
    env = os.environ.copy()
    if home:
        env['HOME'] = str(home)
        logging.debug('HOME: %s', env.get('HOME'))
    if sandbox_flags:
        env['SANDBOX_FLAGS'] = ' '.join(sandbox_flags)
        logging.debug('SANDBOX_FLAGS: %s', env.get('SANDBOX_FLAGS'))
    if sandbox_image:
        env['GEMINI_SANDBOX_IMAGE'] = sandbox_image
        logging.debug('GEMINI_SANDBOX_IMAGE: %s',
                      env.get('GEMINI_SANDBOX_IMAGE'))
    return env


def _install_extensions(extensions: Collection[str] | None = None,
                        home_dir: pathlib.Path | None = None) -> None:
    # The installation script should identify the working tree as the "repo
    # root", so use the copy in the working tree with the CWD set
    # appropriately for subprocesses like `git`.
    if not extensions:
        return

    logging.info('Installing extensions: %s', extensions)
    command = [
        sys.executable,
        pathlib.Path('agents', 'extensions', 'install.py'),
        '--extra-extensions-dir',
        pathlib.Path('agents', 'testing', 'extensions'),
        'add',
        '--copy',
        '--skip-prompt',
        *extensions,
    ]
    result = subprocess.run(command,
                            env=_get_env_with_overrides(home=home_dir),
                            stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT,
                            text=True,
                            check=False)
    logging.debug('Extension install output:\n%s', result.stdout)
    result.check_returncode()
    logging.debug('Installed extensions:\n%s',
                  _get_installed_extensions(home_dir))


def _load_templates(templates: list[str]) -> str:
    """Loads and combines system prompt templates."""
    if not templates:
        return ''

    logging.info('Loading templates: %s', templates)
    prompt_parts = []
    for template in templates:
        with open(template, encoding='utf-8') as t:
            prompt_parts.append(t.read())
    return '\n\n'.join(prompt_parts)


def _apply_changes(changes: list[dict[str, str]]) -> None:
    """Applies changes to the repository."""
    if not changes:
        return

    logging.info('Applying changes: %s', changes)
    for change in changes:
        if len(change) != 1:
            raise ValueError(
                'Invalid change object: must have exactly one key.')

        if 'apply' in change:
            subprocess.check_call(['git', 'apply', change['apply']])
        elif 'stage' in change:
            subprocess.check_call(['git', 'add', change['stage']])
        else:
            raise ValueError(
                'Invalid change object: key must be "apply" or "stage".')


def _get_installed_extensions(home_dir: pathlib.Path | None) -> str:
    """Returns a string listing the installed extensions."""
    return subprocess.check_output(
        [
            sys.executable,
            pathlib.Path('agents', 'extensions', 'install.py'),
            'list',
        ],
        env=_get_env_with_overrides(home_dir),
        text=True,
    )


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

    gemini_cli_bin = provider_vars.get('gemini_cli_bin', 'gemini')
    command = [gemini_cli_bin, '-y']
    if not isinstance(command, list):
        return {
            'error': f"'command' must be a list of strings, but got: {command}"
        }

    sandbox_flags = []
    sandbox_image = os.environ.get('GEMINI_SANDBOX_IMAGE')
    if provider_vars.get('sandbox', False):
        command.append('--sandbox')
        depot_tools_path = checkout_helpers.get_depot_tools_path()
        if not depot_tools_path:
            return {
                'error':
                'Sandbox requires depot_tools, but it could not be located.'
            }
        sandbox_flags.append(f'-v {depot_tools_path.as_posix()}:/depot_tools')

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

    home_dir_str = provider_vars.get('home_dir')
    home_dir = pathlib.Path(home_dir_str) if home_dir_str else None

    extensions = provider_config.get('extensions', DEFAULT_EXTENSIONS)
    _install_extensions(extensions, home_dir=home_dir)

    templates = provider_config.get('templates', [])
    template_prompt = _load_templates(templates)
    if template_prompt:
        if system_prompt:
            system_prompt = f'{system_prompt}\n\n{template_prompt}'
        else:
            system_prompt = template_prompt

    changes = provider_config.get('changes', [])
    _apply_changes(changes)

    try:
        start_time = time.time()
        process = subprocess.Popen(  # pylint: disable=consider-using-with
            command,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            universal_newlines=True,
            env=_get_env_with_overrides(home=home_dir,
                                        sandbox_flags=sandbox_flags,
                                        sandbox_image=sandbox_image),
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
