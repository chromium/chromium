// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/apps/platform_apps/audio_focus_web_contents_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
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

  ExtensionTestMessageListener launched_listener("Launched");
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
  ExtensionTestMessageListener new_launched_listener("Launched");
  LaunchPlatformApp(extension);
  ASSERT_TRUE(new_launched_listener.WaitUntilSatisfied());

  // There should be two app windows, find the "other" one from the first.
  extensions::AppWindowRegistry* app_registry =
      extensions::AppWindowRegistry::Get(browser()->profile());
  const auto& app_windows = app_registry->app_windows();
  ASSERT_EQ(2u, app_windows.size());
  extensions::AppWindow* app_window = *app_windows.begin();
  content::WebContents* new_contents = app_window->web_contents();
  if (new_contents == web_contents) {
    app_window = *(++app_windows.begin());
    new_contents = app_window->web_contents();
  }

  // Ensure the new window has the same group id.
  EXPECT_EQ(GetAudioFocusGroupId(web_contents),
            GetAudioFocusGroupId(new_contents));
}

}  // namespace apps
