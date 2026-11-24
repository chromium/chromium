#!/usr/bin/env vpython3

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing component_storage.py and component_storage_test.py."""

import json
import os
import shutil
import sys
import tempfile
import unittest
import unittest.mock as mock

from types import SimpleNamespace

import component_storage_test
from component_storage import ComponentStorage


# Tests should use their names to explain the meaning of the tests rather than
# relying on the extra docstrings.
class ComponentStorageTest(unittest.TestCase):
    @mock.patch('component_storage.run_ffx_command')
    def test_retrieve_instance_id(self, mock_ffx) -> None:
        # This is the real output of a ffx component show, use it as-is - even
        # lines are fairly long.
        mock_ffx.return_value = SimpleNamespace(
            stdout="""{
            "moniker": "core/session-manager/session:session/cast_runner",
            "url": "fuchsia-pkg://fuchsia.com/cast_runner#meta/cast_runner.cm",
            "environment": null,
            "instance_id": "980a67c8e6b0aa7736e69bc5e826bcc6a54f331a9d25947c2e7fb9d432576a16",
            "resolved": {
              "resolved_url": "fuchsia-pkg://fuchsia.com/cast_runner#meta/cast_runner.cm",
              "merkle_root": "e418dcea78a3335bac2282f35524247e42d5ae66e999cbe4939dab73e4f5e422",
              "runner": "elf",
              "incoming_capabilities": [
                "/config/data",
                "/config/tzdata/icu",
                "/svc/chromium.cast.ApplicationConfigManager",
                "/svc/chromium.cast.CorsExemptHeaderProvider",
                "/svc/fuchsia.feedback.ComponentDataRegister",
                "/svc/fuchsia.feedback.CrashReportingProductRegister",
                "/svc/fuchsia.inspect.InspectSink",
                "/svc/fuchsia.logger.LogSink",
                "/svc/fuchsia.sysmem.Allocator",
                "/svc/fuchsia.sysmem2.Allocator",
                "/svc/fuchsia.vulkan.loader.Loader",
                "/svc/fuchsia.component.Realm",
                "/svc/fuchsia.tracing.provider.Registry",
                "/cache"
              ],
              "exposed_capabilities": [
                "chromium.cast.DataReset",
                "fuchsia.web.Debug",
                "fuchsia.web.FrameHost",
                "cast-resolver",
                "cast-runner"
              ],
              "config": null,
              "started": {
                "runtime": {
                  "Elf": {
                    "job_id": 128198918,
                    "process_id": 128198943,
                    "process_start_time": 1047751358668791,
                    "process_start_time_utc_estimate": "2025-03-05 00:27:32.947167032 UTC"
                  }
                },
                "outgoing_capabilities": [
                  "chromium.cast.DataReset",
                  "debug",
                  "fuchsia.component.resolution.Resolver",
                  "fuchsia.component.runner.ComponentRunner",
                  "fuchsia.web.Debug",
                  "fuchsia.web.FrameHost",
                  "web_instances"
                ],
                "start_reason": "'core/session-manager/session:session/cast_agent/cast-apps:68ab9190-e34c-4ec4-8971-a9e578824640' requested capability 'cast-resolver'"
              },
              "collections": [
                "web_instances"
              ]
            }
        }"""
        )
        self.assertEqual(
            ComponentStorage(
                "fuchsia-pkg://fuchsia.com/cast_runner#meta/cast_runner.cm",
                "fuchsia-ac67-8464-ea93",
            )._instance_id,
            "980a67c8e6b0aa7736e69bc5e826bcc6a54f331a9d25947c2e7fb9d432576a16",
        )

    @mock.patch('component_storage.run_ffx_command')
    def test_assert_on_invalid_ffx_output(self, mock_ffx) -> None:
        mock_ffx.return_value = SimpleNamespace(
            stdout="""{
            "moniker": "core/session-manager/session:session/cast_runner",
            "url": "fuchsia-pkg://fuchsia.com/cast_runner#meta/cast_runner.cm",
            "environment": null,
            "not_an_instance_id": "980a67c8e6b0aa7736e69bc5e826bcc6a54f331a9d25947c2e7fb9d432576a16",
            }
        }"""
        )
        with self.assertRaises(Exception):
            ComponentStorage(
                "fuchsia-pkg://fuchsia.com/cast_runner#meta/cast_runner.cm",
                "fuchsia-ac67-8464-ea93",
            )

    @mock.patch('component_storage.run_ffx_command')
    def test_default_target_reads_fuchsia_nodename_at_init(
        self, mock_ffx
    ) -> None:
        mock_ffx.return_value = SimpleNamespace(
            stdout='{"instance_id": "1234abcd"}'
        )
        with mock.patch.dict(
            os.environ, {'FUCHSIA_NODENAME': 'fuchsia-emulator-9876'}
        ):
            self.assertEqual(
                ComponentStorage('core/feedback')._target,
                'fuchsia-emulator-9876',
            )
        mock_ffx.assert_called_once_with(
            cmd=('--machine', 'json', 'component', 'show', 'core/feedback'),
            target_id='fuchsia-emulator-9876',
            capture_output=True,
        )

    @mock.patch('component_storage.run_ffx_command')
    @mock.patch('component_storage_test.subprocess.Popen')
    def test_component_storage_test_main_orchestrate_args_and_env(
        self, mock_popen, mock_ffx
    ) -> None:
        with tempfile.TemporaryDirectory() as tmp_root:
            mock_proc = mock.MagicMock()
            mock_proc.pid = 4321
            mock_proc.wait.return_value = 0
            mock_popen.return_value = mock_proc
            with open(
                os.path.join(tmp_root, 'test_env_setup.4321.pid'), 'w'
            ) as pid_file:
                json.dump(
                    {'FUCHSIA_NODENAME': 'fuchsia-emulator-4321'}, pid_file
                )

            remote_files = set()
            uploaded_src = []

            def fake_ffx(*, cmd, target_id=None, **_kwargs):
                self.assertEqual(target_id, 'fuchsia-emulator-4321')
                if 'show' in cmd:
                    return SimpleNamespace(stdout='{"instance_id": "inst1"}')
                action = cmd[4]
                if action == 'copy':
                    if cmd[5].startswith('inst1::'):
                        dest = cmd[6]
                        if os.path.isdir(dest):
                            dest = os.path.join(
                                dest, 'component_storage_test.txt'
                            )
                        shutil.copyfile(uploaded_src[-1], dest)
                    else:
                        uploaded_src.append(cmd[5])
                        remote_files.add('component_storage_test.txt')
                    return SimpleNamespace(stdout='')
                if action == 'delete':
                    remote_files.discard('component_storage_test.txt')
                    return SimpleNamespace(stdout='')
                return SimpleNamespace(stdout='\n'.join(remote_files))

            mock_ffx.side_effect = fake_ffx

            with mock.patch.object(
                sys,
                'argv',
                [
                    '../../build/fuchsia/test/component_storage_test.py',
                    '--isolated-script-test-output=/tmp/output.json',
                    '--isolated-script-test-perf-output=/tmp/perf.json',
                    '--device-spec=x64-emu-large',
                    '--orchestrate',
                ],
            ), mock.patch.object(
                component_storage_test, 'LOG_DIR', tmp_root
            ), mock.patch.object(
                component_storage_test, 'TMP', tmp_root
            ), mock.patch.dict(os.environ, {}, clear=False):
                os.environ.pop('FUCHSIA_NODENAME', None)
                self.assertEqual(component_storage_test.main(), 0)

            mock_popen.assert_called_once_with(
                [
                    os.path.join(
                        os.path.dirname(component_storage_test.__file__),
                        'test_env_setup.py',
                    ),
                    '--logs-dir',
                    tmp_root,
                    '--out-dir',
                    '.',
                    '--device-spec=x64-emu-large',
                    '--orchestrate',
                ]
            )
            mock_proc.terminate.assert_called_once()


if __name__ == '__main__':
    unittest.main()
