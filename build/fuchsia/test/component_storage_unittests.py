#!/usr/bin/env vpython3

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing monitors.py."""

import unittest
import unittest.mock as mock

from types import SimpleNamespace

from component_storage import ComponentStorage


# Tests should use their names to explain the meaning of the tests rather than
# relying on the extra docstrings.
# pylint: disable=missing-docstring
class ComponentStorageTest(unittest.TestCase):

    @mock.patch('component_storage.run_ffx_command')
    def test_retrieve_instance_id(self, mock_ffx) -> None:
        # This is the real output of a ffx component show, use it as-is - even
        # lines are fairly long.
        # pylint: disable=line-too-long
        mock_ffx.return_value = SimpleNamespace(stdout="""{
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
        }""")
        # pylint: disable=protected-access
        self.assertEqual(
            ComponentStorage(
                "fuchsia-pkg://fuchsia.com/cast_runner#meta/cast_runner.cm",
                "fuchsia-ac67-8464-ea93")._instance_id,
            "980a67c8e6b0aa7736e69bc5e826bcc6a54f331a9d25947c2e7fb9d432576a16")

    @mock.patch('component_storage.run_ffx_command')
    def test_assert_on_invalid_ffx_output(self, mock_ffx) -> None:
        mock_ffx.return_value = SimpleNamespace(stdout="""{
            "moniker": "core/session-manager/session:session/cast_runner",
            "url": "fuchsia-pkg://fuchsia.com/cast_runner#meta/cast_runner.cm",
            "environment": null,
            "not_an_instance_id": "980a67c8e6b0aa7736e69bc5e826bcc6a54f331a9d25947c2e7fb9d432576a16",
            }
        }""")
        with self.assertRaises(Exception):
            ComponentStorage(
                "fuchsia-pkg://fuchsia.com/cast_runner#meta/cast_runner.cm",
                "fuchsia-ac67-8464-ea93")


if __name__ == '__main__':
    unittest.main()
