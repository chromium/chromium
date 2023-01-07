// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFIER_CONTROLLER_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFIER_CONTROLLER_H_

#include <memory>
#include <vector>

#include "ash/public/cpp/notifier_metadata.h"

class Profile;

namespace message_center {
struct NotifierId;
}

// An interface to control Notifiers, grouped by NotifierType. Controllers are
// responsible for both collating display data and toggling settings in response
// to user inputs.
class NotifierController {
 public:
  class Observer {
   public:
    virtual void OnIconImageUpdated(const message_center::NotifierId& id,
                                    const gfx::ImageSkia& image) = 0;
    virtual void OnNotifierEnabledChanged(const message_center::NotifierId& id,
                                          bool enabled) = 0;
  };

  NotifierController() = default;
  NotifierController(const NotifierController&) = delete;
  NotifierController& operator=(const NotifierController&) = delete;
  virtual ~NotifierController() = default;

  // Returns notifiers to display in the settings UI. Not all notifiers appear
  // in settings. If the source starts loading for icon images, it needs to call
  // Observer::OnIconImageUpdated after the icon is loaded.
  virtual std::vector<ash::NotifierMetadata> GetNotifierList(
      Profile* profile) = 0;

  // Set notifier enabled. |notifier_id| must have notifier type that can be
  // handled by the source. It has responsibility to invoke
  // Observer::OnNotifierEnabledChanged.
  virtual void SetNotifierEnabled(Profile* profile,
                                  const message_center::NotifierId& notifier_id,
                                  bool enabled) = 0;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFIER_CONTROLLER_H_
