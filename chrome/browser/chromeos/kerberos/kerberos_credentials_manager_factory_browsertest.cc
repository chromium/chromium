// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kerberos/kerberos_credentials_manager_factory.h"

#include <memory>

#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/kerberos/kerberos_credentials_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

namespace chromeos {

class KerberosCredentialsManagerFactoryBrowserTest
    : public InProcessBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        chromeos::switches::kIgnoreUserProfileMappingForTests);
  }
};

IN_PROC_BROWSER_TEST_F(KerberosCredentialsManagerFactoryBrowserTest,
                       GetServiceForPrimaryProfile) {
  Profile* const profile = browser()->profile();
  ASSERT_TRUE(ProfileHelper::IsPrimaryProfile(profile));

  KerberosCredentialsManager* manager =
      KerberosCredentialsManagerFactory::GetExisting(profile);
  ASSERT_TRUE(manager);
}

IN_PROC_BROWSER_TEST_F(KerberosCredentialsManagerFactoryBrowserTest,
                       GetServiceForIncognitoProfile) {
  Profile* const profile = browser()->profile();
  Browser* incognito_browser = CreateIncognitoBrowser(profile);
  ASSERT_TRUE(incognito_browser);

  Profile* incognito_profile = incognito_browser->profile();
  ASSERT_NE(incognito_profile, profile);
  ASSERT_EQ(incognito_profile->GetOriginalProfile(), profile);

  // Verify, that Get is not creating a new instance for incognito profile.
  KerberosCredentialsManager* manager =
      KerberosCredentialsManagerFactory::GetExisting(profile);
  ASSERT_TRUE(manager);
  ASSERT_EQ(KerberosCredentialsManagerFactory::Get(incognito_profile), manager);

  CloseBrowserSynchronously(incognito_browser);
}

IN_PROC_BROWSER_TEST_F(KerberosCredentialsManagerFactoryBrowserTest,
                       GetServiceForOtherProfile) {
  Profile* const profile = browser()->profile();
  ASSERT_TRUE(ProfileHelper::IsPrimaryProfile(profile));

  Profile* const other_profile = ProfileHelper::GetSigninProfile();
  ASSERT_NE(other_profile, profile);
  ASSERT_NE(other_profile->GetOriginalProfile(), profile);
  ASSERT_TRUE(!ProfileHelper::IsPrimaryProfile(other_profile));

  // Verify, that Get is not creating a new instance for other (non-primary)
  // profile.
  KerberosCredentialsManager* manager =
      KerberosCredentialsManagerFactory::GetExisting(profile);
  ASSERT_TRUE(manager);
  ASSERT_EQ(KerberosCredentialsManagerFactory::Get(other_profile), manager);
}

}  // namespace chromeos
