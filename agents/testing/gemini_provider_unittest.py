#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for gemini_provider."""

import pathlib
import subprocess
import unittest
import unittest.mock

import gemini_provider


class GeminiProviderUnittest(unittest.TestCase):
    """Unit tests for the `gemini_provider` module."""

    def setUp(self):
        popen_patcher = unittest.mock.patch('subprocess.Popen')
        self.mock_popen = popen_patcher.start()
        self.addCleanup(popen_patcher.stop)

        run_patcher = unittest.mock.patch('subprocess.run')
        self.mock_run = run_patcher.start()
        self.mock_run.return_value = unittest.mock.MagicMock(returncode=0)
        self.addCleanup(run_patcher.stop)

        mock_process = unittest.mock.MagicMock()
        mock_process.stdin = unittest.mock.MagicMock()
        mock_process.stdout.readline.side_effect = ['test output\n', '']
        mock_process.wait.return_value = 0
        mock_process.returncode = 0
        self.mock_popen.return_value = mock_process

        get_depot_tools_path_patcher = unittest.mock.patch(
            'gemini_provider.checkout_helpers.get_depot_tools_path')
        self.mock_get_depot_tools_path = get_depot_tools_path_patcher.start()
        self.addCleanup(get_depot_tools_path_patcher.stop)

        get_gemini_version_patcher = unittest.mock.patch(
            'agents.common.gemini_helpers.get_gemini_version')
        self.mock_get_gemini_version = get_gemini_version_patcher.start()
        self.addCleanup(get_gemini_version_patcher.stop)

        get_sandbox_image_tag_patcher = unittest.mock.patch(
            'gemini_provider._get_sandbox_image_tag')
        self.mock_get_sandbox_image_tag = get_sandbox_image_tag_patcher.start()
        self.addCleanup(get_sandbox_image_tag_patcher.stop)

    def tearDown(self):
        gemini_provider.checkout_helpers.get_depot_tools_path.cache_clear()

    def test_call_api_no_gemini_cli_bin(self):
        """Tests that the default command is used when no bin is provided."""
        options = {'config': {}}
        context = {'vars': {}}

        gemini_provider.call_api('test prompt', options, context)

        self.mock_popen.assert_called_once()
        popen_args = self.mock_popen.call_args[0][0]
        self.assertEqual(popen_args, ['gemini', '-y'])

    def test_call_api_with_gemini_cli_bin(self):
        """Tests that a custom command is used when a bin is provided."""
        options = {'config': {}}
        gemini_cli_bin = pathlib.Path('/', 'custom', 'gemini')
        context = {'vars': {'gemini_cli_bin': str(gemini_cli_bin)}}

        gemini_provider.call_api('test prompt', options, context)

        self.mock_popen.assert_called_once()
        popen_args = self.mock_popen.call_args[0][0]
        self.assertEqual(popen_args, [str(gemini_cli_bin), '-y'])

    def test_call_api_with_home_dir(self):
        """Tests that HOME is set in the environment when home_dir is set."""
        options = {'config': {}}
        home_dir = str(pathlib.Path('/', 'test', 'home'))
        context = {'vars': {'home_dir': home_dir}}

        gemini_provider.call_api('test prompt', options, context)

        # Check the environment for the main Popen call
        self.mock_popen.assert_called_once()
        popen_kwargs = self.mock_popen.call_args.kwargs
        self.assertIn('env', popen_kwargs)
        self.assertEqual(popen_kwargs['env']['HOME'], home_dir)

        # Check the environment for the _install_extensions run call
        self.mock_run.assert_called()
        run_kwargs = self.mock_run.call_args.kwargs
        self.assertIn('env', run_kwargs)
        self.assertEqual(run_kwargs['env']['HOME'], home_dir)

    def test_call_api_sandbox_depot_tools_succeeds(self):
        """Tests that sandbox flags are set when depot_tools is found."""
        fake_depot_tools_path = pathlib.Path('/fake/depot_tools')
        self.mock_get_depot_tools_path.return_value = fake_depot_tools_path
        fake_sandbox_image = 'fake/sandbox/image:latest'
        self.mock_get_sandbox_image_tag.return_value = fake_sandbox_image
        fake_container_path = '/usr/bin:/bin'

        def mock_run_side_effect(*args, **_kwargs):
            command = args[0]
            if command[0] == 'docker':
                self.assertEqual(command, [
                    'docker', 'inspect',
                    r'--format={{range .Config.Env}}{{printf "%s\n" .}}{{end}}',
                    fake_sandbox_image
                ])
                return unittest.mock.MagicMock(
                    stdout=f'PATH={fake_container_path}', returncode=0)
            # For _install_extensions
            return unittest.mock.MagicMock(returncode=0)

        self.mock_run.side_effect = mock_run_side_effect

        options = {'config': {}}
        context = {'vars': {'sandbox': True}}

        with unittest.mock.patch.dict('os.environ', {}, clear=True):
            result = gemini_provider.call_api('test prompt', options, context)

        self.assertNotIn('error', result)
        self.mock_popen.assert_called_once()
        popen_kwargs = self.mock_popen.call_args.kwargs
        self.assertIn('env', popen_kwargs)
        self.assertIn('SANDBOX_FLAGS', popen_kwargs['env'])
        env = popen_kwargs['env']
        self.assertEqual(env['GEMINI_SANDBOX_IMAGE'], fake_sandbox_image)
        sandbox_flags = env['SANDBOX_FLAGS']
        expected_v_flag = f'-v {fake_depot_tools_path.as_posix()}:/depot_tools'
        self.assertIn(expected_v_flag, sandbox_flags)
        expected_e_flag = f'-e PATH=/depot_tools:{fake_container_path}'
        self.assertIn(expected_e_flag, sandbox_flags)

    def test_call_api_sandbox_depot_tools_fails(self):
        """Tests that an error is returned when depot_tools is not found."""
        self.mock_get_depot_tools_path.return_value = None
        options = {'config': {}}
        context = {'vars': {'sandbox': True}}

        result = gemini_provider.call_api('test prompt', options, context)

        self.assertIn('error', result)
        self.assertIn(
            'Sandbox requires depot_tools, but it could not be located.',
            result['error'])
        self.mock_popen.assert_not_called()

    def test_call_api_sandbox_docker_inspect_fails(self):
        """Tests that an error is returned when docker inspect fails."""
        self.mock_get_depot_tools_path.return_value = pathlib.Path(
            '/fake/depot_tools')
        fake_sandbox_image = 'fake/sandbox/image:latest'
        self.mock_get_sandbox_image_tag.return_value = fake_sandbox_image

        def mock_run_side_effect(*args, **_kwargs):
            command = args[0]
            if command[0] == 'docker':
                # Simulate docker inspect failing
                raise subprocess.CalledProcessError(1, command)
            # For _install_extensions
            return unittest.mock.MagicMock(returncode=0)

        self.mock_run.side_effect = mock_run_side_effect

        options = {'config': {}}
        context = {'vars': {'sandbox': True}}

        with unittest.mock.patch.dict('os.environ', {}, clear=True):
            result = gemini_provider.call_api('test prompt', options, context)

        self.assertIn('error', result)
        self.assertIn('Could not determine container PATH', result['error'])
        self.mock_popen.assert_not_called()


if __name__ == '__main__':
    unittest.main()
