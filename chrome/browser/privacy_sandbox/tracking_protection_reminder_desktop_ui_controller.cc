// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/tracking_protection_reminder_desktop_ui_controller.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/privacy_sandbox/tracking_protection_reminder_service.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"

namespace privacy_sandbox {
namespace {

const base::Feature& GetIPHReminderFeature() {
  return feature_engagement::kIPHTrackingProtectionReminderFeature;
}

}  // namespace

TrackingProtectionReminderDesktopUiController::
    TrackingProtectionReminderDesktopUiController(
        TrackingProtectionReminderService* reminder_service)
    : reminder_service_(reminder_service) {
  if (reminder_service_) {
    reminder_service_observation_.Observe(reminder_service_);
    if (reminder_service->IsPendingReminder()) {
      SubscribeToTrackingProtectionIcon();
    }
  }
}

TrackingProtectionReminderDesktopUiController::
    ~TrackingProtectionReminderDesktopUiController() = default;

void TrackingProtectionReminderDesktopUiController::
    SubscribeToTrackingProtectionIcon() {
  if (icon_subscription_) {
    return;
  }
  icon_subscription_ =
      ui::ElementTracker::GetElementTracker()
          ->AddElementShownInAnyContextCallback(
              kCookieControlsIconElementId,
              base::BindRepeating(
                  &TrackingProtectionReminderDesktopUiController::
                      OnTrackingProtectionIconShown,
                  weak_ptr_factory_.GetWeakPtr()));
}

void TrackingProtectionReminderDesktopUiController::
    OnTrackingProtectionIconShown(ui::TrackedElement* element) {
  Browser* browser =
      chrome::FindBrowserWithUiElementContext(element->context());
  if (!browser) {
    return;
  }

  switch (reminder_service_->GetReminderType()) {
    case ReminderType::kNone:
      break;
    case ReminderType::kSilent:
      if (browser->window()->CanShowFeaturePromo(GetIPHReminderFeature())) {
        reminder_service_->OnReminderExperienced();
      }
      break;
    case ReminderType::kActive:
      user_education::FeaturePromoParams params(GetIPHReminderFeature());
      // TODO(crbug.com/349122141): Set the close callback to record the
      // reminder interaction and delta time from reminder appearing to
      // interaction.
      if (browser->window()->MaybeShowFeaturePromo(std::move(params))) {
        reminder_service_->OnReminderExperienced();
      }
      break;
  }
}

void TrackingProtectionReminderDesktopUiController::
    OnTrackingProtectionReminderStatusChanged(
        tracking_protection::TrackingProtectionReminderStatus status) {
  if (status ==
      tracking_protection::TrackingProtectionReminderStatus::kPendingReminder) {
    SubscribeToTrackingProtectionIcon();
  }
}

}  // namespace privacy_sandbox
