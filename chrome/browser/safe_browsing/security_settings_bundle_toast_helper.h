// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SECURITY_SETTINGS_BUNDLE_TOAST_HELPER_H_
#define CHROME_BROWSER_SAFE_BROWSING_SECURITY_SETTINGS_BUNDLE_TOAST_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "base/timer/timer.h"
#include "build/build_config.h"

class Profile;
class ToastController;

namespace safe_browsing {

// Helper class to manage showing the migration toast for security settings
// bundling.
class SecuritySettingsBundleToastHelper : public base::SupportsUserData::Data {
 public:
  explicit SecuritySettingsBundleToastHelper(Profile* profile);
  ~SecuritySettingsBundleToastHelper() override;

  static SecuritySettingsBundleToastHelper* GetForProfile(Profile* profile);

  // Triggers the toast if the user is in the
  // SecuritySettingsBundleToastState::kPending state.
  void TriggerIfNeeded();

  void SetToastControllerForTesting(ToastController* controller);

  static constexpr base::TimeDelta kRetryDelay = base::Seconds(15);
  static constexpr int kMaxRetries = 3;

 private:
  //  Attempts to show the toast retrying upon failure.
  void TryShowToast();

  ToastController* GetToastController();

  static const void* const kUserDataKey;

  raw_ptr<Profile> profile_;
  base::OneShotTimer toast_retry_timer_;
  raw_ptr<ToastController> toast_controller_for_testing_ = nullptr;
  int retry_count_ = 0;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SECURITY_SETTINGS_BUNDLE_TOAST_HELPER_H_
