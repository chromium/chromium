// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_NOTIFIER_SETTINGS_CONTROLLER_H_
#define ASH_PUBLIC_CPP_NOTIFIER_SETTINGS_CONTROLLER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/macros.h"

namespace message_center {
struct NotifierId;
}

namespace ash {

class NotifierSettingsObserver;

// An interface, implemented by Chrome, which allows Ash to read and write
// settings and UI data regarding message center notification sources.
class ASH_PUBLIC_EXPORT NotifierSettingsController {
 public:
  // Returns the singleton instance.
  static NotifierSettingsController* Get();

  // Assembles the list of active notifiers and updates all
  // NotifierSettingsObservers via OnNotifiersUpdated.
  virtual void GetNotifiers() = 0;

  // Called to toggle the |enabled| state of a specific notifier (in response to
  // a user selecting or de-selecting that notifier).
  virtual void SetNotifierEnabled(const message_center::NotifierId& notifier_id,
                                  bool enabled) = 0;

  virtual void AddNotifierSettingsObserver(
      NotifierSettingsObserver* listener) = 0;
  virtual void RemoveNotifierSettingsObserver(
      NotifierSettingsObserver* listener) = 0;

 protected:
  NotifierSettingsController();
  virtual ~NotifierSettingsController();

  DISALLOW_COPY_AND_ASSIGN(NotifierSettingsController);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_NOTIFIER_SETTINGS_CONTROLLER_H_
