// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HID_MOCK_HID_DEVICE_OBSERVER_H_
#define CHROME_BROWSER_HID_MOCK_HID_DEVICE_OBSERVER_H_

#include "chrome/browser/hid/hid_chooser_context.h"

#include "testing/gmock/include/gmock/gmock.h"

class MockHidDeviceObserver : public HidChooserContext::DeviceObserver {
 public:
  MockHidDeviceObserver();
  MockHidDeviceObserver(MockHidDeviceObserver&) = delete;
  MockHidDeviceObserver& operator=(MockHidDeviceObserver&) = delete;
  ~MockHidDeviceObserver() override;

  MOCK_METHOD1(OnDeviceAdded, void(const device::mojom::HidDeviceInfo&));
  MOCK_METHOD1(OnDeviceRemoved, void(const device::mojom::HidDeviceInfo&));
  MOCK_METHOD1(OnDeviceChanged, void(const device::mojom::HidDeviceInfo&));
  MOCK_METHOD0(OnHidManagerConnectionError, void());
  MOCK_METHOD0(OnHidChooserContextShutdown, void());
};

#endif  // CHROME_BROWSER_HID_MOCK_HID_DEVICE_OBSERVER_H_
