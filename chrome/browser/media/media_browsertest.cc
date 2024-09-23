// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_browsertest.h"

#include <memory>

#include "base/command_line.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_utils.h"
#include "media/audio/audio_features.h"
#include "media/base/media_switches.h"
#include "media/base/test_data_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

MediaBrowserTest::MediaBrowserTest() {}

MediaBrowserTest::~MediaBrowserTest() {}

void MediaBrowserTest::SetUpCommandLine(base::CommandLine* command_line) {
  command_line->AppendSwitchASCII(
      switches::kAutoplayPolicy,
      switches::autoplay::kNoUserGestureRequiredPolicy);

  std::vector<base::test::FeatureRef> enabled_features = {
    media::kBuiltInH264Decoder,

#if BUILDFLAG(IS_ANDROID)
    features::kLogJsConsoleMessages,
#endif
  };

  std::vector<base::test::FeatureRef> disabled_features = {
    // Disable fallback after decode error to avoid unexpected test pass on
    // the fallback path.
    media::kFallbackAfterDecodeError,

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    // Disable out of process audio on Linux due to process spawn
    // failures. http://crbug.com/986021
    features::kAudioServiceOutOfProcess,
#endif
  };

  scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
}

void MediaBrowserTest::RunMediaTestPage(const std::string& html_page,
                                        const base::StringPairs& query_params,
                                        const std::string& expected_title,
                                        bool http,
                                        bool with_transient_activation) {
  GURL gurl;
  std::string query = media::GetURLQueryString(query_params);
  std::unique_ptr<net::EmbeddedTestServer> http_test_server;
  if (http) {
    DVLOG(0) << base::TimeFormatTimeOfDayWithMilliseconds(base::Time::Now())
             << " Starting HTTP server";
    http_test_server = std::make_unique<net::EmbeddedTestServer>();
    http_test_server->ServeFilesFromSourceDirectory(media::GetTestDataPath());
    CHECK(http_test_server->Start());
    gurl = http_test_server->GetURL("/" + html_page + "?" + query);
  } else {
    gurl = content::GetFileUrlWithQuery(media::GetTestDataFilePath(html_page),
                                        query);
  }
  std::string final_title =
      RunTest(gurl, expected_title, with_transient_activation);
  EXPECT_EQ(expected_title, final_title);
}

std::string MediaBrowserTest::RunTest(const GURL& gurl,
                                      const std::string& expected_title,
                                      bool with_transient_activation) {
  DVLOG(0) << base::TimeFormatTimeOfDayWithMilliseconds(base::Time::Now())
           << " Running test URL: " << gurl;

  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(),
      base::ASCIIToUTF16(expected_title));
  AddWaitForTitles(&title_watcher);
  CHECK(ui_test_utils::NavigateToURL(browser(), gurl));

  if (with_transient_activation) {
    content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(content::ExecJs(tab, "runTest()"));
  }
  std::u16string result = title_watcher.WaitAndGetTitle();
  return base::UTF16ToASCII(result);
}

void MediaBrowserTest::AddWaitForTitles(content::TitleWatcher* title_watcher) {
  title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(media::kEndedTitle));
  title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(media::kErrorTitle));
  title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(media::kFailedTitle));
}
