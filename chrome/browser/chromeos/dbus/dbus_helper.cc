// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/dbus_helper.h"

#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/settings/install_attributes.h"

namespace chromeos {

void PreEarlyInitDBus() {
  SystemSaltGetter::Initialize();

  // Initialize DBusThreadManager for the browser.
  DBusThreadManager::Initialize(DBusThreadManager::kAll);

  // Initialize the device settings service so that we'll take actions per
  // signals sent from the session manager. This needs to happen before
  // g_browser_process initializes BrowserPolicyConnector.
  DeviceSettingsService::Initialize();
  InstallAttributes::Initialize();
}

void ShutdownDBus() {
  // NOTE: This must only be called if Initialize() was called.
  DBusThreadManager::Shutdown();
  SystemSaltGetter::Shutdown();
}

}  // namespace chromeos
