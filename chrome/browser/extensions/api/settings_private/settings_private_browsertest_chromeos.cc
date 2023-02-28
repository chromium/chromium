// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chrome/browser/ash/login/test/guest_session_mixin.h"
#include "chrome/browser/extensions/api/settings_private/prefs_util_enums.h"
#include "chrome/browser/extensions/api/settings_private/settings_private_api.h"
#include "chrome/browser/extensions/api/settings_private/settings_private_delegate.h"
#include "chrome/browser/extensions/api/settings_private/settings_private_delegate_factory.h"
#include "chrome/browser/extensions/api/settings_private/settings_private_event_router.h"
#include "chrome/browser/extensions/api/settings_private/settings_private_event_router_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

class SettingsPrivateGuestModeTest : public MixinBasedInProcessBrowserTest {
 protected:
  ash::GuestSessionMixin guest_session_{&mixin_host_};
};

// Regression test for https://crbug.com/887383.
IN_PROC_BROWSER_TEST_F(SettingsPrivateGuestModeTest, GuestMode) {
  Profile* guest_profile = browser()->profile();
  EXPECT_TRUE(guest_profile->IsOffTheRecord());

  // SettingsPrivate uses the incognito profile, not the recording profile,
  // to set preferences.
  SettingsPrivateDelegate* delegate =
      SettingsPrivateDelegateFactory::GetForBrowserContext(guest_profile);
  Profile* delegate_profile = delegate->profile_for_test();
  EXPECT_EQ(guest_profile, delegate_profile);
  EXPECT_NE(guest_profile->GetOriginalProfile(), delegate_profile);

  // SettingsPrivate observes the incognito profile, not the recording profile,
  // for pref changes.
  SettingsPrivateEventRouter* router =
      SettingsPrivateEventRouterFactory::GetForProfile(guest_profile);
  Profile* router_profile = static_cast<Profile*>(router->context_for_test());
  EXPECT_EQ(guest_profile, router_profile);
  EXPECT_NE(guest_profile->GetOriginalProfile(), router_profile);

  // Page zoom preferences cannot be changed in guest mode and always return a
  // default value.
  EXPECT_EQ(settings_private::SetPrefResult::PREF_NOT_MODIFIABLE,
            delegate->SetDefaultZoom(0.5));
  EXPECT_EQ(delegate->GetDefaultZoom().GetDouble(), 0.0);
}

}  // namespace
}  // namespace extensions
