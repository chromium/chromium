// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notifier_settings_test_observer.h"

#include "ash/public/cpp/notifier_settings_controller.h"

namespace test {

NotifierSettingsTestObserver::NotifierSettingsTestObserver() {
  ash::NotifierSettingsController::Get()->AddNotifierSettingsObserver(this);
}

NotifierSettingsTestObserver::~NotifierSettingsTestObserver() {
  ash::NotifierSettingsController::Get()->RemoveNotifierSettingsObserver(this);
}

void NotifierSettingsTestObserver::OnNotifiersUpdated(
    const std::vector<ash::NotifierMetadata>& notifiers) {
  notifiers_ = notifiers;
}

}  // namespace test
