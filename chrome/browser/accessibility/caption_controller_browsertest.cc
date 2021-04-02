// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/caption_controller.h"

#include "base/files/file_path.h"
#include "base/ranges/ranges.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/accessibility/caption_controller_factory.h"
#include "chrome/browser/accessibility/caption_host_impl.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/scoped_profile_keep_alive.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/caption_bubble_controller.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/soda/pref_names.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/test/browser_test.h"
#include "media/base/media_switches.h"

namespace captions {

// Blocks until a new profile is created.
void UnblockOnProfileCreation(base::RunLoop* run_loop,
                              Profile* profile,
                              Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_INITIALIZED)
    run_loop->Quit();
}

Profile* CreateProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath profile_path =
      profile_manager->GenerateNextProfileDirectoryPath();
  base::RunLoop run_loop;
  profile_manager->CreateProfileAsync(
      profile_path, base::BindRepeating(&UnblockOnProfileCreation, &run_loop));
  run_loop.Run();
  return profile_manager->GetProfileByPath(profile_path);
}

class CaptionControllerTest : public InProcessBrowserTest {
 public:
  CaptionControllerTest() = default;
  ~CaptionControllerTest() override = default;
  CaptionControllerTest(const CaptionControllerTest&) = delete;
  CaptionControllerTest& operator=(const CaptionControllerTest&) = delete;

  // InProcessBrowserTest overrides:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {media::kLiveCaption, media::kUseSodaForLiveCaption}, {});
    InProcessBrowserTest::SetUp();
  }

  void SetLiveCaptionEnabled(bool enabled) {
    browser()->profile()->GetPrefs()->SetBoolean(prefs::kLiveCaptionEnabled,
                                                 enabled);
    if (enabled)
      speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  }

  void SetLiveCaptionEnabledForProfile(bool enabled, Profile* profile) {
    profile->GetPrefs()->SetBoolean(prefs::kLiveCaptionEnabled, enabled);
    if (enabled)
      speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  }

  CaptionController* GetController() {
    return GetControllerForBrowser(browser());
  }

  CaptionController* GetControllerForBrowser(Browser* browser) {
    return GetControllerForProfile(browser->profile());
  }

  CaptionController* GetControllerForProfile(Profile* profile) {
    return CaptionControllerFactory::GetForProfile(profile);
  }

  CaptionBubbleController* GetBubbleController() {
    return GetBubbleControllerForBrowser(browser());
  }

  CaptionBubbleController* GetBubbleControllerForBrowser(Browser* browser) {
    return GetControllerForBrowser(browser)
        ->GetCaptionBubbleControllerForBrowser(browser);
  }

  CaptionHostImpl* GetCaptionHostImplForBrowser(Browser* browser) {
    if (!caption_host_impls_.count(browser)) {
      caption_host_impls_.emplace(browser, std::make_unique<CaptionHostImpl>(
                                               browser->tab_strip_model()
                                                   ->GetActiveWebContents()
                                                   ->GetMainFrame()));
    }
    return caption_host_impls_[browser].get();
  }

  bool DispatchTranscription(std::string text) {
    return DispatchTranscriptionToBrowser(text, browser());
  }

  bool DispatchTranscriptionToBrowser(std::string text, Browser* browser) {
    return DispatchTranscriptionToBrowserForProfile(text, browser,
                                                    browser->profile());
  }

  bool DispatchTranscriptionToBrowserForProfile(std::string text,
                                                Browser* browser,
                                                Profile* profile) {
    return GetControllerForProfile(profile)->DispatchTranscription(
        GetCaptionHostImplForBrowser(browser),
        chrome::mojom::TranscriptionResult::New(text, false /* is_final */));
  }

  void OnError() { OnErrorOnBrowser(browser()); }

  void OnErrorOnBrowser(Browser* browser) {
    OnErrorOnBrowserForProfile(browser, browser->profile());
  }

  void OnErrorOnBrowserForProfile(Browser* browser, Profile* profile) {
    GetControllerForProfile(profile)->OnError(
        GetCaptionHostImplForBrowser(browser));
  }

  void OnAudioStreamEnd() { OnAudioStreamEndOnBrowser(browser()); }

  void OnAudioStreamEndOnBrowser(Browser* browser) {
    OnAudioStreamEndOnBrowserForProfile(browser, browser->profile());
  }

  void OnAudioStreamEndOnBrowserForProfile(Browser* browser, Profile* profile) {
    GetControllerForProfile(profile)->OnAudioStreamEnd(
        GetCaptionHostImplForBrowser(browser));
  }

  int NumBubbleControllers() {
    return NumBubbleControllersForProfile(browser()->profile());
  }

  int NumBubbleControllersForProfile(Profile* profile) {
    return GetControllerForProfile(profile)->caption_bubble_controllers_.size();
  }

  void ExpectIsWidgetVisible(bool visible) {
    ExpectIsWidgetVisibleOnBrowser(visible, browser());
  }

  void ExpectIsWidgetVisibleOnBrowser(bool visible, Browser* browser) {
#if defined(TOOLKIT_VIEWS)
    EXPECT_EQ(
        visible,
        GetBubbleControllerForBrowser(browser)->IsWidgetVisibleForTesting());
#endif
  }

  void ExpectBubbleLabelTextEquals(std::string text) {
    ExpectBubbleLabelTextOnBrowserEquals(text, browser());
  }

  void ExpectBubbleLabelTextOnBrowserEquals(std::string text,
                                            Browser* browser) {
#if defined(TOOLKIT_VIEWS)
    EXPECT_EQ(
        text,
        GetBubbleControllerForBrowser(browser)->GetBubbleLabelTextForTesting());
#endif
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unordered_map<Browser*, std::unique_ptr<CaptionHostImpl>>
      caption_host_impls_;
};

IN_PROC_BROWSER_TEST_F(CaptionControllerTest, ProfilePrefsAreRegistered) {
  EXPECT_FALSE(
      browser()->profile()->GetPrefs()->GetBoolean(prefs::kLiveCaptionEnabled));
  EXPECT_EQ(base::FilePath(), g_browser_process->local_state()->GetFilePath(
                                  prefs::kSodaBinaryPath));
  EXPECT_EQ(base::FilePath(), g_browser_process->local_state()->GetFilePath(
                                  prefs::kSodaEnUsConfigPath));
}

IN_PROC_BROWSER_TEST_F(CaptionControllerTest,
                       ProfilePrefsAreRegistered_Incognito) {
  // Set live caption enabled on the regular profile.
  SetLiveCaptionEnabled(true);
  EXPECT_TRUE(
      browser()->profile()->GetPrefs()->GetBoolean(prefs::kLiveCaptionEnabled));
  EXPECT_EQ(base::FilePath(), g_browser_process->local_state()->GetFilePath(
                                  prefs::kSodaBinaryPath));
  EXPECT_EQ(base::FilePath(), g_browser_process->local_state()->GetFilePath(
                                  prefs::kSodaEnUsConfigPath));

  // Ensure that live caption is also enabled in the incognito profile.
  Profile* incognito_profile = browser()->profile()->GetPrimaryOTRProfile();
  EXPECT_TRUE(
      incognito_profile->GetPrefs()->GetBoolean(prefs::kLiveCaptionEnabled));
  EXPECT_EQ(base::FilePath(), g_browser_process->local_state()->GetFilePath(
                                  prefs::kSodaBinaryPath));
  EXPECT_EQ(base::FilePath(), g_browser_process->local_state()->GetFilePath(
                                  prefs::kSodaEnUsConfigPath));
}

IN_PROC_BROWSER_TEST_F(CaptionControllerTest, LiveCaptionEnabledChanged) {
  EXPECT_EQ(nullptr, GetBubbleController());
  EXPECT_EQ(0, NumBubbleControllers());

  SetLiveCaptionEnabled(true);
  EXPECT_NE(nullptr, GetBubbleController());
  EXPECT_EQ(1, NumBubbleControllers());

  SetLiveCaptionEnabled(false);
  EXPECT_EQ(nullptr, GetBubbleController());
  EXPECT_EQ(0, NumBubbleControllers());
}

IN_PROC_BROWSER_TEST_F(CaptionControllerTest,
                       LiveCaptionEnabledChanged_BubbleVisible) {
  SetLiveCaptionEnabled(true);
  // Make the bubble visible by dispatching a transcription.
  DispatchTranscription(
      "In Switzerland it is illegal to own just one guinea pig.");
  ExpectIsWidgetVisible(true);

  SetLiveCaptionEnabled(false);
  EXPECT_EQ(nullptr, GetBubbleController());
  EXPECT_EQ(0, NumBubbleControllers());
}

IN_PROC_BROWSER_TEST_F(CaptionControllerTest, OnSodaInstalled) {
  EXPECT_EQ(0, NumBubbleControllers());
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kLiveCaptionEnabled,
                                               true);
  EXPECT_EQ(0, NumBubbleControllers());

  // The UI is only created after SODA is installed.
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  EXPECT_EQ(1, NumBubbleControllers());
}

IN_PROC_BROWSER_TEST_F(CaptionControllerTest, OnBrowserAdded) {
  EXPECT_EQ(0, NumBubbleControllers());

  // Add a new browser and then enable live caption. Test that a caption bubble
  // controller is created.
  CreateBrowser(browser()->profile());
  SetLiveCaptionEnabled(true);
  EXPECT_EQ(2, NumBubbleControllers());

  // Add a new browser and test that a caption bubble controller is created.
  CreateBrowser(browser()->profile());
  EXPECT_EQ(3, NumBubbleControllers());

  // Disable live caption. Add a new browser and test that a caption bubble
  // controller is not created.
  SetLiveCaptionEnabled(false);
  CreateBrowser(browser()->profile());
  EXPECT_EQ(0, NumBubbleControllers());
}

IN_PROC_BROWSER_TEST_F(CaptionControllerTest, OnBrowserAdded_Incognito) {
  EXPECT_EQ(0, NumBubbleControllers());

  // Add a new incognito browser and then enable live caption. Test that a
  // caption bubble controller is created in each browser (incognito and
  // regular).
  CreateIncognitoBrowser();
  SetLiveCaptionEnabled(true);
  EXPECT_EQ(2, NumBubbleControllers());

  // Add a new incognito browser and test that a caption bubble controller is
  // created.
  CreateIncognitoBrowser();
  EXPECT_EQ(3, NumBubbleControllers());

  // Disable live caption. Add a new incognito browser and test that a caption
  // bubble controller is not created.
  SetLiveCaptionEnabled(false);
  CreateIncognitoBrowser();
  EXPECT_EQ(0, NumBubbleControllers());
}

IN_PROC_BROWSER_TEST_F(CaptionControllerTest, OnBrowserRemoved) {
  Profile* profile = browser()->profile();
  CaptionController* controller = GetController();
  Browser* browser1 = browser();
  // Add 3 browsers.
  Browser* browser2 = CreateBrowser(profile);
  Browser* browser3 = CreateBrowser(profile);
  Browser* browser4 = CreateBrowser(profile);

  SetLiveCaptionEnabled(true);
  EXPECT_EQ(4, NumBubbleControllers());

  // Close browser4 and test that the caption bubble controller was destroyed.
  browser4->window()->Close();
  ui_test_utils::WaitForBrowserToClose();
  EXPECT_EQ(nullptr,
            controller->GetCaptionBubbleControllerForBrowser(browser4));
  EXPECT_EQ(3, NumBubbleControllers());

  // Make the bubble on browser3 visible by dispatching a transcription.
  DispatchTranscriptionToBrowser(
      "If you lift a kangaroo's tail off the ground it can't hop.", browser3);
  ExpectIsWidgetVisibleOnBrowser(true, browser3);
  browser3->window()->Close();
  ui_test_utils::WaitForBrowserToClose();
  EXPECT_EQ(nullptr,
            controller->GetCaptionBubbleControllerForBrowser(browser3));
  EXPECT_EQ(2, NumBubbleControllers());

  // Make the bubble on browser2 visible by dispatching a transcription.
  DispatchTranscriptionToBrowser(
      "A lion's roar can be heard from 5 miles away.", browser2);
  ExpectIsWidgetVisibleOnBrowser(true, browser2);

  // Close all browsers and verify that the caption bubbles are destroyed on
  // the two remaining browsers.
  chrome::CloseAllBrowsers();
  ui_test_utils::WaitForBrowserToClose();
  ui_test_utils::WaitForBrowserToClose();

  if (base::FeatureList::IsEnabled(features::kDestroyProfileOnBrowserClose)) {
    // With DestroyProfileOnBrowserClose, the Profile* is deleted entirely.
    //
    // TODO(crbug.com/88586): Remove the other branch once
    // DestroyProfileOnBrowserClose becomes the default.
    base::RunLoop().RunUntilIdle();
    std::vector<Profile*> loaded_profiles =
        g_browser_process->profile_manager()->GetLoadedProfiles();
    auto it = base::ranges::find(loaded_profiles, profile);
    EXPECT_EQ(loaded_profiles.end(), it);
  } else {
    EXPECT_EQ(nullptr,
              controller->GetCaptionBubbleControllerForBrowser(browser2));
    EXPECT_EQ(nullptr,
              controller->GetCaptionBubbleControllerForBrowser(browser1));
  }
}

IN_PROC_BROWSER_TEST_F(CaptionControllerTest, OnBrowserRemoved_Incognito) {
  CaptionController* controller = GetController();
  Browser* incognito_browser1 = CreateIncognitoBrowser();
  Browser* incognito_browser2 = CreateIncognitoBrowser();

  SetLiveCaptionEnabled(true);
  // There is 1 regular browser and 2 incognito browsers.
  EXPECT_EQ(3, NumBubbleControllers());

  // Close incognito_browser2 and test that the caption bubble controller was
  // destroyed.
  incognito_browser2->window()->Close();
  ui_test_utils::WaitForBrowserToClose();
  EXPECT_EQ(nullptr, controller->GetCaptionBubbleControllerForBrowser(
                         incognito_browser2));
  EXPECT_EQ(2, NumBubbleControllers());

  // Make the bubble on incognito_browser1 visible by dispatching a
  // transcription.
  DispatchTranscriptionToBrowser(
      "If you lift a kangaroo's tail off the ground it can't hop.",
      incognito_browser1);
  ExpectIsWidgetVisibleOnBrowser(true, incognito_browser1);
  incognito_browser1->window()->Close();
  ui_test_utils::WaitForBrowserToClose();
  EXPECT_EQ(nullptr, controller->GetCaptionBubbleControllerForBrowser(
                         incognito_browser1));
  EXPECT_EQ(1, NumBubbleControllers());
}

IN_PROC_BROWSER_TEST_F(CaptionControllerTest, DispatchTranscription) {
  bool success = DispatchTranscription("A baby spider is called a spiderling.");
  EXPECT_FALSE(success);
  EXPECT_EQ(0, NumBubbleControllers());

  SetLiveCaptionEnabled(true);
  success = DispatchTranscription(
      "A baby octopus is about the size of a flea when it is born.");
  EXPECT_TRUE(success);
  ExpectIsWidgetVisible(true);
  ExpectBubbleLabelTextEquals(
      "A baby octopus is about the size of a flea when it is born.");

  SetLiveCaptionEnabled(false);
  success = DispatchTranscription(
      "Approximately 10-20% of power outages in the US are caused by "
      "squirrels.");
  EXPECT_FALSE(success);
  EXPECT_EQ(0, NumBubbleControllers());
}

IN_PROC_BROWSER_TEST_F(CaptionControllerTest,
                       DispatchTranscription_MultipleBrowsers) {
  Browser* browser1 = browser();
  Browser* browser2 = CreateBrowser(browser()->profile());
  Browser* incognito_browser = CreateIncognitoBrowser();
  SetLiveCaptionEnabled(true);

  // Dispatch transcription routes the transcription to the right browser.
  bool success = DispatchTranscriptionToBrowser(
      "Honeybees can recognize human faces.", browser1);
  EXPECT_TRUE(success);
  ExpectIsWidgetVisibleOnBrowser(true, browser1);
  ExpectIsWidgetVisibleOnBrowser(false, browser2);
  ExpectIsWidgetVisibleOnBrowser(false, incognito_browser);
  ExpectBubbleLabelTextOnBrowserEquals("Honeybees can recognize human faces.",
                                       browser1);
  ExpectBubbleLabelTextOnBrowserEquals("", browser2);
  ExpectBubbleLabelTextOnBrowserEquals("", incognito_browser);

  success = DispatchTranscriptionToBrowser(
      "A blue whale's heart is the size of a small car.", browser2);
  EXPECT_TRUE(success);
  ExpectIsWidgetVisibleOnBrowser(true, browser1);
  ExpectIsWidgetVisibleOnBrowser(true, browser2);
  ExpectIsWidgetVisibleOnBrowser(false, incognito_browser);
  ExpectBubbleLabelTextOnBrowserEquals(
      "A blue whale's heart is the size of a small car.", browser2);
  ExpectBubbleLabelTextOnBrowserEquals("Honeybees can recognize human faces.",
                                       browser1);
  ExpectBubbleLabelTextOnBrowserEquals("", incognito_browser);

  success = DispatchTranscriptionToBrowser(
      "Squirrels forget where they hide about half of their nuts.",
      incognito_browser);
  EXPECT_TRUE(success);
  ExpectIsWidgetVisibleOnBrowser(true, browser1);
  ExpectIsWidgetVisibleOnBrowser(true, browser2);
  ExpectIsWidgetVisibleOnBrowser(true, incognito_browser);
  ExpectBubbleLabelTextOnBrowserEquals(
      "A blue whale's heart is the size of a small car.", browser2);
  ExpectBubbleLabelTextOnBrowserEquals("Honeybees can recognize human faces.",
                                       browser1);
  ExpectBubbleLabelTextOnBrowserEquals(
      "Squirrels forget where they hide about half of their nuts.",
      incognito_browser);
}

IN_PROC_BROWSER_TEST_F(CaptionControllerTest, OnError) {
  OnError();
  EXPECT_EQ(0, NumBubbleControllers());

  SetLiveCaptionEnabled(true);
  OnError();
  ExpectIsWidgetVisible(true);

  SetLiveCaptionEnabled(false);
  OnError();
  EXPECT_EQ(0, NumBubbleControllers());
}

IN_PROC_BROWSER_TEST_F(CaptionControllerTest, OnError_MultipleBrowsers) {
  Browser* browser1 = browser();
  Browser* browser2 = CreateBrowser(browser()->profile());
  Browser* incognito_browser = CreateIncognitoBrowser();
  SetLiveCaptionEnabled(true);

  // OnError routes to the right browser.
  OnErrorOnBrowser(browser1);
  ExpectIsWidgetVisibleOnBrowser(true, browser1);
  ExpectIsWidgetVisibleOnBrowser(false, browser2);
  ExpectIsWidgetVisibleOnBrowser(false, incognito_browser);

  OnErrorOnBrowser(browser2);
  ExpectIsWidgetVisibleOnBrowser(true, browser1);
  ExpectIsWidgetVisibleOnBrowser(true, browser2);
  ExpectIsWidgetVisibleOnBrowser(false, incognito_browser);

  OnErrorOnBrowser(incognito_browser);
  ExpectIsWidgetVisibleOnBrowser(true, browser1);
  ExpectIsWidgetVisibleOnBrowser(true, browser2);
  ExpectIsWidgetVisibleOnBrowser(true, incognito_browser);
}

IN_PROC_BROWSER_TEST_F(CaptionControllerTest, OnAudioStreamEnd) {
  OnAudioStreamEnd();
  EXPECT_EQ(0, NumBubbleControllers());

  SetLiveCaptionEnabled(true);
  DispatchTranscription("Some cicadas appear only once every 17 years.");
  ExpectIsWidgetVisible(true);

  OnAudioStreamEnd();
  ExpectIsWidgetVisible(false);

  SetLiveCaptionEnabled(false);
  OnAudioStreamEnd();
  EXPECT_EQ(0, NumBubbleControllers());
}

IN_PROC_BROWSER_TEST_F(CaptionControllerTest,
                       OnAudioStreamEnd_MultipleBrowsers) {
  Browser* browser1 = browser();
  Browser* browser2 = CreateBrowser(browser()->profile());
  Browser* incognito_browser = CreateIncognitoBrowser();
  SetLiveCaptionEnabled(true);
  DispatchTranscriptionToBrowser("Ladybugs are beetles, not bugs.", browser1);
  DispatchTranscriptionToBrowser("Ladybugs eat 5000 bugs in their lifetimes.",
                                 browser2);
  DispatchTranscriptionToBrowser("Ladybugs have up to 20 spots.",
                                 incognito_browser);
  ExpectIsWidgetVisibleOnBrowser(true, browser1);
  ExpectIsWidgetVisibleOnBrowser(true, browser2);
  ExpectIsWidgetVisibleOnBrowser(true, incognito_browser);

  // OnAudioStreamEnd routes to the right browser.
  OnAudioStreamEndOnBrowser(browser1);
  ExpectIsWidgetVisibleOnBrowser(false, browser1);
  ExpectIsWidgetVisibleOnBrowser(true, browser2);
  ExpectIsWidgetVisibleOnBrowser(true, incognito_browser);

  OnAudioStreamEndOnBrowser(browser2);
  ExpectIsWidgetVisibleOnBrowser(false, browser1);
  ExpectIsWidgetVisibleOnBrowser(false, browser2);
  ExpectIsWidgetVisibleOnBrowser(true, incognito_browser);

  OnAudioStreamEndOnBrowser(incognito_browser);
  ExpectIsWidgetVisibleOnBrowser(false, browser1);
  ExpectIsWidgetVisibleOnBrowser(false, browser2);
  ExpectIsWidgetVisibleOnBrowser(false, incognito_browser);
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)  // No multi-profile on ChromeOS.

IN_PROC_BROWSER_TEST_F(CaptionControllerTest,
                       LiveCaptionEnabledChanged_MultipleProfiles) {
  Profile* profile1 = browser()->profile();
  Profile* profile2 = CreateProfile();
  CreateBrowser(profile2);

  // The profiles start with no caption bubble controllers.
  EXPECT_EQ(0, NumBubbleControllersForProfile(profile1));
  EXPECT_EQ(0, NumBubbleControllersForProfile(profile2));

  // Enable live caption on profile1.
  SetLiveCaptionEnabled(true);
  EXPECT_EQ(1, NumBubbleControllersForProfile(profile1));
  EXPECT_EQ(0, NumBubbleControllersForProfile(profile2));

  // Enable live caption on profile2.
  SetLiveCaptionEnabledForProfile(true, profile2);
  EXPECT_EQ(1, NumBubbleControllersForProfile(profile1));
  EXPECT_EQ(1, NumBubbleControllersForProfile(profile2));

  // Disable live caption on profile1.
  SetLiveCaptionEnabled(false);
  EXPECT_EQ(0, NumBubbleControllersForProfile(profile1));
  EXPECT_EQ(1, NumBubbleControllersForProfile(profile2));

  // Disable live caption on profile2.
  SetLiveCaptionEnabledForProfile(false, profile2);
  EXPECT_EQ(0, NumBubbleControllersForProfile(profile1));
  EXPECT_EQ(0, NumBubbleControllersForProfile(profile2));
}

IN_PROC_BROWSER_TEST_F(CaptionControllerTest, OnBrowserAdded_MultipleProfiles) {
  Profile* profile1 = browser()->profile();
  Profile* profile2 = CreateProfile();

  // Enable live caption on both profiles.
  SetLiveCaptionEnabled(true);
  SetLiveCaptionEnabledForProfile(true, profile2);

  // Add a new browser to profile1. Test that there are caption bubble
  // controllers on all of the existing browsers.
  CreateBrowser(profile1);
  EXPECT_EQ(2, NumBubbleControllersForProfile(profile1));
  EXPECT_EQ(0, NumBubbleControllersForProfile(profile2));

  // Add a new browser to profile 2. Test that a caption bubble controller is
  // created in profile2 and not in profile1.
  Browser* profile2_browser2 = CreateBrowser(profile2);
  EXPECT_NE(
      nullptr,
      GetControllerForProfile(profile2)->GetCaptionBubbleControllerForBrowser(
          profile2_browser2));
  EXPECT_EQ(
      nullptr,
      GetControllerForProfile(profile1)->GetCaptionBubbleControllerForBrowser(
          profile2_browser2));
  EXPECT_EQ(2, NumBubbleControllersForProfile(profile1));
  EXPECT_EQ(1, NumBubbleControllersForProfile(profile2));

  // Disable live caption on profile1. Add a new browser to both profiles, and
  // test that a caption bubble controller is only created on profile2.
  SetLiveCaptionEnabled(false);
  EXPECT_EQ(0, NumBubbleControllersForProfile(profile1));
  EXPECT_EQ(1, NumBubbleControllersForProfile(profile2));
  CreateBrowser(profile1);
  CreateBrowser(profile2);
  EXPECT_EQ(0, NumBubbleControllersForProfile(profile1));
  EXPECT_EQ(2, NumBubbleControllersForProfile(profile2));
}

IN_PROC_BROWSER_TEST_F(CaptionControllerTest,
                       OnBrowserRemoved_MultipleProfiles) {
  Profile* profile1 = browser()->profile();
  Profile* profile2 = CreateProfile();
  Browser* browser1 = browser();
  Browser* browser2 = CreateBrowser(profile2);
  CaptionController* controller1 = GetControllerForProfile(profile1);
  CaptionController* controller2 = GetControllerForProfile(profile2);

  // TODO(crbug.com/88586): Remove this test when the
  // DestroyProfileOnBrowserClose flag is removed.
  ScopedProfileKeepAlive profile1_keep_alive(
      profile1, ProfileKeepAliveOrigin::kBrowserWindow);
  ScopedProfileKeepAlive profile2_keep_alive(
      profile2, ProfileKeepAliveOrigin::kBrowserWindow);

  // Enable live caption on both profiles.
  SetLiveCaptionEnabled(true);
  SetLiveCaptionEnabledForProfile(true, profile2);
  EXPECT_EQ(1, NumBubbleControllersForProfile(profile1));
  EXPECT_EQ(1, NumBubbleControllersForProfile(profile2));

  // Close browser2 and test that the caption bubble controller was destroyed.
  browser2->window()->Close();
  ui_test_utils::WaitForBrowserToClose();
  EXPECT_EQ(nullptr,
            controller2->GetCaptionBubbleControllerForBrowser(browser2));
  EXPECT_EQ(1, NumBubbleControllersForProfile(profile1));
  EXPECT_EQ(0, NumBubbleControllersForProfile(profile2));

  // Make the bubble on incognito_browser1 visible by dispatching a
  // transcription.
  DispatchTranscriptionToBrowser(
      "If you lift a kangaroo's tail off the ground it can't hop.", browser1);
  ExpectIsWidgetVisibleOnBrowser(true, browser1);
  browser1->window()->Close();
  ui_test_utils::WaitForBrowserToClose();
  EXPECT_EQ(nullptr,
            controller1->GetCaptionBubbleControllerForBrowser(browser1));
  EXPECT_EQ(0, NumBubbleControllersForProfile(profile1));
  EXPECT_EQ(0, NumBubbleControllersForProfile(profile2));
}

IN_PROC_BROWSER_TEST_F(CaptionControllerTest,
                       DispatchTranscription_MultipleProfiles) {
  Profile* profile1 = browser()->profile();
  Profile* profile2 = CreateProfile();
  Browser* browser1 = browser();
  Browser* browser2 = CreateBrowser(profile2);

  // Enable live caption on both profiles.
  SetLiveCaptionEnabled(true);
  SetLiveCaptionEnabledForProfile(true, profile2);

  // Dispatch transcription routes the transcription to the right browser on the
  // right profile.
  bool success = DispatchTranscriptionToBrowserForProfile(
      "Only female mosquitos bite.", browser1, profile1);
  EXPECT_TRUE(success);
  ExpectIsWidgetVisibleOnBrowser(true, browser1);
  ExpectIsWidgetVisibleOnBrowser(false, browser2);
  ExpectBubbleLabelTextOnBrowserEquals("Only female mosquitos bite.", browser1);
  ExpectBubbleLabelTextOnBrowserEquals("", browser2);

  success = DispatchTranscriptionToBrowserForProfile(
      "Mosquitos were around at the time of the dinosaurs.", browser2,
      profile2);
  EXPECT_TRUE(success);
  ExpectIsWidgetVisibleOnBrowser(true, browser1);
  ExpectIsWidgetVisibleOnBrowser(true, browser2);
  ExpectBubbleLabelTextOnBrowserEquals("Only female mosquitos bite.", browser1);
  ExpectBubbleLabelTextOnBrowserEquals(
      "Mosquitos were around at the time of the dinosaurs.", browser2);

  // Dispatch transcription returns false for browsers on different profiles.
  success = DispatchTranscriptionToBrowserForProfile(
      "There are over 3000 species of mosquitos.", browser1, profile2);
  EXPECT_FALSE(success);
  ExpectIsWidgetVisibleOnBrowser(true, browser1);
  ExpectIsWidgetVisibleOnBrowser(true, browser2);
  ExpectBubbleLabelTextOnBrowserEquals("Only female mosquitos bite.", browser1);
  ExpectBubbleLabelTextOnBrowserEquals(
      "Mosquitos were around at the time of the dinosaurs.", browser2);
}

IN_PROC_BROWSER_TEST_F(CaptionControllerTest, OnError_MultipleProfiles) {
  Profile* profile1 = browser()->profile();
  Profile* profile2 = CreateProfile();
  Browser* browser1 = browser();
  Browser* browser2 = CreateBrowser(profile2);

  // Enable live caption on both profiles.
  SetLiveCaptionEnabled(true);
  SetLiveCaptionEnabledForProfile(true, profile2);

  // OnError routes to the right browser on the right profile.
  OnErrorOnBrowserForProfile(browser1, profile1);
  ExpectIsWidgetVisibleOnBrowser(true, browser1);
  ExpectIsWidgetVisibleOnBrowser(false, browser2);

  OnErrorOnBrowserForProfile(browser2, profile2);
  ExpectIsWidgetVisibleOnBrowser(true, browser1);
  ExpectIsWidgetVisibleOnBrowser(true, browser2);

  // OnError does nothing when sent to browsers on different profiles.
  OnErrorOnBrowserForProfile(browser1, profile2);
  ExpectIsWidgetVisibleOnBrowser(true, browser1);
  ExpectIsWidgetVisibleOnBrowser(true, browser2);
}

IN_PROC_BROWSER_TEST_F(CaptionControllerTest,
                       OnAudioStreamEnd_MultipleProfiles) {
  Profile* profile1 = browser()->profile();
  Profile* profile2 = CreateProfile();
  Browser* browser1 = browser();
  Browser* browser2 = CreateBrowser(profile2);

  // Enable live caption on both profiles.
  SetLiveCaptionEnabled(true);
  SetLiveCaptionEnabledForProfile(true, profile2);

  DispatchTranscriptionToBrowserForProfile(
      "Capybaras are the largest rodents in the world.", browser1, profile1);
  DispatchTranscriptionToBrowserForProfile(
      "Capybaras' teeth grow continuously.", browser2, profile2);
  ExpectIsWidgetVisibleOnBrowser(true, browser1);
  ExpectIsWidgetVisibleOnBrowser(true, browser2);

  // OnAudioStreamEnd routes to the right browser on the right profile.
  OnAudioStreamEndOnBrowserForProfile(browser1, profile1);
  ExpectIsWidgetVisibleOnBrowser(false, browser1);
  ExpectIsWidgetVisibleOnBrowser(true, browser2);

  OnAudioStreamEndOnBrowserForProfile(browser2, profile2);
  ExpectIsWidgetVisibleOnBrowser(false, browser1);
  ExpectIsWidgetVisibleOnBrowser(false, browser2);

  // OnAudioStreamEnd does nothing when sent to browsers on different profiles.
  OnAudioStreamEndOnBrowserForProfile(browser1, profile2);
  ExpectIsWidgetVisibleOnBrowser(false, browser1);
  ExpectIsWidgetVisibleOnBrowser(false, browser2);
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace captions
