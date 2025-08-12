# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A promptfoo provider for the Gemini CLI."""

import os
import subprocess
import sys
import threading
from typing import Any

FINAL_OUTPUT_TAG = 'final_output'
DEFAULT_TIMEOUT_SECONDS = 600
DEFAULT_COMMAND = ['gemini', '-y']


def _stream_reader(stream, output_list: list[str]):
    """Reads a stream line-by-line and appends to a list."""
    try:
        for line in iter(stream.readline, ''):
            sys.stderr.write(line)
            output_list.append(line)
    except OSError:
        # Stream may be closed unexpectedly
        pass
    finally:
        stream.close()


def call_api(prompt: str, options: dict[str, Any],
             context: dict[str, Any]) -> dict[str, Any]:
    """A flexible promptfoo provider that runs a command-line tool.

    This provider streams the tool's output and captures artifacts with a
    reliable timeout.
    """
    provider_config = options.get('config', {})
    command = provider_config.get('command', DEFAULT_COMMAND)
    if not isinstance(command, list):
        return {
            'error': f"'command' must be a list of strings, but got: {command}"
        }
    system_prompt = provider_config.get('system_prompt', '')
    final_output_tag = provider_config.get('final_output_tag',
                                           FINAL_OUTPUT_TAG)
    output_instruction = (
        'IMPORTANT: After you have finished all your work, wrap your final '
        f'output in <{final_output_tag}> tags. For example: '
        f'<{final_output_tag}>your output here</{final_output_tag}>')
    combined_input = f'{output_instruction}\n{system_prompt}\n\n{prompt}'
    try:
        timeout_seconds = int(
            provider_config.get('timeoutSeconds', DEFAULT_TIMEOUT_SECONDS))
    except (ValueError, TypeError):
        timeout_seconds = DEFAULT_TIMEOUT_SECONDS
    process = None
    combined_output: list[str] = []
    try:
        cwd = provider_config.get('cwd', os.getcwd())
        process = subprocess.Popen(
            command,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            universal_newlines=True,
            cwd=cwd,
        )
        if process.stdin:
            process.stdin.write(combined_input)
            process.stdin.close()
        print(
            f'--- Streaming Output (Timeout: {timeout_seconds}s) ---',
            file=sys.stderr,
        )
        output_thread = threading.Thread(target=_stream_reader,
                                         args=(process.stdout,
                                               combined_output))
        output_thread.start()
        process.wait(timeout=timeout_seconds)
        output_thread.join(timeout=5)
        print('\n--- End of Stream ---', file=sys.stderr)

        full_output = ''.join(combined_output)
        metrics = {
            'system_prompt': system_prompt,
            'user_prompt': prompt,
            'full_output': full_output,
        }
        if process.returncode != 0:
            error_message = (
                f"Command '{' '.join(command)}' failed with return code "
                f'{process.returncode}.\n'
                f'Output:\n{full_output}')
            return {'error': error_message, 'metrics': metrics}

        start_tag = f'<{final_output_tag}>'
        end_tag = f'</{final_output_tag}>'
        start_index = full_output.rfind(start_tag)
        end_index = -1
        if start_index != -1:
            end_index = full_output.find(end_tag, start_index)
        if start_index != -1 and end_index != -1:
            final_output = full_output[start_index +
                                       len(start_tag):end_index].strip()
        else:
            print(
                f"Warning: Could not find '{start_tag}' and '{end_tag}' in "
                'output. Falling back to full output.',
                file=sys.stderr,
            )
            final_output = full_output.strip()
        return {'output': final_output, 'metrics': metrics}
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
