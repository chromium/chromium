#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for gemini_provider."""

import json
import os
import pathlib
import subprocess
import tempfile
import unittest
import unittest.mock

from pyfakefs import fake_filesystem_unittest

import gemini_provider

# pylint: disable=protected-access


class GetContainerPathUnittest(unittest.TestCase):
    """Unit tests for the `_get_container_path` function."""

    def setUp(self):
        run_patcher = unittest.mock.patch('subprocess.run')
        self.mock_run = run_patcher.start()
        self.addCleanup(run_patcher.stop)

    def tearDown(self):
        gemini_provider._get_container_path.cache_clear()

    def test_success(self):
        """Tests that the container path is returned on success."""
        self.mock_run.return_value = unittest.mock.MagicMock(
            stdout='PATH=/usr/bin:/bin\nOTHER=foo', returncode=0)

        path = gemini_provider._get_container_path('fake/image:latest')

        self.assertEqual(path, '/usr/bin:/bin')
        self.mock_run.assert_called_once_with([
            'docker', 'inspect',
            r'--format={{range .Config.Env}}{{printf "%s\n" .}}{{end}}',
            'fake/image:latest'
        ],
                                              stdout=subprocess.PIPE,
                                              stderr=subprocess.PIPE,
                                              text=True,
                                              check=True)

    def test_no_path(self):
        """Tests that None is returned when PATH is not in the output."""
        self.mock_run.return_value = unittest.mock.MagicMock(
            stdout='OTHER=foo', returncode=0)

        path = gemini_provider._get_container_path('fake/image:latest')

        self.assertIsNone(path)

    def test_docker_inspect_fails_called_process_error(self):
        """Tests that None is returned when docker inspect fails."""
        self.mock_run.side_effect = subprocess.CalledProcessError(1, 'docker')
        path = gemini_provider._get_container_path('fake/image:latest')
        self.assertIsNone(path)

    def test_docker_inspect_fails_file_not_found_error(self):
        """Tests that None is returned when FileNotFoundError is raised."""
        self.mock_run.side_effect = FileNotFoundError()
        path = gemini_provider._get_container_path('fake/image:latest')
        self.assertIsNone(path)

    def test_no_sandbox_image(self):
        """Tests that None is returned when no sandbox image is provided."""
        path = gemini_provider._get_container_path(None)
        self.assertIsNone(path)
        self.mock_run.assert_not_called()

    def test_is_cached(self):
        """Tests that the function is cached properly."""
        self.mock_run.return_value = unittest.mock.MagicMock(
            stdout='PATH=/usr/bin:/bin\nOTHER=foo', returncode=0)
        gemini_provider._get_container_path('fake/image:latest')
        gemini_provider._get_container_path('fake/image:latest')
        self.mock_run.assert_called_once()
        gemini_provider._get_container_path('fake/image:old')
        gemini_provider._get_container_path('fake/image:old')
        self.assertEqual(self.mock_run.call_count, 2)


class GetSandboxFlagsUnittest(unittest.TestCase):
    """Unit tests for the `_get_sandbox_flags` function."""

    def setUp(self):
        get_depot_tools_path_patcher = unittest.mock.patch(
            'gemini_provider.checkout_helpers.get_depot_tools_path')
        self.mock_get_depot_tools_path = get_depot_tools_path_patcher.start()
        self.addCleanup(get_depot_tools_path_patcher.stop)

        get_container_path_patcher = unittest.mock.patch(
            'gemini_provider._get_container_path')
        self.mock_get_container_path = get_container_path_patcher.start()
        self.addCleanup(get_container_path_patcher.stop)

        get_sandbox_image_tag_patcher = unittest.mock.patch(
            'gemini_provider._get_sandbox_image_tag')
        self.mock_get_sandbox_image_tag = get_sandbox_image_tag_patcher.start()
        self.addCleanup(get_sandbox_image_tag_patcher.stop)

    def test_get_sandbox_flags_success(self):
        """Tests that sandbox flags are returned correctly on success."""
        fake_depot_tools_path = pathlib.Path('/fake/depot_tools')
        self.mock_get_depot_tools_path.return_value = fake_depot_tools_path
        self.mock_get_container_path.return_value = '/usr/bin:/bin'
        self.mock_get_sandbox_image_tag.return_value = 'fake/image:latest'

        flags, error = gemini_provider._get_sandbox_flags()

        self.assertEqual(error, '')
        self.assertIn(f'-v {fake_depot_tools_path.as_posix()}:/depot_tools',
                      flags)
        self.assertIn('-e PATH=/depot_tools:/usr/bin:/bin', flags)

    def test_get_sandbox_flags_no_depot_tools(self):
        """Tests that an error is returned when depot_tools is not found."""
        self.mock_get_depot_tools_path.return_value = None

        flags, error = gemini_provider._get_sandbox_flags()

        self.assertEqual(flags, [])
        self.assertEqual(
            error,
            'Sandbox requires depot_tools, but it could not be located.')

    def test_get_sandbox_flags_no_container_path(self):
        """Tests that a missing container path results in an error."""
        self.mock_get_depot_tools_path.return_value = pathlib.Path(
            '/fake/depot_tools')
        self.mock_get_container_path.return_value = None
        self.mock_get_sandbox_image_tag.return_value = 'fake/image:latest'

        flags, error = gemini_provider._get_sandbox_flags()

        self.assertEqual(flags, [])
        self.assertEqual(
            error,
            'Could not determine container PATH. PATH will not be overridden.')


class GetGeminiCliArgumentsUnittest(unittest.TestCase):
    """Unit tests for the `_get_gemini_cli_arguments` function."""

    def setUp(self):
        get_sandbox_flags_patcher = unittest.mock.patch(
            'gemini_provider._get_sandbox_flags')
        self.mock_get_sandbox_flags = get_sandbox_flags_patcher.start()
        self.addCleanup(get_sandbox_flags_patcher.stop)
        self.mock_get_sandbox_flags.return_value = ([], '')

        get_sandbox_image_tag_patcher = unittest.mock.patch(
            'gemini_provider._get_sandbox_image_tag')
        self.mock_get_sandbox_image_tag = get_sandbox_image_tag_patcher.start()
        self.addCleanup(get_sandbox_image_tag_patcher.stop)

        get_system_prompt_patcher = unittest.mock.patch(
            'gemini_provider._get_system_prompt')
        self.mock_get_system_prompt = get_system_prompt_patcher.start()
        self.addCleanup(get_system_prompt_patcher.stop)

    def test_default_arguments(self):
        """Tests that default arguments are correct."""
        provider_vars = {}
        provider_config = {}
        user_prompt = 'test prompt'

        args, error = gemini_provider._get_gemini_cli_arguments(
            provider_vars, provider_config, user_prompt,
            pathlib.Path('/fake/telemetry.out'))

        self.assertEqual(error, '')
        self.assertEqual(args.command, [
            'gemini', '-y', '--telemetry', '--telemetry-outfile',
            str(pathlib.Path('/fake/telemetry.out'))
        ])
        self.assertIsNone(args.home_dir)
        self.assertEqual(args.timeout_seconds,
                         gemini_provider.DEFAULT_TIMEOUT_SECONDS)
        self.assertEqual(args.user_prompt, user_prompt)
        self.assertEqual(args.console_width, 80)

    def test_custom_gemini_cli_bin(self):
        """Tests that a custom gemini_cli_bin is used."""
        provider_vars = {'gemini_cli_bin': '/custom/gemini'}
        provider_config = {}
        user_prompt = 'test prompt'

        args, error = gemini_provider._get_gemini_cli_arguments(
            provider_vars, provider_config, user_prompt,
            pathlib.Path('/fake/telemetry.out'))

        self.assertEqual(error, '')
        self.assertEqual(args.command, [
            '/custom/gemini', '-y', '--telemetry', '--telemetry-outfile',
            str(pathlib.Path('/fake/telemetry.out'))
        ])

    def test_sandbox_enabled(self):
        """Tests that sandbox flags are added when sandbox is enabled."""
        self.mock_get_sandbox_flags.return_value = (['--sandbox-flag'], '')
        provider_vars = {'sandbox': True}
        provider_config = {}
        user_prompt = 'test prompt'

        args, error = gemini_provider._get_gemini_cli_arguments(
            provider_vars, provider_config, user_prompt,
            pathlib.Path('/fake/telemetry.out'))

        self.assertEqual(error, '')
        self.assertEqual(args.command, [
            'gemini', '-y', '--telemetry', '--telemetry-outfile',
            str(pathlib.Path('/fake/telemetry.out')), '--sandbox'
        ])
        self.assertIn('SANDBOX_FLAGS', args.env)
        self.assertEqual(args.env['SANDBOX_FLAGS'], '--sandbox-flag')

    def test_sandbox_flag_error(self):
        """Tests that an error is returned when _get_sandbox_flags fails."""
        self.mock_get_sandbox_flags.return_value = ([], 'Fake error')
        provider_vars = {'sandbox': True}
        provider_config = {}
        user_prompt = 'test prompt'

        args, error = gemini_provider._get_gemini_cli_arguments(
            provider_vars, provider_config, user_prompt,
            pathlib.Path('/fake/telemetry.out'))

        self.assertIsNone(args)
        self.assertEqual(error, 'Fake error')

    def test_custom_home_dir(self):
        """Tests that a custom home_dir is used."""
        provider_vars = {'home_dir': '/custom/home'}
        provider_config = {}
        user_prompt = 'test prompt'

        args, error = gemini_provider._get_gemini_cli_arguments(
            provider_vars, provider_config, user_prompt,
            pathlib.Path('/fake/telemetry.out'))

        self.assertEqual(error, '')
        self.assertEqual(args.home_dir, pathlib.Path('/custom/home'))
        self.assertIn('HOME', args.env)
        self.assertEqual(args.env['HOME'], str(pathlib.Path('/custom/home')))

    def test_invalid_timeout(self):
        """Tests that an error is returned for an invalid timeout."""
        provider_vars = {}
        provider_config = {'timeoutSeconds': 'invalid'}
        user_prompt = 'test prompt'

        args, error = gemini_provider._get_gemini_cli_arguments(
            provider_vars, provider_config, user_prompt,
            pathlib.Path('/fake/telemetry.out'))

        self.assertIsNone(args)
        self.assertEqual(error, 'Failed to parse timeout from invalid')

    def test_valid_timeout(self):
        """Tests that a valid timeout is used."""
        provider_vars = {}
        provider_config = {'timeoutSeconds': 123}
        user_prompt = 'test prompt'

        args, error = gemini_provider._get_gemini_cli_arguments(
            provider_vars, provider_config, user_prompt,
            pathlib.Path('/fake/telemetry.out'))

        self.assertEqual(error, '')
        self.assertEqual(args.timeout_seconds, 123)

    def test_string_console_width(self):
        """Tests that string console widths are successfully parsed."""
        provider_vars = {'console_width': '99'}
        provider_config = {}
        user_prompt = ''

        args, error = gemini_provider._get_gemini_cli_arguments(
            provider_vars, provider_config, user_prompt,
            pathlib.Path('/fake/telemetry.out'))

        self.assertEqual(error, '')
        self.assertEqual(args.console_width, 99)


class GetSystemPromptUnittest(unittest.TestCase):
    """Unit tests for the `_get_system_prompt` function."""

    def setUp(self):
        load_templates_patcher = unittest.mock.patch(
            'gemini_provider._load_templates')
        self.mock_load_templates = load_templates_patcher.start()
        self.addCleanup(load_templates_patcher.stop)

    def test_no_prompt(self):
        """Tests that an empty string is returned when there is no prompt."""
        self.mock_load_templates.return_value = ''
        provider_config = {}

        prompt = gemini_provider._get_system_prompt(provider_config)

        self.assertEqual(prompt, '')
        self.mock_load_templates.assert_called_once_with([])

    def test_system_prompt_only(self):
        """Tests that the system prompt is returned w/o templates."""
        self.mock_load_templates.return_value = ''
        provider_config = {'system_prompt': 'System prompt'}

        prompt = gemini_provider._get_system_prompt(provider_config)

        self.assertEqual(prompt, 'System prompt')
        self.mock_load_templates.assert_called_once_with([])

    def test_templates_only(self):
        """Tests that the template prompt is returned w/o a system prompt."""
        self.mock_load_templates.return_value = 'Template prompt'
        provider_config = {'templates': ['template1.txt']}

        prompt = gemini_provider._get_system_prompt(provider_config)

        self.assertEqual(prompt, 'Template prompt')
        self.mock_load_templates.assert_called_once_with(['template1.txt'])

    def test_system_prompt_and_templates(self):
        """Tests that the combined prompt is returned when there are both."""
        self.mock_load_templates.return_value = 'Template prompt'
        provider_config = {
            'system_prompt': 'System prompt',
            'templates': ['template1.txt']
        }

        prompt = gemini_provider._get_system_prompt(provider_config)

        self.assertEqual(prompt, 'System prompt\n\nTemplate prompt')
        self.mock_load_templates.assert_called_once_with(['template1.txt'])


class RunGeminiCliWithOutputStreamingUnittest(unittest.TestCase):
    """Unit tests for the `_run_gemini_cli_with_output_streaming` function."""

    def setUp(self):
        popen_patcher = unittest.mock.patch('subprocess.Popen')
        self.mock_popen = popen_patcher.start()
        self.addCleanup(popen_patcher.stop)

        mock_process = unittest.mock.MagicMock()
        mock_process.stdin = unittest.mock.MagicMock()
        mock_process.stdout.readline.side_effect = ['test output\n', '']
        mock_process.poll.return_value = 0
        self.mock_popen.return_value = mock_process

    def test_successful_execution(self):
        """Tests a successful execution of the gemini CLI."""
        args = gemini_provider.GeminiCliArguments(
            command=['gemini', '-y'],
            home_dir=None,
            env={},
            timeout_seconds=10,
            system_prompt='system prompt',
            user_prompt='user prompt',
            console_width=80,
        )

        process, combined_output = (
            gemini_provider._run_gemini_cli_with_output_streaming(args))

        self.mock_popen.assert_called_once_with(
            args.command,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            universal_newlines=True,
            env=args.env,
        )
        process.stdin.write.assert_called_once_with(
            'system prompt\n\nuser prompt')
        process.stdin.close.assert_called_once()
        process.wait.assert_called_once_with(timeout=10)
        self.assertEqual(combined_output, ['test output\n'])

    def test_process_killed_on_exception(self):
        """Tests that the process is killed when an exception occurs."""
        self.mock_popen.return_value.wait.side_effect = RuntimeError(
            'Fake error')
        self.mock_popen.return_value.poll.return_value = None
        args = gemini_provider.GeminiCliArguments(
            command=['gemini', '-y'],
            home_dir=None,
            env={},
            timeout_seconds=10,
            system_prompt='system prompt',
            user_prompt='user prompt',
            console_width=80,
        )

        with self.assertRaises(RuntimeError):
            gemini_provider._run_gemini_cli_with_output_streaming(args)

        self.mock_popen.return_value.kill.assert_called_once()


class ExtractTokenUsageUnittest(fake_filesystem_unittest.TestCase):
    """Unit tests for the `_extract_token_usage` function."""

    def setUp(self):
        self.setUpPyfakefs()

    def test_valid_telemetry_file(self):
        """Tests that token usage is extracted correctly from a valid file."""
        telemetry_data = {
            'scopeMetrics': [{
                'scope': {
                    'name': 'gemini-cli'
                },
                'metrics': [{
                    'descriptor': {
                        'name': 'gemini_cli.token.usage'
                    },
                    'dataPoints': [{
                        'attributes': {
                            'type': 'prompt'
                        },
                        'value': 10
                    }, {
                        'attributes': {
                            'type': 'completion'
                        },
                        'value': 20
                    }]
                }]
            }]
        }
        telemetry_content = json.dumps(telemetry_data)
        with tempfile.NamedTemporaryFile(mode='w', delete=False) as temp_file:
            temp_file.write(telemetry_content)
            temp_file_path = pathlib.Path(temp_file.name)

        token_usage = gemini_provider._extract_token_usage(temp_file_path)

        self.assertEqual(token_usage, {'prompt': 10, 'completion': 20})
        os.remove(temp_file_path)

    def test_empty_file(self):
        """Tests that an empty dict is returned for an empty file."""
        with tempfile.NamedTemporaryFile(mode='w', delete=False) as temp_file:
            temp_file_path = pathlib.Path(temp_file.name)

        token_usage = gemini_provider._extract_token_usage(temp_file_path)

        self.assertEqual(token_usage, {})
        os.remove(temp_file_path)

    def test_no_token_usage(self):
        """Tests that an empty dict is returned when there's no token usage."""
        telemetry_data = {
            'scopeMetrics': [{
                'scope': {
                    'name': 'gemini-cli'
                },
                'metrics': [{
                    'descriptor': {
                        'name': 'other_metric'
                    },
                    'dataPoints': []
                }]
            }]
        }
        telemetry_content = json.dumps(telemetry_data)
        with tempfile.NamedTemporaryFile(mode='w', delete=False) as temp_file:
            temp_file.write(telemetry_content)
            temp_file_path = pathlib.Path(temp_file.name)

        token_usage = gemini_provider._extract_token_usage(temp_file_path)

        self.assertEqual(token_usage, {})
        os.remove(temp_file_path)

    def test_multiple_data_points(self):
        """Tests that the last data point is used when there are multiple."""
        telemetry_data_1 = {
            'scopeMetrics': [{
                'scope': {
                    'name': 'gemini-cli'
                },
                'metrics': [{
                    'descriptor': {
                        'name': 'gemini_cli.token.usage'
                    },
                    'dataPoints': [{
                        'attributes': {
                            'type': 'prompt'
                        },
                        'value': 10
                    }, {
                        'attributes': {
                            'type': 'completion'
                        },
                        'value': 20
                    }]
                }]
            }]
        }
        telemetry_data_2 = {
            'scopeMetrics': [{
                'scope': {
                    'name': 'gemini-cli'
                },
                'metrics': [{
                    'descriptor': {
                        'name': 'gemini_cli.token.usage'
                    },
                    'dataPoints': [{
                        'attributes': {
                            'type': 'prompt'
                        },
                        'value': 30
                    }, {
                        'attributes': {
                            'type': 'completion'
                        },
                        'value': 40
                    }]
                }]
            }]
        }
        telemetry_content = (json.dumps(telemetry_data_1) + '\n' +
                             json.dumps(telemetry_data_2))
        with tempfile.NamedTemporaryFile(mode='w', delete=False) as temp_file:
            temp_file.write(telemetry_content)
            temp_file_path = pathlib.Path(temp_file.name)

        token_usage = gemini_provider._extract_token_usage(temp_file_path)

        self.assertEqual(token_usage, {'prompt': 30, 'completion': 40})
        os.remove(temp_file_path)



class CallApiUnittest(unittest.TestCase):
    """Unit tests for the call_api function."""

    def setUp(self):
        run_patcher = unittest.mock.patch('subprocess.run')
        self.mock_run = run_patcher.start()
        self.mock_run.return_value = unittest.mock.MagicMock(returncode=0)
        self.addCleanup(run_patcher.stop)

        get_gemini_cli_arguments_patcher = unittest.mock.patch(
            'gemini_provider._get_gemini_cli_arguments')
        self.mock_get_gemini_cli_arguments = (
            get_gemini_cli_arguments_patcher.start())
        self.addCleanup(get_gemini_cli_arguments_patcher.stop)

        run_gemini_cli_with_output_streaming_patcher = unittest.mock.patch(
            'gemini_provider._run_gemini_cli_with_output_streaming')
        self.mock_run_gemini_cli_with_output_streaming = (
            run_gemini_cli_with_output_streaming_patcher.start())
        self.addCleanup(run_gemini_cli_with_output_streaming_patcher.stop)

        self.mock_process = unittest.mock.MagicMock()
        self.mock_process.returncode = 0
        self.mock_run_gemini_cli_with_output_streaming.return_value = (
            self.mock_process, ['test output'])

    def tearDown(self):
        gemini_provider.checkout_helpers.get_depot_tools_path.cache_clear()
        gemini_provider._get_container_path.cache_clear()

    def test_successful_call(self):
        """Tests a successful call to call_api."""
        options = {'config': {}}
        context = {'vars': {}}
        self.mock_get_gemini_cli_arguments.return_value = (
            gemini_provider.GeminiCliArguments(
                command=['gemini', '-y'],
                home_dir=None,
                env={},
                timeout_seconds=10,
                system_prompt='system prompt',
                user_prompt='user prompt',
                console_width=80,
            ),
            '',
        )

        result = gemini_provider.call_api('test prompt', options, context)

        self.assertNotIn('error', result)
        self.assertEqual(result['output'], 'test output')
        self.mock_get_gemini_cli_arguments.assert_called_once_with(
            context['vars'], options['config'], 'test prompt',
            unittest.mock.ANY)
        self.mock_run_gemini_cli_with_output_streaming.assert_called_once()

    def test_get_gemini_cli_arguments_fails(self):
        """Tests when _get_gemini_cli_arguments returns an error."""
        options = {'config': {}}
        context = {'vars': {}}
        self.mock_get_gemini_cli_arguments.return_value = (None, 'Fake error')

        result = gemini_provider.call_api('test prompt', options, context)

        self.assertIn('error', result)
        self.assertEqual(result['error'], 'Fake error')
        self.mock_run_gemini_cli_with_output_streaming.assert_not_called()

    def test_process_fails(self):
        """Tests when the gemini-cli process fails."""
        options = {'config': {}}
        context = {'vars': {}}
        self.mock_get_gemini_cli_arguments.return_value = (
            gemini_provider.GeminiCliArguments(
                command=['gemini', '-y'],
                home_dir=None,
                env={},
                timeout_seconds=10,
                system_prompt='system prompt',
                user_prompt='user prompt',
                console_width=80,
            ),
            '',
        )
        self.mock_process.returncode = 1
        self.mock_run_gemini_cli_with_output_streaming.return_value = (
            self.mock_process, ['test output'])

        result = gemini_provider.call_api('test prompt', options, context)

        self.assertIn('error', result)
        self.assertIn('failed with return code 1', result['error'])

    def test_timeout_expired(self):
        """Tests that an error is returned when the process times out."""
        options = {'config': {}}
        context = {'vars': {}}
        self.mock_get_gemini_cli_arguments.return_value = (
            gemini_provider.GeminiCliArguments(
                command=['gemini', '-y'],
                home_dir=None,
                env={},
                timeout_seconds=123,
                system_prompt='system prompt',
                user_prompt='user prompt',
                console_width=80,
            ),
            '',
        )
        self.mock_run_gemini_cli_with_output_streaming.side_effect = (
            subprocess.TimeoutExpired(cmd='gemini', timeout=123))

        result = gemini_provider.call_api('test prompt', options, context)

        self.assertIn('error', result)
        self.assertEqual(result['error'],
                         'Command timed out after 123 seconds.')

    def test_file_not_found(self):
        """Tests that an error is returned when the command is not found."""
        options = {'config': {}}
        context = {'vars': {}}
        self.mock_get_gemini_cli_arguments.return_value = (
            gemini_provider.GeminiCliArguments(
                command=['gemini', '-y'],
                home_dir=None,
                env={},
                timeout_seconds=123,
                system_prompt='system prompt',
                user_prompt='user prompt',
                console_width=80,
            ),
            '',
        )
        self.mock_run_gemini_cli_with_output_streaming.side_effect = (
            FileNotFoundError())

        result = gemini_provider.call_api('test prompt', options, context)

        self.assertIn('error', result)
        self.assertIn("Command not found: 'gemini'", result['error'])

    def test_unexpected_error(self):
        """Tests that an error is returned when an unexpected error occurs."""
        options = {'config': {}}
        context = {'vars': {}}
        self.mock_get_gemini_cli_arguments.return_value = (
            gemini_provider.GeminiCliArguments(
                command=['gemini', '-y'],
                home_dir=None,
                env={},
                timeout_seconds=123,
                system_prompt='system prompt',
                user_prompt='user prompt',
                console_width=80,
            ),
            '',
        )
        self.mock_run_gemini_cli_with_output_streaming.side_effect = (
            RuntimeError('Fake unexpected error'))

        result = gemini_provider.call_api('test prompt', options, context)

        self.assertIn('error', result)
        self.assertEqual(
            result['error'],
            'An unexpected error occurred: Fake unexpected error')


if __name__ == '__main__':
    unittest.main()
