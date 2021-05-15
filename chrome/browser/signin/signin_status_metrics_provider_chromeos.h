// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_STATUS_METRICS_PROVIDER_CHROMEOS_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_STATUS_METRICS_PROVIDER_CHROMEOS_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "components/signin/core/browser/signin_status_metrics_provider_base.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// Record and report the browser sign-in status on ChromeOS during each UMA
// session. On ChromeOS, the browser can only be at unsigned-in status when
// browsing as a guest, or before the user logs in (i.e. the user sees a login
// window.) When user logs out, the browser process is terminated. Therefore,
// the browser's sign-in status during one UMA session can only be alway
// signed-in, or always unsigned-in, or changing from unsigned-in to signed-in.
class SigninStatusMetricsProviderChromeOS
    : public SigninStatusMetricsProviderBase {
 public:
  SigninStatusMetricsProviderChromeOS();
  ~SigninStatusMetricsProviderChromeOS() override;

  // SigninStatusMetricsProviderBase:
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SigninStatusMetricsProviderChromeOS,
                           ComputeSigninStatusToUpload);
  FRIEND_TEST_ALL_PREFIXES(SigninStatusMetricsProviderChromeOS,
                           ProvideCurrentSessionData_Guest);
  FRIEND_TEST_ALL_PREFIXES(SigninStatusMetricsProviderChromeOS,
                           ProvideCurrentSessionData_NonGuest);

  // Sets the |signin_status_| purely based on if the user is currently logged
  // in to a non-guest profile.
  void SetCurrentSigninStatus();

  // Compute the sign-in status to upload to UMA log given the recorded sign-in
  // status and if user is logged in now.
  SigninStatus ComputeSigninStatusToUpload(SigninStatus recorded_status,
                                           bool logged_in_now);

  // Returns true if user is signed in to a non-guest profile.
  bool IsSignedInNonGuest();

  // Used only for testing.
  static void SetGuestForTesting(bool is_guest);
  static absl::optional<bool> guest_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(SigninStatusMetricsProviderChromeOS);
};

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_STATUS_METRICS_PROVIDER_CHROMEOS_H_
