// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/test_notifier_settings_controller.h"

#include <vector>

#include "ash/public/cpp/notifier_metadata.h"
#include "ash/public/cpp/notifier_settings_observer.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace ash {

TestNotifierSettingsController::TestNotifierSettingsController() = default;
TestNotifierSettingsController::~TestNotifierSettingsController() = default;

void TestNotifierSettingsController::GetNotifiers() {
  std::vector<NotifierMetadata> notifiers = GetTestNotifiersMetadata();

  for (auto& observer : observers_)
    observer.OnNotifiersUpdated(notifiers);
}

void TestNotifierSettingsController::SetNotifierEnabled(
    const message_center::NotifierId& notifier_id,
    bool enabled) {}

void TestNotifierSettingsController::AddNotifierSettingsObserver(
    NotifierSettingsObserver* observer) {
  observers_.AddObserver(observer);
}

void TestNotifierSettingsController::RemoveNotifierSettingsObserver(
    NotifierSettingsObserver* observer) {
  observers_.RemoveObserver(observer);
}

std::vector<NotifierMetadata>
TestNotifierSettingsController::GetTestNotifiersMetadata() const {
  std::vector<NotifierMetadata> notifiers;
  if (!no_notifiers_) {
    notifiers.emplace_back(message_center::NotifierId(
                               message_center::NotifierType::APPLICATION, "id"),
                           u"title", /* enabled= */ true, /* enforced= */ false,
                           gfx::ImageSkia());
    notifiers.emplace_back(
        message_center::NotifierId(message_center::NotifierType::APPLICATION,
                                   "id2"),
        u"other title", /* enabled= */ false, /* enforced= */ false,
        gfx::ImageSkia());
  }

  return notifiers;
}

}  // namespace ash
