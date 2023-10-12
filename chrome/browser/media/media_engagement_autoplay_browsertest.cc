// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/chrome_content_settings_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/media/media_engagement_preloaded_list.h"
#include "chrome/browser/media/media_engagement_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

base::FilePath GetPythonPath() {
  // Every environment should have python3.
  return base::FilePath(FILE_PATH_LITERAL("python3"));
}

const base::FilePath kTestDataPath = base::FilePath(
    FILE_PATH_LITERAL("chrome/test/data/media/engagement/preload"));

const char kMediaEngagementTestDataPath[] = "chrome/test/data/media/engagement";

const std::u16string kAllowedTitle = u"Allowed";

const std::u16string kDeniedTitle = u"Denied";

const base::FilePath kEmptyDataPath = kTestDataPath.AppendASCII("empty.pb");

const std::vector<base::test::FeatureRef> kFeatures = {
    media::kMediaEngagementBypassAutoplayPolicies,
    media::kPreloadMediaEngagementData};

}  // namespace

// Class used to test that origins with a high Media Engagement score
// can bypass autoplay policies.
class MediaEngagementAutoplayBrowserTest
    : public testing::WithParamInterface<bool>,
      public InProcessBrowserTest {
 public:
  MediaEngagementAutoplayBrowserTest() {
    http_server_.ServeFilesFromSourceDirectory(kMediaEngagementTestDataPath);
    http_server_origin2_.ServeFilesFromSourceDirectory(
        kMediaEngagementTestDataPath);

    // Enable or disable MEI based on the test parameter.
    if (GetParam()) {
      scoped_feature_list_.InitWithFeatures(kFeatures, {});
    } else {
      scoped_feature_list_.InitWithFeatures({}, kFeatures);
    }
  }

  ~MediaEngagementAutoplayBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kDocumentUserActivationRequiredPolicy);
  }

  void SetUp() override {
    ASSERT_TRUE(http_server_.Start());
    ASSERT_TRUE(http_server_origin2_.Start());

    InProcessBrowserTest::SetUp();

    // Clear any preloaded MEI data.
    ApplyEmptyPreloadedList();
  }

  void LoadTestPage(const std::string& page) {
    NavigateParams params(browser()->profile(), http_server_.GetURL("/" + page),
                          ui::PageTransition::PAGE_TRANSITION_LINK);
    params.user_gesture = false;
    params.is_renderer_initiated = false;
    ui_test_utils::NavigateToURL(&params);
  }

  void LoadTestPageSecondaryOrigin(const std::string& page) {
    NavigateParams params(browser()->profile(),
                          http_server_origin2_.GetURL("/" + page),
                          ui::PageTransition::PAGE_TRANSITION_LINK);
    params.user_gesture = false;
    params.is_renderer_initiated = false;
    ui_test_utils::NavigateToURL(&params);
  }

  void LoadSubFrame(const std::string& page) {
    EXPECT_TRUE(content::ExecJs(
        GetWebContents(),
        "document.getElementsByName('subframe')[0].src = \"" +
            http_server_origin2_.GetURL("/" + page).spec() + "\"",
        content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  }

  void SetScores(const url::Origin& origin, int visits, int media_playbacks) {
    MediaEngagementScore score = GetService()->CreateEngagementScore(origin);
    score.SetVisits(visits);
    score.SetMediaPlaybacks(media_playbacks);
    score.Commit();
  }

  url::Origin PrimaryOrigin() const {
    return url::Origin::Create(http_server_.GetURL("/"));
  }

  url::Origin SecondaryOrigin() const {
    return url::Origin::Create(http_server_origin2_.GetURL("/"));
  }

  void ExpectAutoplayAllowedIfEnabled() {
    if (GetParam()) {
      ExpectAutoplayAllowed();
    } else {
      ExpectAutoplayDenied();
    }
  }

  void ExpectAutoplayAllowed() { EXPECT_EQ(kAllowedTitle, WaitAndGetTitle()); }

  void ExpectAutoplayDenied() { EXPECT_EQ(kDeniedTitle, WaitAndGetTitle()); }

  void ApplyPreloadedOrigin(const url::Origin& origin) {
    base::ScopedAllowBlockingForTesting allow_blocking;

    // Get two temporary files.
    base::FilePath input_path;
    base::FilePath output_path;
    EXPECT_TRUE(base::CreateTemporaryFile(&input_path));
    EXPECT_TRUE(base::CreateTemporaryFile(&output_path));

    // Write JSON file with the server origin in it.
    base::Value::List list;
    list.Append(origin.Serialize());
    std::string json_data;
    base::JSONWriter::Write(list, &json_data);
    EXPECT_TRUE(base::WriteFile(input_path, json_data));

    // Get the source root. The make_dafsa.py script is in here.
    base::FilePath src_root;
    EXPECT_TRUE(
        base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_root));

    // Get the generated root. The protobuf-generated files are in here.
    base::FilePath gen_root;
    EXPECT_TRUE(
        base::PathService::Get(base::DIR_OUT_TEST_DATA_ROOT, &gen_root));

    // Launch the generator and wait for it to finish.
    base::CommandLine cmd(GetPythonPath());
    cmd.AppendArgPath(src_root.Append(
        FILE_PATH_LITERAL("tools/media_engagement_preload/make_dafsa.py")));
    cmd.AppendArgPath(gen_root);
    cmd.AppendArgPath(input_path);
    cmd.AppendArgPath(output_path);
    base::Process process = base::LaunchProcess(cmd, base::LaunchOptions());
    EXPECT_TRUE(process.WaitForExit(nullptr));

    // Load the preloaded list.
    EXPECT_TRUE(
        MediaEngagementPreloadedList::GetInstance()->LoadFromFile(output_path));
  }

  void ApplyEmptyPreloadedList() {
    // Get the path relative to the source root.
    base::FilePath source_root;
    EXPECT_TRUE(
        base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root));

    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(MediaEngagementPreloadedList::GetInstance()->LoadFromFile(
        source_root.Append(kEmptyDataPath)));
  }

 private:
  std::u16string WaitAndGetTitle() {
    content::TitleWatcher title_watcher(GetWebContents(), kAllowedTitle);
    title_watcher.AlsoWaitForTitle(kDeniedTitle);
    return title_watcher.WaitAndGetTitle();
  }

  MediaEngagementService* GetService() {
    return MediaEngagementService::Get(browser()->profile());
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  net::EmbeddedTestServer http_server_;
  net::EmbeddedTestServer http_server_origin2_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(MediaEngagementAutoplayBrowserTest,
                       BypassAutoplayHighEngagement) {
  SetScores(PrimaryOrigin(), 20, 20);
  LoadTestPage("engagement_autoplay_test.html");
  ExpectAutoplayAllowedIfEnabled();
}

IN_PROC_BROWSER_TEST_P(MediaEngagementAutoplayBrowserTest,
                       DoNotBypassAutoplayLowEngagement) {
  SetScores(PrimaryOrigin(), 1, 1);
  LoadTestPage("engagement_autoplay_test.html");
  ExpectAutoplayDenied();
}

IN_PROC_BROWSER_TEST_P(MediaEngagementAutoplayBrowserTest,
                       DoNotBypassAutoplayNoEngagement) {
  LoadTestPage("engagement_autoplay_test.html");
  ExpectAutoplayDenied();
}

IN_PROC_BROWSER_TEST_P(MediaEngagementAutoplayBrowserTest,
                       DoNotBypassAutoplayFrameHighEngagement_NoDelegation) {
  SetScores(PrimaryOrigin(), 20, 20);
  LoadTestPage("engagement_autoplay_iframe_test.html");
  LoadSubFrame("engagement_autoplay_iframe_test_frame.html");
  ExpectAutoplayDenied();
}

IN_PROC_BROWSER_TEST_P(MediaEngagementAutoplayBrowserTest,
                       DoNotBypassAutoplayFrameLowEngagement_NoDelegation) {
  SetScores(SecondaryOrigin(), 20, 20);
  LoadTestPage("engagement_autoplay_iframe_test.html");
  LoadSubFrame("engagement_autoplay_iframe_test_frame.html");
  ExpectAutoplayDenied();
}

IN_PROC_BROWSER_TEST_P(MediaEngagementAutoplayBrowserTest,
                       BypassAutoplayFrameHighEngagement_Delegation) {
  SetScores(PrimaryOrigin(), 20, 20);
  LoadTestPage("engagement_autoplay_iframe_delegation.html");
  LoadSubFrame("engagement_autoplay_iframe_test_frame.html");
  ExpectAutoplayAllowedIfEnabled();
}

IN_PROC_BROWSER_TEST_P(MediaEngagementAutoplayBrowserTest,
                       DoNotBypassAutoplayFrameLowEngagement_Delegation) {
  SetScores(SecondaryOrigin(), 20, 20);
  LoadTestPage("engagement_autoplay_iframe_delegation.html");
  LoadSubFrame("engagement_autoplay_iframe_test_frame.html");
  ExpectAutoplayDenied();
}

IN_PROC_BROWSER_TEST_P(MediaEngagementAutoplayBrowserTest,
                       DoNotBypassAutoplayFrameNoEngagement) {
  LoadTestPage("engagement_autoplay_iframe_test.html");
  LoadSubFrame("engagement_autoplay_iframe_test_frame.html");
  ExpectAutoplayDenied();
}

IN_PROC_BROWSER_TEST_P(MediaEngagementAutoplayBrowserTest,
                       ClearEngagementOnNavigation) {
  SetScores(PrimaryOrigin(), 20, 20);
  LoadTestPage("engagement_autoplay_test.html");
  ExpectAutoplayAllowedIfEnabled();

  LoadTestPageSecondaryOrigin("engagement_autoplay_test.html");
  ExpectAutoplayDenied();
  SetScores(SecondaryOrigin(), 20, 20);

  LoadTestPage("engagement_autoplay_test.html");
  ExpectAutoplayAllowedIfEnabled();

  LoadTestPageSecondaryOrigin("engagement_autoplay_test.html");
  ExpectAutoplayAllowedIfEnabled();
}

// Test have high score threshold.
IN_PROC_BROWSER_TEST_P(MediaEngagementAutoplayBrowserTest,
                       HasHighScoreThreshold) {
  SetScores(PrimaryOrigin(), 20, 16);
  SetScores(PrimaryOrigin(), 20, 10);
  LoadTestPage("engagement_autoplay_test.html");
  ExpectAutoplayAllowedIfEnabled();
}

// Disabled due to being flaky. crbug.com/1212507
#if BUILDFLAG(IS_MAC)
#define MAYBE_UsePreloadedData_Allowed DISABLED_UsePreloadedData_Allowed
#else
#define MAYBE_UsePreloadedData_Allowed UsePreloadedData_Allowed
#endif
IN_PROC_BROWSER_TEST_P(MediaEngagementAutoplayBrowserTest,
                       MAYBE_UsePreloadedData_Allowed) {
  // Autoplay should be blocked by default if we have a bad score.
  SetScores(PrimaryOrigin(), 0, 0);
  LoadTestPage("engagement_autoplay_test.html");
  ExpectAutoplayDenied();

  // Load the preloaded data and we should now be able to autoplay.
  ApplyPreloadedOrigin(PrimaryOrigin());
  LoadTestPage("engagement_autoplay_test.html");
  ExpectAutoplayAllowedIfEnabled();

  // If we now have a high MEI score we should still be allowed to autoplay.
  SetScores(PrimaryOrigin(), 20, 20);
  LoadTestPage("engagement_autoplay_test.html");
  ExpectAutoplayAllowedIfEnabled();

  // If we clear the preloaded data we should still be allowed to autoplay.
  ApplyEmptyPreloadedList();
  LoadTestPage("engagement_autoplay_test.html");
  ExpectAutoplayAllowedIfEnabled();
}

IN_PROC_BROWSER_TEST_P(MediaEngagementAutoplayBrowserTest,
                       UsePreloadedData_Denied) {
  // Autoplay should be blocked by default if we have a bad score.
  SetScores(PrimaryOrigin(), 0, 0);
  LoadTestPage("engagement_autoplay_test.html");
  ExpectAutoplayDenied();

  // Load the preloaded data but we are not in that dataset so we should not be
  // allowed to autoplay.
  ApplyPreloadedOrigin(SecondaryOrigin());
  LoadTestPage("engagement_autoplay_test.html");
  ExpectAutoplayDenied();

  // If we now have a high MEI score we should now be allowed to autoplay.
  SetScores(PrimaryOrigin(), 20, 20);
  LoadTestPage("engagement_autoplay_test.html");
  ExpectAutoplayAllowedIfEnabled();

  // If we clear the preloaded data we should still be allowed to autoplay.
  ApplyEmptyPreloadedList();
  LoadTestPage("engagement_autoplay_test.html");
  ExpectAutoplayAllowedIfEnabled();
}

IN_PROC_BROWSER_TEST_P(MediaEngagementAutoplayBrowserTest,
                       PreloadedDataAndHighVisits) {
  // Autoplay should be denied due to a low score.
  SetScores(PrimaryOrigin(), 20, 0);
  LoadTestPage("engagement_autoplay_test.html");
  ExpectAutoplayDenied();

  // Load the preloaded data and even though we are in the dataset we should not
  // be allowed to play.
  ApplyPreloadedOrigin(PrimaryOrigin());
  LoadTestPage("engagement_autoplay_test.html");
  ExpectAutoplayDenied();
}

IN_PROC_BROWSER_TEST_P(MediaEngagementAutoplayBrowserTest, TopFrameNavigation) {
  SetScores(SecondaryOrigin(), 20, 20);
  LoadTestPage("engagement_autoplay_test.html");
  ExpectAutoplayDenied();

  LoadTestPageSecondaryOrigin("engagement_autoplay_test.html");
  ExpectAutoplayAllowedIfEnabled();
}

class MediaEngagementAutoplayBrowserTestHttpsOnly
    : public MediaEngagementAutoplayBrowserTest {
 public:
  MediaEngagementAutoplayBrowserTestHttpsOnly() {
    feature_list_.InitAndEnableFeature(media::kMediaEngagementHTTPSOnly);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(MediaEngagementAutoplayBrowserTestHttpsOnly,
                       BypassAutoplayHighEngagement) {
  SetScores(PrimaryOrigin(), 20, 20);
  LoadTestPage("engagement_autoplay_test.html");
  ExpectAutoplayDenied();
}

INSTANTIATE_TEST_SUITE_P(All,
                         MediaEngagementAutoplayBrowserTest,
                         testing::Bool());

INSTANTIATE_TEST_SUITE_P(All,
                         MediaEngagementAutoplayBrowserTestHttpsOnly,
                         testing::Bool());
