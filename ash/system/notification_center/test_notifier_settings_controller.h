// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_TEST_NOTIFIER_SETTINGS_CONTROLLER_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_TEST_NOTIFIER_SETTINGS_CONTROLLER_H_

#include "ash/public/cpp/notifier_settings_controller.h"
#include "base/observer_list.h"

namespace ash {

class TestNotifierSettingsController : public NotifierSettingsController {
 public:
  TestNotifierSettingsController();

  TestNotifierSettingsController(const TestNotifierSettingsController&) =
      delete;
  TestNotifierSettingsController& operator=(
      const TestNotifierSettingsController&) = delete;

  ~TestNotifierSettingsController() override;

  void set_no_notifiers(bool no_notifiers) { no_notifiers_ = no_notifiers; }

  // NotifierSettingsController:
  void GetNotifiers() override;
  void SetNotifierEnabled(const message_center::NotifierId& notifier_id,
                          bool enabled) override;
  void AddNotifierSettingsObserver(NotifierSettingsObserver* observer) override;
  void RemoveNotifierSettingsObserver(
      NotifierSettingsObserver* observer) override;

 private:
  bool no_notifiers_ = false;

  base::ObserverList<NotifierSettingsObserver> observers_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_TEST_NOTIFIER_SETTINGS_CONTROLLER_H_
