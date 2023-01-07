// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFIER_SETTINGS_TEST_OBSERVER_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFIER_SETTINGS_TEST_OBSERVER_H_

#include "ash/public/cpp/notifier_settings_observer.h"

#include "ash/public/cpp/notifier_metadata.h"

namespace test {

class NotifierSettingsTestObserver : public ash::NotifierSettingsObserver {
 public:
  NotifierSettingsTestObserver();
  NotifierSettingsTestObserver(const NotifierSettingsTestObserver&) = delete;
  NotifierSettingsTestObserver& operator=(const NotifierSettingsTestObserver&) =
      delete;
  ~NotifierSettingsTestObserver() override;

  std::vector<ash::NotifierMetadata> notifiers() const { return notifiers_; }

  // ash::NotifierSettingsObserver:
  void OnNotifiersUpdated(
      const std::vector<ash::NotifierMetadata>& notifiers) override;

 private:
  std::vector<ash::NotifierMetadata> notifiers_;
};

}  // namespace test

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFIER_SETTINGS_TEST_OBSERVER_H_
