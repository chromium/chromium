// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_TRACKING_PROTECTION_REMINDER_DESKTOP_UI_CONTROLLER_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_TRACKING_PROTECTION_REMINDER_DESKTOP_UI_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/privacy_sandbox/tracking_protection_reminder_service.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "ui/base/interaction/element_tracker.h"

#if BUILDFLAG(IS_ANDROID)
#error This file should only be included on desktop.
#endif

namespace privacy_sandbox {

class TrackingProtectionReminderDesktopUiController
    : TrackingProtectionReminderService::Observer,
      public KeyedService {
 public:
  TrackingProtectionReminderDesktopUiController(
      TrackingProtectionReminderService* reminder_service);
  ~TrackingProtectionReminderDesktopUiController() override;

  // Subscribes to element shown events for the tracking protection icon.
  void SubscribeToTrackingProtectionIcon();

  // From TrackingProtectionReminderService::Observer
  void OnTrackingProtectionReminderStatusChanged(
      tracking_protection::TrackingProtectionReminderStatus status) override;

 private:
  // Fired off when the tracking protection icon is shown.
  void OnTrackingProtectionIconShown(ui::TrackedElement* element);

  // Called when the reminder IPH is closed.
  void OnReminderClosed(
      user_education::FeaturePromoController* promo_controller);

  base::ScopedObservation<TrackingProtectionReminderService,
                          TrackingProtectionReminderService::Observer>
      reminder_service_observation_{this};
  raw_ptr<TrackingProtectionReminderService> reminder_service_;
  base::CallbackListSubscription icon_subscription_;
  base::WeakPtrFactory<TrackingProtectionReminderDesktopUiController>
      weak_ptr_factory_{this};
};

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_TRACKING_PROTECTION_REMINDER_DESKTOP_UI_CONTROLLER_H_
