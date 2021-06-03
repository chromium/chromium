// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/sound_content_setting_observer.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char* kMediaTestDataPath = "media/test/data";

// Observes a WebContents and waits until it becomes audible.
// both indicate that they are audible.
class TestAudioStartObserver : public content::WebContentsObserver {
 public:
  TestAudioStartObserver(content::WebContents* web_contents,
                         base::OnceClosure quit_closure)
      : content::WebContentsObserver(web_contents),
        quit_closure_(std::move(quit_closure)) {
    DCHECK(!web_contents->IsCurrentlyAudible());
  }
  ~TestAudioStartObserver() override = default;

  // WebContentsObserver:
  void OnAudioStateChanged(bool audible) override {
    if (quit_closure_)
      std::move(quit_closure_).Run();
  }

 private:
  base::OnceClosure quit_closure_;
};

}  // namespace

class SoundContentSettingObserverBrowserTest : public InProcessBrowserTest {
 public:
  SoundContentSettingObserverBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &SoundContentSettingObserverBrowserTest::web_contents,
            base::Unretained(this))) {}
  ~SoundContentSettingObserverBrowserTest() override = default;

  content::test::PrerenderTestHelper* prerender_helper() {
    return &prerender_helper_;
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

// Tests that the prerending doesn't affect SoundContentSettingObserver status
// with the main frame.
IN_PROC_BROWSER_TEST_F(SoundContentSettingObserverBrowserTest,
                       SoundContentSettingObserverInPrerendering) {
  // Sets up the embedded test server to serve the test javascript file.
  embedded_test_server()->ServeFilesFromSourceDirectory(kMediaTestDataPath);
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());

  // Configures to check `logged_site_muted_ukm_` in
  // SoundContentSettingObserver.
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  content_settings->SetDefaultContentSetting(ContentSettingsType::SOUND,
                                             CONTENT_SETTING_BLOCK);

  GURL url = embedded_test_server()->GetURL("/webaudio_oscillator.html");
  ui_test_utils::NavigateToURL(browser(), url);

  // Start the audio on the current main frame.
  base::RunLoop run_loop;
  TestAudioStartObserver audio_start_observer(web_contents(),
                                              run_loop.QuitClosure());
  EXPECT_EQ("OK", content::EvalJs(web_contents(), "StartOscillator();",
                                  content::EXECUTE_SCRIPT_USE_MANUAL_REPLY));
  run_loop.Run();

  SoundContentSettingObserver* observer =
      SoundContentSettingObserver::FromWebContents(web_contents());
  // `logged_site_muted_ukm_` should be set.
  EXPECT_TRUE(observer->HasLoggedSiteMutedUkmForTesting());

  // Loads a page in the prerender.
  auto prerender_url = embedded_test_server()->GetURL("/simple.html");
  int host_id = prerender_helper()->AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);
  // The prerendering should not affect the current status.
  EXPECT_TRUE(observer->HasLoggedSiteMutedUkmForTesting());

  // Activates the page from the prerendering.
  ui_test_utils::NavigateToURL(browser(), prerender_url);
  // Makes sure that the page is activated from the prerendering.
  EXPECT_TRUE(host_observer.was_activated());
  // It should be reset.
  EXPECT_FALSE(observer->HasLoggedSiteMutedUkmForTesting());
}
