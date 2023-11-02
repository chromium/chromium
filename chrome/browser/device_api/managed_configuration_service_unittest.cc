// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_api/managed_configuration_service.h"

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "url/gurl.h"

namespace {
const char kUrl[] = "https://example.com";
}

class ManagedConfigurationServiceTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ASSERT_TRUE(profile_manager_.SetUp());
  }

  void TearDown() override {
    profile_manager_.DeleteAllTestingProfiles();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void TryCreatingService(content::WebContents* web_contents) {
    content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents,
                                                               GURL(kUrl));
    ManagedConfigurationServiceImpl::Create(
        web_contents->GetPrimaryMainFrame(),
        remote_.BindNewPipeAndPassReceiver());
  }

  TestingProfileManager* profile_manager() { return &profile_manager_; }
  mojo::Remote<blink::mojom::ManagedConfigurationService>* remote() {
    return &remote_;
  }

 private:
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  mojo::Remote<blink::mojom::ManagedConfigurationService> remote_;
};

TEST_F(ManagedConfigurationServiceTest, Incognito) {
  std::unique_ptr<content::WebContents> incognito_web_contents =
      content::WebContentsTester::CreateTestWebContents(
          profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true), nullptr);
  TryCreatingService(incognito_web_contents.get());

  remote()->FlushForTesting();
  ASSERT_FALSE(remote()->is_connected());
}

TEST_F(ManagedConfigurationServiceTest, NormalProfile) {
  TryCreatingService(web_contents());
  remote()->FlushForTesting();
  ASSERT_TRUE(remote()->is_connected());
}

TEST_F(ManagedConfigurationServiceTest, GuestProfile) {
  std::unique_ptr<content::WebContents> guest_web_contents =
      content::WebContentsTester::CreateTestWebContents(
          profile_manager()->CreateGuestProfile()->GetPrimaryOTRProfile(
              /*create_if_needed=*/true),
          nullptr);
  TryCreatingService(guest_web_contents.get());

  remote()->FlushForTesting();
  ASSERT_FALSE(remote()->is_connected());
}
