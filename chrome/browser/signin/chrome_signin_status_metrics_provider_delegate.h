// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_STATUS_METRICS_PROVIDER_DELEGATE_H_
#define CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_STATUS_METRICS_PROVIDER_DELEGATE_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "components/signin/core/browser/signin_status_metrics_provider_delegate.h"

class ChromeSigninStatusMetricsProviderDelegate
    : public SigninStatusMetricsProviderDelegate,
#if !defined(OS_ANDROID)
      public BrowserListObserver,
#endif
      public IdentityManagerFactory::Observer {
 public:
  ChromeSigninStatusMetricsProviderDelegate();
  ~ChromeSigninStatusMetricsProviderDelegate() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromeSigninStatusMetricsProviderDelegateTest,
                           UpdateStatusWhenBrowserAdded);

  // SigninStatusMetricsProviderDelegate:
  void Initialize() override;
  AccountsStatus GetStatusOfAllAccounts() override;
  std::vector<signin::IdentityManager*> GetIdentityManagersForAllAccounts()
      override;

#if !defined(OS_ANDROID)
  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
#endif

  // IdentityManagerFactoryObserver:
  void IdentityManagerCreated(
      signin::IdentityManager* identity_manager) override;
  void IdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

  // Updates the sign-in status right after a new browser is opened.
  void UpdateStatusWhenBrowserAdded(bool signed_in);

  DISALLOW_COPY_AND_ASSIGN(ChromeSigninStatusMetricsProviderDelegate);
};

#endif  // CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_STATUS_METRICS_PROVIDER_DELEGATE_H_
