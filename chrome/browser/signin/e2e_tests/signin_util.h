// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_E2E_TESTS_SIGNIN_UTIL_H_
#define CHROME_BROWSER_SIGNIN_E2E_TESTS_SIGNIN_UTIL_H_

#include "base/time/time.h"
#include "chrome/browser/ui/browser.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/test_accounts.h"
#include "components/sync/service/sync_service.h"
#include "content/public/browser/web_contents.h"

namespace content {
class WebContents;
}

namespace signin::test {

const base::TimeDelta kDialogTimeout = base::Seconds(10);

// A wrapper importing the settings module when the chrome://settings serve the
// Polymer 3 version.
const char kSettingsScriptWrapperFormat[] =
    "import('./settings.js').then(settings => {%s});";

signin::IdentityManager* identity_manager(Browser* browser);

syncer::SyncService* sync_service(Browser* browser);

AccountReconcilor* account_reconcilor(Browser* browser);

class SignInFunctions {
 public:
  SignInFunctions(
      const base::RepeatingCallback<Browser*()> browser,
      const base::RepeatingCallback<bool(int, const GURL&, ui::PageTransition)>
          add_tab_function);

  ~SignInFunctions();

  void SignInFromWeb(const TestAccountSigninCredentials& test_account,
                     int previously_signed_in_accounts);

  void SignInFromSettings(const TestAccountSigninCredentials& test_account,
                          int previously_signed_in_accounts);

  void SignInFromCurrentPage(content::WebContents* web_contents,
                             const TestAccountSigninCredentials& test_account,
                             int previously_signed_in_accounts);

  void TurnOnSync(const TestAccountSigninCredentials& test_account,
                  int previously_signed_in_accounts);

  void SignOutFromWeb();

  void TurnOffSync();

 private:
  const base::RepeatingCallback<Browser*()> browser_;
  const base::RepeatingCallback<bool(int, const GURL&, ui::PageTransition)>
      add_tab_function_;
};

}  // namespace signin::test

#endif  // CHROME_BROWSER_SIGNIN_E2E_TESTS_SIGNIN_UTIL_H_
