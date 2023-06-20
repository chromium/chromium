// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/hid_status_icon.h"

#include "chrome/browser/device_notifications/device_status_icon_renderer.h"
#include "chrome/grit/generated_resources.h"

HidStatusIcon::HidStatusIcon()
    : HidSystemTrayIcon(std::make_unique<DeviceStatusIconRenderer>(
          this,
          chrome::HELP_SOURCE_WEBHID,
          IDS_WEBHID_SYSTEM_TRAY_ICON_ABOUT_HID_DEVICE)) {}

HidStatusIcon::~HidStatusIcon() = default;
