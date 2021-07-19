// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/data_use_measurement/chrome_data_use_measurement.h"
#include "chrome/browser/login_detection/login_detection_util.h"
#include "chrome/browser/subresource_redirect/subresource_redirect_observer.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/browser/data_store_impl.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace subresource_redirect {

class SubresourceRedirectLoginRobotsTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    command_line_.GetProcessCommandLine()->AppendSwitch(
        data_reduction_proxy::switches::kOverrideHttpsImageCompressionInfobar);
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kSubresourceRedirect,
          {{"enable_public_image_hints_based_compression", "false"},
           {"enable_login_robots_based_compression", "true"},
           {"enable_login_robots_for_low_memory", "true"}}},
         {login_detection::kLoginDetection,
          {{"logged_in_sites", "https://loggedin.test"}}}},
        {});

    ChromeRenderViewHostTestHarness::SetUp();

    data_use_measurement::ChromeDataUseMeasurement::CreateInstance(
        g_browser_process->local_state());
    data_reduction_proxy::DataReductionProxySettings::
        SetDataSaverEnabledForTesting(profile()->GetPrefs(), true);
    auto* drp_settings =
        DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
            profile());
    drp_settings->InitDataReductionProxySettings(
        profile(),
        std::make_unique<data_reduction_proxy::DataStoreImpl>(
            profile()->GetPath()),
        task_environment()->GetMainThreadTaskRunner());
  }

  std::unique_ptr<content::NavigationSimulator> CreateAndNavigateFrame(
      const std::string& name,
      const GURL& url) {
    content::RenderFrameHost* rfh =
        content::RenderFrameHostTester::For(main_rfh())->AppendChild(name);
    return content::NavigationSimulator::CreateRendererInitiated(url, rfh);
  }

  bool CompressionWasAppliedTo(content::RenderFrameHost* rfh) {
    return ImageCompressionAppliedDocument::GetState(rfh) ==
           ImageCompressionAppliedDocument::kLoginRobotsCheckedEnabled;
  }

 private:
  base::test::ScopedCommandLine command_line_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(SubresourceRedirectLoginRobotsTest, RaceBetweenFrames) {
  SetContents(CreateTestWebContents());
  SubresourceRedirectObserver::MaybeCreateForWebContents(web_contents());
  ASSERT_TRUE(SubresourceRedirectObserver::FromWebContents(web_contents()))
      << "subresource redirect was not enabled";
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://main.test/"));
  auto simulator_logged_in =
      CreateAndNavigateFrame("loggedin", GURL("https://loggedin.test/"));
  auto simulator_logged_out =
      CreateAndNavigateFrame("loggedout", GURL("https://loggedout.test/"));

  // Since these navigations are occurring at the same time, and the
  // "did finish navigation" signal needs to wait for a renderer ack, it is
  // possible though unlikely for these to arrive at nearly the same time.
  //
  // We deterministically force this to happen here, which requires the
  // implementation to track per-navigation data, and not simply assume that
  // each DidFinishNavigation corresponds to the last ReadyToCommit.
  simulator_logged_in->ReadyToCommit();
  simulator_logged_out->ReadyToCommit();
  simulator_logged_in->Commit();
  simulator_logged_out->Commit();

  // Despite this race, compression should always be applied only to eligible
  // documents.
  EXPECT_FALSE(
      CompressionWasAppliedTo(simulator_logged_in->GetFinalRenderFrameHost()));
  EXPECT_TRUE(
      CompressionWasAppliedTo(simulator_logged_out->GetFinalRenderFrameHost()));
}

}  // namespace subresource_redirect
