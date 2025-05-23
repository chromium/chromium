// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/supervised_user/browser_user.h"
#include "chrome/test/supervised_user/family_live_test.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/supervised_user/test_support/family_link_settings_state_management.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/state_observer.h"
#include "url/gurl.h"

namespace supervised_user {
namespace {

// Unblocks rendering the youtube iframe.
constexpr std::string_view kYouTubeHostPattern = "*.youtube.com";
// Unblocks playing back the video
constexpr std::string_view kGoogleVideoHostPattern = "*.googlevideo.com";
// When this string is present in frame's HTML, it indicates that the
// interstitial is shown.
constexpr std::string_view kInterstitialBodyClass =
    "supervised-user-verify-subframe";

// Returns true, when in the given `browser`, in a tab with title `tab_title`,
// there is a iframe with name `iframe_name` that displays the YouTube
// interstitial. Note that the interstitial's presence is validated by looking
// for a specific class in the inner HTML of the iframe rather than parsing the
// DOM structure.
bool IsYouTubeInterstitialDisplayedInIframe(Browser& browser,
                                            std::u16string_view tab_title,
                                            std::string_view iframe_name) {
  content::WebContents* web_contents = nullptr;
  TabStripModel* const tab_strip_model = browser.tab_strip_model();
  for (int i = 0; i < tab_strip_model->GetTabCount(); ++i) {
    const std::u16string wc_title = tab_strip_model->GetTabAtIndex(i)
                                        ->GetTabFeatures()
                                        ->tab_ui_helper()
                                        ->GetTitle();
    if (wc_title == tab_title) {
      web_contents = browser.GetTabStripModel()->GetWebContentsAt(i);
      break;
    }
  }
  CHECK(web_contents) << "Expected tab with supplied title";

  content::RenderFrameHost* rfh = content::FrameMatchingPredicate(
      web_contents->GetPrimaryPage(),
      base::BindRepeating(&content::FrameMatchesName, iframe_name));

  CHECK(rfh) << "Expected iframe with supplied name";
  std::string innerHTML =
      content::EvalJs(rfh, "document.documentElement.innerHTML")
          .ExtractString();
  return innerHTML.find(kInterstitialBodyClass) != std::string::npos;
}

// All tests in this unit are subject to flakiness because they interact with a
// system that can be externally modified during execution.
// TODO(crbug.com/301587955): Fix placement of supervised_user/e2e test files
// and their dependencies.
class KidsProfileUiTest
    : public InteractiveFamilyLiveTest,
      public testing::WithParamInterface<FamilyLiveTest::RpcMode> {
 public:
  KidsProfileUiTest()
      : InteractiveFamilyLiveTest(
            GetParam(),
            /*extra_enabled_hosts=*/{kYouTubeHostPattern,
                                     kGoogleVideoHostPattern}) {}

  void SetUpOnMainThread() override {
    InteractiveFamilyLiveTest::SetUpOnMainThread();
    test_server_.AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
    CHECK(test_server_.Start());
    LOG(INFO) << "Embedded test server is listening on "
              << test_server_.host_port_pair().ToString() << ".";
  }

 protected:
  net::test_server::EmbeddedTestServer& TestServer() { return test_server_; }

 private:
  // Serves static page that embeds an arbitrary YouTube widget (actual video is
  // irrelevant).
  net::test_server::EmbeddedTestServer test_server_;
};

IN_PROC_BROWSER_TEST_P(KidsProfileUiTest, DisplayInterstitialInPendingState) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kYouTubeTab);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                      kInterstitialDisplayed);

  TurnOnSync();
  child().SignOutFromWeb();

  GURL url_with_youtube_iframes =
      TestServer().GetURL("/supervised_user/with_embedded_youtube_videos.html");

  Browser& child_browser = child().browser();

  RunTestSequence(
      AddInstrumentedTab(kYouTubeTab, url_with_youtube_iframes, std::nullopt,
                         &child().browser()),

      // Refer to with_embedded_youtube_videos.html to get title and iframe
      // names.
      PollState(
          kInterstitialDisplayed,
          [&child_browser]() {
            return IsYouTubeInterstitialDisplayedInIframe(
                child_browser,
                u"Supervised User test: page with embedded YouTube videos",
                "iframe1");
          }),
      WaitForState(kInterstitialDisplayed, true));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    KidsProfileUiTest,
    testing::Values(FamilyLiveTest::RpcMode::kProd,
                    FamilyLiveTest::RpcMode::kTestImpersonation),
    [](const testing::TestParamInfo<FamilyLiveTest::RpcMode>& info) {
      return ToString(info.param);
    });

}  // namespace
}  // namespace supervised_user
