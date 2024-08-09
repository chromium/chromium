// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_CONSUMER_KIOSK_TEST_HELPER_H_
#define CHROME_BROWSER_ASH_APP_MODE_CONSUMER_KIOSK_TEST_HELPER_H_

#include <string>

#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"

// TODO(crbug.com/308944171): Remove this file.
namespace ash {

// TODO(crbug.com/308944171): Remove.
// Sets `app_id` as the app to auto launch at start up.
void SetConsumerKioskAutoLaunchChromeAppForTesting(
    KioskChromeAppManager& manager,
    OwnerSettingsServiceAsh& service,
    const std::string& app_id);

// TODO(crbug.com/308944171): Remove.
// Adds a consumer Kiosk app given its `app_id`.
void AddConsumerKioskChromeAppForTesting(OwnerSettingsServiceAsh& service,
                                         const std::string& chrome_app_id);

// TODO(crbug.com/308944171): Remove.
// Removes a consumer Kiosk app given its `app_id`. Includes removing any
// locally cached data.
void RemoveConsumerKioskChromeAppForTesting(KioskChromeAppManager& manager,
                                            OwnerSettingsServiceAsh& service,
                                            const std::string& chrome_app_id);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_CONSUMER_KIOSK_TEST_HELPER_H_
