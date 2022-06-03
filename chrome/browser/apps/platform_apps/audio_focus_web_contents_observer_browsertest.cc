// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/apps/platform_apps/audio_focus_web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/extension_test_message_listener.h"

namespace apps {

// AudioFocusWebContentsObserverBrowserTest test that apps have separate audio
// focus from the rest of the browser.
class AudioFocusWebContentsObserverBrowserTest
    : public extensions::PlatformAppBrowserTest {
 public:
  AudioFocusWebContentsObserverBrowserTest() = default;
  AudioFocusWebContentsObserverBrowserTest(
      const AudioFocusWebContentsObserverBrowserTest&) = delete;
  AudioFocusWebContentsObserverBrowserTest& operator=(
      const AudioFocusWebContentsObserverBrowserTest&) = delete;
  ~AudioFocusWebContentsObserverBrowserTest() override = default;

  const base::UnguessableToken& GetAudioFocusGroupId(
      content::WebContents* web_contents) {
    AudioFocusWebContentsObserver* wco =
        AudioFocusWebContentsObserver::FromWebContents(web_contents);
    return wco->audio_focus_group_id_;
  }
};

IN_PROC_BROWSER_TEST_F(AudioFocusWebContentsObserverBrowserTest,
                       PlatformAppHasDifferentAudioFocus) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ExtensionTestMessageListener launched_listener("Launched", false);
  const extensions::Extension* extension =
      InstallAndLaunchPlatformApp("minimal");
  ASSERT_TRUE(extension);
  EXPECT_TRUE(extension->is_platform_app());

  // Wait for the extension to tell us it's initialized its context menus and
  // launched a window.
  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());

  // Get the web contents and make sure it has a group id.
  content::WebContents* web_contents = GetFirstAppWindowWebContents();
  EXPECT_TRUE(web_contents);
  EXPECT_NE(base::UnguessableToken::Null(), GetAudioFocusGroupId(web_contents));

  // Create a new window and navigate it to the test app.
  ExtensionTestMessageListener new_launched_listener("Launched", false);
  LaunchPlatformApp(extension);
  ASSERT_TRUE(new_launched_listener.WaitUntilSatisfied());

  // Ensure the new window has the same group id.
  content::WebContents* new_contents = GetFirstAppWindowWebContents();
  EXPECT_TRUE(new_contents);
  EXPECT_NE(web_contents, new_contents);
  EXPECT_EQ(GetAudioFocusGroupId(web_contents),
            GetAudioFocusGroupId(new_contents));
}

}  // namespace apps
