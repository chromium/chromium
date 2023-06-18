// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/profile_loader.h"

#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/profile_ui_test_utils.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/startup/browser_init_params.h"
#include "content/public/test/browser_test.h"

using ProfileLoaderBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(ProfileLoaderBrowserTest, LoadMainProfile) {
  base::test::TestFuture<Profile*> result;
  LoadMainProfile(result.GetCallback(), /*can_trigger_fre=*/false);
  Profile* profile = result.Get();

  ASSERT_NE(profile, nullptr);
  ASSERT_TRUE(profile->IsMainProfile());
}

class ProfileLoaderGuestSessionBrowserTest : public ProfileLoaderBrowserTest {
 public:
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    crosapi::mojom::BrowserInitParamsPtr init_params =
        chromeos::BrowserInitParams::GetForTests()->Clone();
    init_params->session_type = crosapi::mojom::SessionType::kGuestSession;
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));

    InProcessBrowserTest::CreatedBrowserMainParts(browser_main_parts);
  }
};

IN_PROC_BROWSER_TEST_F(ProfileLoaderGuestSessionBrowserTest, LoadMainProfile) {
  base::test::TestFuture<Profile*> result;
  LoadMainProfile(result.GetCallback(), /*can_trigger_fre=*/false);
  Profile* profile = result.Get();

  ASSERT_NE(profile, nullptr);
  EXPECT_TRUE(profile->IsMainProfile());
  EXPECT_TRUE(profile->IsGuestSession());
  EXPECT_TRUE(profile->IsOffTheRecord());
}
