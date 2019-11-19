// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_status_metrics_provider_delegate.h"

#include <utility>

#include "build/build_config.h"
#include "components/signin/core/browser/signin_status_metrics_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(OS_ANDROID)
TEST(ChromeSigninStatusMetricsProviderDelegateTest,
     UpdateStatusWhenBrowserAdded) {
  content::BrowserTaskEnvironment task_environment;

  std::unique_ptr<ChromeSigninStatusMetricsProviderDelegate> delegate(
      new ChromeSigninStatusMetricsProviderDelegate);
  ChromeSigninStatusMetricsProviderDelegate* raw_delegate = delegate.get();
  std::unique_ptr<SigninStatusMetricsProvider> metrics_provider =
      SigninStatusMetricsProvider::CreateInstance(std::move(delegate));

  // Initial status is all signed in and then a signed-in browser is opened.
  metrics_provider->UpdateInitialSigninStatusForTesting(2, 2);
  raw_delegate->UpdateStatusWhenBrowserAdded(true);
  EXPECT_EQ(SigninStatusMetricsProviderBase::ALL_PROFILES_SIGNED_IN,
            metrics_provider->GetSigninStatusForTesting());

  // Initial status is all signed in and then a signed-out browser is opened.
  metrics_provider->UpdateInitialSigninStatusForTesting(2, 2);
  raw_delegate->UpdateStatusWhenBrowserAdded(false);
  EXPECT_EQ(SigninStatusMetricsProviderBase::MIXED_SIGNIN_STATUS,
            metrics_provider->GetSigninStatusForTesting());

  // Initial status is all signed out and then a signed-in browser is opened.
  metrics_provider->UpdateInitialSigninStatusForTesting(2, 0);
  raw_delegate->UpdateStatusWhenBrowserAdded(true);
  EXPECT_EQ(SigninStatusMetricsProviderBase::MIXED_SIGNIN_STATUS,
            metrics_provider->GetSigninStatusForTesting());

  // Initial status is all signed out and then a signed-out browser is opened.
  metrics_provider->UpdateInitialSigninStatusForTesting(2, 0);
  raw_delegate->UpdateStatusWhenBrowserAdded(false);
  EXPECT_EQ(SigninStatusMetricsProviderBase::ALL_PROFILES_NOT_SIGNED_IN,
            metrics_provider->GetSigninStatusForTesting());

  // Initial status is mixed and then a signed-in browser is opened.
  metrics_provider->UpdateInitialSigninStatusForTesting(2, 1);
  raw_delegate->UpdateStatusWhenBrowserAdded(true);
  EXPECT_EQ(SigninStatusMetricsProviderBase::MIXED_SIGNIN_STATUS,
            metrics_provider->GetSigninStatusForTesting());

  // Initial status is mixed and then a signed-out browser is opened.
  metrics_provider->UpdateInitialSigninStatusForTesting(2, 1);
  raw_delegate->UpdateStatusWhenBrowserAdded(false);
  EXPECT_EQ(SigninStatusMetricsProviderBase::MIXED_SIGNIN_STATUS,
            metrics_provider->GetSigninStatusForTesting());
}
#endif
