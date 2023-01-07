// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/device_attributes.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/device_attributes.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"

// This class provides integration testing for the device attributes crosapi.
// TODO(https://crbug.com/1134340): The logic being tested does not rely on
// //chrome or //content so it would be helpful if this lived in a lower-level
// test suite.
using DeviceAttributesLacrosBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(DeviceAttributesLacrosBrowserTest,
                       GetDirectoryDeviceId) {
  crosapi::mojom::DeviceAttributesStringResultPtr result;
  crosapi::mojom::DeviceAttributesAsyncWaiter async_waiter(
      chromeos::LacrosService::Get()
          ->GetRemote<crosapi::mojom::DeviceAttributes>()
          .get());
  async_waiter.GetDirectoryDeviceId(&result);

  // TODO(https://crbug.com/1165882): Write more robust tests. These APIs all
  // fail because the ash user is not affiliated with the device.
  ASSERT_TRUE(result->is_error_message());
}

IN_PROC_BROWSER_TEST_F(DeviceAttributesLacrosBrowserTest,
                       GetDeviceSerialNumber) {
  crosapi::mojom::DeviceAttributesStringResultPtr result;
  crosapi::mojom::DeviceAttributesAsyncWaiter async_waiter(
      chromeos::LacrosService::Get()
          ->GetRemote<crosapi::mojom::DeviceAttributes>()
          .get());
  async_waiter.GetDeviceSerialNumber(&result);

  // TODO(https://crbug.com/1165882): Write more robust tests. These APIs all
  // fail because the ash user is not affiliated with the device.
  ASSERT_TRUE(result->is_error_message());
}

IN_PROC_BROWSER_TEST_F(DeviceAttributesLacrosBrowserTest, GetDeviceAssetId) {
  crosapi::mojom::DeviceAttributesStringResultPtr result;
  crosapi::mojom::DeviceAttributesAsyncWaiter async_waiter(
      chromeos::LacrosService::Get()
          ->GetRemote<crosapi::mojom::DeviceAttributes>()
          .get());
  async_waiter.GetDeviceAssetId(&result);

  // TODO(https://crbug.com/1165882): Write more robust tests. These APIs all
  // fail because the ash user is not affiliated with the device.
  ASSERT_TRUE(result->is_error_message());
}

IN_PROC_BROWSER_TEST_F(DeviceAttributesLacrosBrowserTest,
                       GetDeviceAnnotatedLocation) {
  crosapi::mojom::DeviceAttributesStringResultPtr result;
  crosapi::mojom::DeviceAttributesAsyncWaiter async_waiter(
      chromeos::LacrosService::Get()
          ->GetRemote<crosapi::mojom::DeviceAttributes>()
          .get());
  async_waiter.GetDeviceAnnotatedLocation(&result);

  // TODO(https://crbug.com/1165882): Write more robust tests. These APIs all
  // fail because the ash user is not affiliated with the device.
  ASSERT_TRUE(result->is_error_message());
}

IN_PROC_BROWSER_TEST_F(DeviceAttributesLacrosBrowserTest, GetDeviceHostname) {
  crosapi::mojom::DeviceAttributesStringResultPtr result;
  crosapi::mojom::DeviceAttributesAsyncWaiter async_waiter(
      chromeos::LacrosService::Get()
          ->GetRemote<crosapi::mojom::DeviceAttributes>()
          .get());
  async_waiter.GetDeviceHostname(&result);

  // TODO(https://crbug.com/1165882): Write more robust tests. These APIs all
  // fail because the ash user is not affiliated with the device.
  ASSERT_TRUE(result->is_error_message());
}
