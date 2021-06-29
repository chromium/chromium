// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_api/managed_configuration_service.h"

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "url/gurl.h"

namespace {
const char kUrl[] = "https://example.com";
}

class ManagedConfigurationServiceTest : public ChromeRenderViewHostTestHarness {
 public:
  void TryCreatingService(content::WebContents* web_contents) {
    content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents,
                                                               GURL(kUrl));
    ManagedConfigurationServiceImpl::Create(
        web_contents->GetMainFrame(), remote_.BindNewPipeAndPassReceiver());
  }

  mojo::Remote<blink::mojom::ManagedConfigurationService>* remote() {
    return &remote_;
  }

 private:
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
