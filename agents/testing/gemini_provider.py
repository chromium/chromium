# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A promptfoo provider for the Gemini CLI."""

import dataclasses
import functools
import json
import logging
import os
import pathlib
import re
import subprocess
import sys
import textwrap
import threading
import time
from collections.abc import Collection
from typing import Any

import constants

sys.path.append(str(constants.CHROMIUM_SRC))
from agents.common import gemini_helpers
from agents.common import tempfile_ext
from agents.testing import checkout_helpers

DEFAULT_TIMEOUT_SECONDS = 600
DEFAULT_EXTENSIONS = [
    'build-information',
    'depot-tools',
    'landmines',
    'test-landmines',
]


@dataclasses.dataclass
class GeminiCliArguments:
    """Information that is relevant to starting gemini-cli for a test."""
    # The command to run gemini-cli.
    command: list[str]
    # The home directory that gemini-cli will use.
    home_dir: pathlib.Path | None
    # The environment that gemini-cli will be started in.
    env: dict[str, str]
    # The duration that gemini-cli will be allowed to run for.
    timeout_seconds: int
    # The system prompt that gemini-cli will be run with.
    system_prompt: str
    # The template prompt that gemini-cli will be run with.
    template_prompt: str
    # The user prompt to pass to gemini-cli
    user_prompt: str
    # How wide to treat the console that gemini-cli is run in.
    console_width: int


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


def _get_sandbox_image_tag() -> str | None:
    """Gets the full sandbox image tag."""
    gemini_version = gemini_helpers.get_gemini_version()
    if not gemini_version:
        logging.error('Failed to get gemini version.')
        return None
    return f'{constants.GEMINI_SANDBOX_IMAGE_URL}:{gemini_version}'


@functools.cache
def _get_container_path(sandbox_image: str | None) -> str | None:
    """Gets the default PATH from the sandbox container."""
    if not sandbox_image:
        return None

    # This is a Go template that iterates over all environment variables in the
    # image's configuration and prints each one on a new line.
    command = [
        'docker', 'inspect',
        r'--format={{range .Config.Env}}{{printf "%s\n" .}}{{end}}',
        sandbox_image
    ]
    try:
        result = subprocess.run(command,
                                stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE,
                                text=True,
                                check=True)
        logging.debug('docker inspect output:\n%s', result.stdout)
        for line in result.stdout.splitlines():
            if line.startswith('PATH='):
                return line.split('=', 1)[1]

        logging.warning('PATH not found in environment of %s', sandbox_image)
        return None
    except (subprocess.CalledProcessError, FileNotFoundError) as e:
        error_message = f'Failed to get container PATH for {sandbox_image}: {e}'
        if hasattr(e, 'stderr') and e.stderr:
            error_message += f'\nstderr:\n{e.stderr}'
        logging.error(error_message)
        return None


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


def _get_sandbox_flags() -> tuple[list[str], str]:
    """Gets flags for the gemini-cli sandbox.

    Returns:
        A tuple (flags, error). |flags| is a list of flags to use with the
        sandbox. |error| is an empty string if no error occurred, otherwise the
        error string that should be surfaced to promptfoo.
    """
    sandbox_flags = []
    depot_tools_path = checkout_helpers.get_depot_tools_path()
    if not depot_tools_path:
        return ([],
                'Sandbox requires depot_tools, but it could not be located.')
    sandbox_flags.append(f'-v {depot_tools_path.as_posix()}:/depot_tools')

    container_path = _get_container_path(_get_sandbox_image_tag())
    if container_path:
        sandbox_flags.append(f'-e PATH=/depot_tools:{container_path}')
    else:
        return ([], 'Could not determine container PATH. PATH will not be '
                'overridden.')

    return sandbox_flags, ''


def _configure_gemini_cli(home_dir: pathlib.Path,
                          telemetry_outfile: pathlib.Path) -> None:
    """Configures gemini-cli via its settings files.

    Args:
        home_dir: The path to the directory being used as the home directory.
        telemetry_outfile: The path to the file to write telemetry data to.
    """
    gemini_dir = home_dir / '.gemini'
    os.makedirs(gemini_dir, exist_ok=True)

    settings_file = gemini_dir / 'settings.json'
    if os.path.exists(settings_file):
        with open(settings_file, 'r', encoding='utf-8') as infile:
            settings_json = json.load(infile)
    else:
        settings_json = {}

    settings_json.setdefault('telemetry', {})
    settings_json['telemetry']['enabled'] = True
    settings_json['telemetry']['outfile'] = str(telemetry_outfile)

    with open(settings_file, 'w', encoding='utf-8') as outfile:
        json.dump(settings_json, outfile)

    # Add trusted folders. This is necessary for the corp version
    # but is a good safeguard for the 3p version
    trusted_folders_file = gemini_dir / 'trustedFolders.json'
    if os.path.exists(trusted_folders_file):
        with open(trusted_folders_file, 'r', encoding='utf-8') as infile:
            trusted_folders_json = json.load(infile)
    else:
        trusted_folders_json = {}

    trusted_folders_json.setdefault(os.getcwd(), 'TRUST_FOLDER')

    with open(trusted_folders_file, 'w', encoding='utf-8') as outfile:
        json.dump(trusted_folders_json, outfile)
    logging.debug('Wrote trusted folder %s: %s', trusted_folders_file,
                  trusted_folders_json)


def _get_gemini_cli_arguments(
        provider_vars: dict[str, Any], provider_config: dict[str, Any],
        user_prompt: str) -> tuple[GeminiCliArguments | None, str]:
    """Collects arguments relevant to starting/running gemini-cli.

    Args:
        provider_vars: The key/value variables given to the provider.
        provider_config: The config parsed from the test's YAML config file.
        user_prompt: The user prompt to pass to gemini-cli.

    Returns:
        A tuple (arguments, error). On success, |arguments| will be a
        GeminiCliArguments instance with all fields filled and |error| will be
        an empty string. On failure, |arguments| will be None and |error| will
        be a non-empty string containing the error message.
    """
    try:
        unparsed_timeout = provider_config.get('timeoutSeconds',
                                               DEFAULT_TIMEOUT_SECONDS)
        timeout_seconds = int(unparsed_timeout)
    except (ValueError, TypeError):
        return None, f'Failed to parse timeout from {unparsed_timeout}'

    command = []
    node_bin = provider_vars.get('node_bin')
    if node_bin:
        command.append(node_bin)
    gemini_cli_bin = provider_vars.get('gemini_cli_bin',
                                       gemini_helpers.get_gemini_executable())
    command.extend([gemini_cli_bin, '-y'])

    sandbox_flags = []
    if provider_vars.get('sandbox', False):
        command.append('--sandbox')
        sandbox_flags, error = _get_sandbox_flags()
        if error:
            return None, error

    home_dir_str = provider_vars.get('home_dir')
    home_dir = pathlib.Path(home_dir_str) if home_dir_str else None

    return GeminiCliArguments(
        command=command,
        home_dir=home_dir,
        env=_get_env_with_overrides(
            home=home_dir,
            sandbox_flags=sandbox_flags,
            sandbox_image=_get_sandbox_image_tag(),
        ),
        timeout_seconds=timeout_seconds,
        system_prompt=_get_system_prompt(provider_config),
        template_prompt=_load_templates(provider_config.get('templates', [])),
        user_prompt=user_prompt,
        console_width=int(provider_vars.get('console_width', 80)),
    ), ''


def _get_system_prompt(provider_config: dict[str, Any]) -> str:
    """Gets the system prompt to use for gemini-cli.

    Args:
        provider_config: The config parsed from the test's YAML config file.

    Returns:
        A string to use as the system prompt for the test.
    """
    return provider_config.get('system_prompt', '')


def _run_gemini_cli_with_output_streaming(
        arguments: GeminiCliArguments) -> tuple[subprocess.Popen, list[str]]:
    """Runs gemini-cli and with output streamed to console.

    The caller is responsible for handling any exceptions that may arise from
    running gemini-cli.

    Args:
        arguments: The GeminiCliArguments to run gemini-cli with

    Returns:
        A tuple (process, combined_output). |process| is the subprocess used
        to run gemini-cli. |combined_output| is a list of all stdout and stderr
        output collected from |process|. |process| will have terminated by the
        time this function returns.
    """
    output_thread = None
    process = None
    combined_output = []
    try:
        pathlib.Path('GEMINI.md').write_text(arguments.template_prompt,
                                             encoding='utf-8')
        with tempfile_ext.mkstemp_closed(suffix='.md') as system_prompt_path:

            # If a system prompt is included in the test it should replace the
            # gemini cli system prompt and is not just another user prompt.
            env = arguments.env
            if arguments.system_prompt:
                system_prompt_path.write_text(arguments.system_prompt,
                                              encoding='utf-8')
                env['GEMINI_SYSTEM_MD'] = str(system_prompt_path)

            process = subprocess.Popen(  # pylint: disable=consider-using-with
                arguments.command,
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                universal_newlines=True,
                env=env,
            )
            process.stdin.write(arguments.user_prompt)
            process.stdin.close()
            logging.info('--- Streaming Output (Timeout: %ss) ---',
                         arguments.timeout_seconds)
            output_thread = threading.Thread(
                target=_stream_reader,
                args=(process.stdout, combined_output,
                      arguments.console_width),
                daemon=True,
            )
            output_thread.start()
            process.wait(timeout=arguments.timeout_seconds)
            output_thread.join(timeout=5)
            logging.info('\n--- End of Stream ---')
            return process, combined_output
    finally:
        if process and process.poll() is None:
            process.kill()
        if output_thread:
            output_thread.join(timeout=5)
            if output_thread.is_alive():
                logging.warning('Output thread did not cleanly terminate.')


def _parse_telemetry_data(telemetry_file: pathlib.Path) -> list[dict[str, Any]]:
    """Parses gemini-cli telemetry into a list of JSON objects.

    Args:
        telemetry_file: A path to the file that gemini-cli wrote telemetry
            information to.

    Returns:
        A list of telemetry data objects. Returns an empty list if the
        telemetry file is empty or invalid.
    """
    with open(telemetry_file, encoding='utf-8') as infile:
        contents = infile.read()
    if not contents:
        return []

    # The file contents are mostly-valid JSON, except the multiple objects it
    # contains aren't within an actual list. So, modify the content to be a
    # valid list.
    # The alternative to this would be to spin up an OpenTelemetry collector
    # and directly receive the telemetry data as the test is running.
    contents_with_commas = re.sub(r'}\s*{', '},{', contents)
    corrected_content = f'[{contents_with_commas}]'
    try:
        return json.loads(corrected_content)
    except json.JSONDecodeError:
        logging.warning('Failed to parse telemetry file: %s', telemetry_file)
        return []


def _extract_token_usage(
        telemetry_data: list[dict[str, Any]]) -> dict[str, int]:
    """Extracts token usage data from gemini-cli telemetry.

    Args:
        telemetry_data: A list of telemetry data objects parsed from the
            telemetry file.

    Returns:
        A dict mapping token type to the total usage of that token type during
        the test. Returns an empty dict if the telemetry data is empty or
        invalid.
    """
    if not telemetry_data:
        return {}

    def _extract_from_last_report():
        # Get the last reported token metrics, which should be the total at the
        # end of the test.
        gemini_cli_tokens = {}
        for td in reversed(telemetry_data):
            scope_metrics = td.get('scopeMetrics', [])
            if not scope_metrics:
                continue
            for sm in scope_metrics:
                if sm.get('scope', {}).get('name') != 'gemini-cli':
                    continue
                for metric in sm.get('metrics', []):
                    if (metric.get('descriptor', {}).get('name')
                            != 'gemini_cli.token.usage'):
                        continue
                    for dp in metric.get('dataPoints', []):
                        token_type = dp['attributes']['type']
                        value = dp['value']
                        gemini_cli_tokens[token_type] = value
                    return gemini_cli_tokens
        return gemini_cli_tokens

    return _extract_from_last_report()


def _extract_tool_calls(
        telemetry_data: list[dict[str, Any]]) -> list[dict[str, Any]]:
    """Extracts tool call data from gemini-cli telemetry.

    Args:
        telemetry_data: A list of telemetry data objects parsed from the
            telemetry file.

    Returns:
        A list of tool calls made during the test. Each tool call is a dict
        containing details about the call. Returns an empty list if the
        telemetry data is empty or invalid.
    """
    if not telemetry_data:
        return []

    tool_calls = []
    for td in telemetry_data:
        attributes = td.get('attributes', {})
        if attributes.get('event.name') == 'gemini_cli.tool_call':
            function_name = attributes.get('function_name')
            if function_name:
                tool_calls.append({
                    'function_name':
                    function_name,
                    'function_args':
                    attributes.get('function_args', ''),
                    'success':
                    attributes.get('success', False),
                    'duration_ms':
                    attributes.get('duration_ms', 0),
                    'tool_type':
                    attributes.get('tool_type', ''),
                    'mcp_server_name':
                    attributes.get('mcp_server_name', ''),
                    'extension_name':
                    attributes.get('extension_name', ''),
                })
    return tool_calls


def call_api(prompt: str, options: dict[str, Any],
             context: dict[str, Any]) -> dict[str, Any]:
    """A flexible promptfoo provider that runs a command-line tool.

    This provider streams the tool's output and captures artifacts with a
    reliable timeout.
    """
    provider_config = options.get('config', {})
    provider_vars = context.get('vars', {}) if context else {}
    logging.basicConfig(
        level=logging.DEBUG
        if provider_vars.get('verbose', False) else logging.INFO,
        format='%(message)s',
    )
    logging.debug('options: %s', json.dumps(options, indent=2))
    logging.debug('context: %s', json.dumps(context, indent=2))

    with tempfile_ext.mkstemp_closed() as telemetry_outfile:
        return _run_gemini_cli_with_telemetry_output(provider_config,
                                                     provider_vars, prompt,
                                                     telemetry_outfile)


def _run_gemini_cli_with_telemetry_output(
        provider_config: dict[str, Any], provider_vars: dict[str, Any],
        user_prompt: str, telemetry_outfile: pathlib.Path) -> dict[str, Any]:
    """Runs gemini-cli using the provided information.

    Args:
        provider_config: The config parsed from the test's YAML config file.
        provider_vars: The key/value variables given to the provider.
        user_prompt: The user prompt to use for the test
        telemetry_outfile: The file to write gemini-cli telemetry info to.

    Returns:
        A promptfoo result dict.
    """

    gcli_arguments, error = _get_gemini_cli_arguments(provider_vars,
                                                      provider_config,
                                                      user_prompt)
    if error:
        return {'error': error}

    # The provider is also used for asserts which do not receive the
    # full options/context
    if gcli_arguments.home_dir:
        _configure_gemini_cli(gcli_arguments.home_dir, telemetry_outfile)
        _install_extensions(provider_config.get('extensions',
                                                DEFAULT_EXTENSIONS),
                            home_dir=gcli_arguments.home_dir)
        _apply_changes(provider_config.get('changes', []))

    process = None
    combined_output: list[str] = []
    metrics = {
        'system_prompt': gcli_arguments.system_prompt,
        'template_prompt': gcli_arguments.template_prompt,
        'user_prompt': gcli_arguments.user_prompt,
    }
    try:
        start_time = time.time()
        process, combined_output = _run_gemini_cli_with_output_streaming(
            gcli_arguments)
        elapsed_time = time.time() - start_time

        full_output = ''.join(combined_output)
        metrics['full_output'] = full_output
        metrics['duration'] = elapsed_time
        # We put this information in our own field instead of in promptfoo's
        # tokenUsage field since how tokens are grouped differs and there is not
        # a clear mapping from gemini-cli's data to what promptfoo wants.
        telemetry_data = _parse_telemetry_data(telemetry_outfile)
        metrics[constants.GEMINI_CLI_TOKEN_USAGE] = _extract_token_usage(
            telemetry_data)
        metrics['tool_calls'] = _extract_tool_calls(telemetry_data)
        if process.returncode != 0:
            error_message = (
                f"Command '{' '.join(gcli_arguments.command)}' failed with "
                f'return code {process.returncode}.\n'
                f'Output:\n{full_output}')
            return {'error': error_message, 'metrics': metrics}
        return {
            'output': full_output.strip(),
            'metrics': metrics,
        }
    except subprocess.TimeoutExpired:
        metrics['full_output'] = ''.join(combined_output)
        return {
            'error': (f'Command timed out after '
                      f'{gcli_arguments.timeout_seconds} seconds.'),
            'metrics':
            metrics,
        }
    except FileNotFoundError:
        return {
            'error': (f"Command not found: '{gcli_arguments.command[0]}'. "
                      f'Please ensure it is in your PATH.'),
            'metrics':
            metrics,
        }
    except Exception as e:
        metrics['full_output'] = ''.join(combined_output)
        return {
            'error': f'An unexpected error occurred: {e}',
            'metrics': metrics,
        }
