// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_settings_util.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSettingsTab);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kBubbleIsVisible);

auto BubbleIsVisibleStateChange(
    WebContentsInteractionTestUtil::DeepQuery query) {
  WebContentsInteractionTestUtil::StateChange bubble_is_visible;
  bubble_is_visible.event = kBubbleIsVisible;
  bubble_is_visible.where = query;
  bubble_is_visible.type =
      WebContentsInteractionTestUtil::StateChange::Type::kExists;
  return bubble_is_visible;
}
}  // namespace

class GlicSettingsUtilUiTest : public InteractiveBrowserTest {
 public:
  GlicSettingsUtilUiTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kGlic, features::kTabstripComboButton}, {});
  }
  ~GlicSettingsUtilUiTest() override = default;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    g_browser_process->local_state()->SetBoolean(
        glic::prefs::kGlicLauncherEnabled, true);
  }

  auto VerifyOpensGlicSettings(auto f) {
    return Steps(
        Do([this, f]() { f(browser()->profile()); }),
        InstrumentTab(kSettingsTab),
        WaitForWebContentsReady(
            kSettingsTab, chrome::GetSettingsUrl(chrome::kChromeUIGlicHost)));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicSettingsUtilUiTest, OpenSettings) {
  RunTestSequence(VerifyOpensGlicSettings(glic::OpenGlicSettingsPage));
}

IN_PROC_BROWSER_TEST_F(GlicSettingsUtilUiTest, OpenOsToggleSetting) {
  RunTestSequence(
      VerifyOpensGlicSettings(glic::OpenGlicOsToggleSetting),
      WaitForStateChange(kSettingsTab,
                         BubbleIsVisibleStateChange(
                             {"settings-ui", "settings-main",
                              "settings-basic-page", "settings-glic-page",
                              "#launcherToggle", "help-bubble", "#close"})));
}

IN_PROC_BROWSER_TEST_F(GlicSettingsUtilUiTest, OpenKeyboardShortcutSetting) {
  RunTestSequence(
      VerifyOpensGlicSettings(glic::OpenGlicKeyboardShortcutSetting),
      WaitForStateChange(kSettingsTab,
                         BubbleIsVisibleStateChange(
                             {"settings-ui", "settings-main",
                              "settings-basic-page", "settings-glic-page",
                              "#shortcutInput", "help-bubble", "#close"})));
}
