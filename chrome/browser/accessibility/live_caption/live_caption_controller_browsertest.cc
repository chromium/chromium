// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/live_caption_controller.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/ranges/ranges.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/accessibility/caption_bubble_context_browser.h"
#include "chrome/browser/accessibility/live_caption/live_caption_controller_factory.h"
#include "chrome/browser/accessibility/live_caption/live_caption_test_util.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/live_caption/caption_bubble_controller.h"
#include "components/live_caption/pref_names.h"
#include "components/soda/pref_names.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif

namespace captions {

namespace {

speech::LanguageCode en_us() {
  return speech::LanguageCode::kEnUs;
}

speech::LanguageCode fr_fr() {
  return speech::LanguageCode::kFrFr;
}

}  // namespace

Profile* CreateProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath profile_path =
      profile_manager->GenerateNextProfileDirectoryPath();
  return &profiles::testing::CreateProfileSync(profile_manager, profile_path);
}

class LiveCaptionControllerTest : public LiveCaptionBrowserTest {
 public:
  LiveCaptionControllerTest() = default;
  ~LiveCaptionControllerTest() override = default;
  LiveCaptionControllerTest(const LiveCaptionControllerTest&) = delete;
  LiveCaptionControllerTest& operator=(const LiveCaptionControllerTest&) =
      delete;

  LiveCaptionController* GetControllerForProfile(Profile* profile) {
    return LiveCaptionControllerFactory::GetForProfile(profile);
  }

  CaptionBubbleController* GetBubbleController() {
    return GetBubbleControllerForProfile(browser()->profile());
  }

  CaptionBubbleController* GetBubbleControllerForProfile(Profile* profile) {
    return GetControllerForProfile(profile)
        ->caption_bubble_controller_for_testing();
  }

  CaptionBubbleContextBrowser* GetCaptionBubbleContextBrowser() {
    if (!caption_bubble_context_) {
      caption_bubble_context_ = CaptionBubbleContextBrowser::Create(
          browser()->tab_strip_model()->GetActiveWebContents());
    }
    return caption_bubble_context_.get();
  }

  bool DispatchTranscription(std::string text) {
    return DispatchTranscriptionToProfile(text, browser()->profile());
  }

  bool DispatchTranscriptionToProfile(std::string text, Profile* profile) {
    return GetControllerForProfile(profile)->DispatchTranscription(
        GetCaptionBubbleContextBrowser(),
        media::SpeechRecognitionResult(text, /* is_final */ false));
  }

  void OnError() { OnErrorOnProfile(browser()->profile()); }

  void OnErrorOnProfile(Profile* profile) {
    GetControllerForProfile(profile)->OnError(
        GetCaptionBubbleContextBrowser(), CaptionBubbleErrorType::kGeneric,
        base::RepeatingClosure(),
        base::BindRepeating(
            [](CaptionBubbleErrorType error_type, bool checked) {}));
  }

  void OnAudioStreamEnd() { OnAudioStreamEndOnProfile(browser()->profile()); }

  void OnAudioStreamEndOnProfile(Profile* profile) {
    GetControllerForProfile(profile)->OnAudioStreamEnd(
        GetCaptionBubbleContextBrowser());
  }

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
  void OnToggleFullscreen() {
    GetControllerForProfile(browser()->profile())
        ->OnToggleFullscreen(GetCaptionBubbleContextBrowser());
  }
#endif

  bool HasBubbleController() {
    return HasBubbleControllerOnProfile(browser()->profile());
  }

  bool HasBubbleControllerOnProfile(Profile* profile) {
    return GetControllerForProfile(profile)
               ->caption_bubble_controller_for_testing() != nullptr;
  }

  void ExpectIsWidgetVisible(bool visible) {
    ExpectIsWidgetVisibleOnProfile(visible, browser()->profile());
  }

  void ExpectIsWidgetVisibleOnProfile(bool visible, Profile* profile) {
#if defined(TOOLKIT_VIEWS)
    EXPECT_EQ(
        visible,
        GetBubbleControllerForProfile(profile)->IsWidgetVisibleForTesting());
#endif
  }

  void ExpectBubbleLabelTextEquals(std::string text) {
    ExpectBubbleLabelTextOnProfileEquals(text, browser()->profile());
  }

  void ExpectBubbleLabelTextOnProfileEquals(std::string text,
                                            Profile* profile) {
#if defined(TOOLKIT_VIEWS)
    EXPECT_EQ(
        text,
        GetBubbleControllerForProfile(profile)->GetBubbleLabelTextForTesting());
#endif
  }

 private:
  std::unique_ptr<CaptionBubbleContextBrowser> caption_bubble_context_;
};

IN_PROC_BROWSER_TEST_F(LiveCaptionControllerTest, ProfilePrefsAreRegistered) {
  EXPECT_FALSE(
      browser()->profile()->GetPrefs()->GetBoolean(prefs::kLiveCaptionEnabled));

#if !BUILDFLAG(IS_CHROMEOS)
  // These prefs are used for the component updater, but SODA does not use the
  // component updater on Chrome OS.
  EXPECT_EQ(base::FilePath(), g_browser_process->local_state()->GetFilePath(
                                  prefs::kSodaBinaryPath));
  EXPECT_EQ(base::FilePath(), g_browser_process->local_state()->GetFilePath(
                                  prefs::kSodaEnUsConfigPath));
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

IN_PROC_BROWSER_TEST_F(LiveCaptionControllerTest,
                       ProfilePrefsAreRegistered_Incognito) {
  // Set live caption enabled on the regular profile.
  SetLiveCaptionEnabled(true);
  EXPECT_TRUE(
      browser()->profile()->GetPrefs()->GetBoolean(prefs::kLiveCaptionEnabled));
#if !BUILDFLAG(IS_CHROMEOS)
  // These prefs are used for the component updater, but SODA does not use the
  // component updater on Chrome OS.
  EXPECT_EQ(base::FilePath(), g_browser_process->local_state()->GetFilePath(
                                  prefs::kSodaBinaryPath));
  EXPECT_EQ(base::FilePath(), g_browser_process->local_state()->GetFilePath(
                                  prefs::kSodaEnUsConfigPath));
#endif  // !BUILDFLAG(IS_CHROMEOS)

  // Ensure that live caption is also enabled in the incognito profile.
  Profile* incognito_profile =
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_TRUE(
      incognito_profile->GetPrefs()->GetBoolean(prefs::kLiveCaptionEnabled));
#if !BUILDFLAG(IS_CHROMEOS)
  // These prefs are used for the component updater, but SODA does not use the
  // component updater on Chrome OS.
  EXPECT_EQ(base::FilePath(), g_browser_process->local_state()->GetFilePath(
                                  prefs::kSodaBinaryPath));
  EXPECT_EQ(base::FilePath(), g_browser_process->local_state()->GetFilePath(
                                  prefs::kSodaEnUsConfigPath));
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

IN_PROC_BROWSER_TEST_F(LiveCaptionControllerTest, LiveCaptionEnabledChanged) {
  EXPECT_EQ(nullptr, GetBubbleController());
  EXPECT_FALSE(HasBubbleController());

  SetLiveCaptionEnabled(true);
  EXPECT_NE(nullptr, GetBubbleController());
  EXPECT_TRUE(HasBubbleController());

  SetLiveCaptionEnabled(false);
  EXPECT_EQ(nullptr, GetBubbleController());
  EXPECT_FALSE(HasBubbleController());
}

IN_PROC_BROWSER_TEST_F(LiveCaptionControllerTest,
                       LiveCaptionEnabledChanged_BubbleVisible) {
  SetLiveCaptionEnabled(true);
  // Make the bubble visible by dispatching a transcription.
  DispatchTranscription(
      "In Switzerland it is illegal to own just one guinea pig.");
  ExpectIsWidgetVisible(true);

  SetLiveCaptionEnabled(false);
  EXPECT_EQ(nullptr, GetBubbleController());
  EXPECT_FALSE(HasBubbleController());
}

IN_PROC_BROWSER_TEST_F(LiveCaptionControllerTest, OnSodaInstalled) {
  EXPECT_FALSE(HasBubbleController());
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kLiveCaptionEnabled,
                                               true);
  EXPECT_FALSE(HasBubbleController());

  // The UI is only created after SODA is installed.
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting(en_us());
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  EXPECT_TRUE(HasBubbleController());
}

// TODO(crbug.com/40936746): Re-enable this test.
IN_PROC_BROWSER_TEST_F(LiveCaptionControllerTest, DISABLED_OnSodaError) {
  // Live Caption is disabled when there is an error in the SODA download for
  // the language belonging to Live Caption.
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kLiveCaptionEnabled,
                                               true);
  EXPECT_TRUE(
      browser()->profile()->GetPrefs()->GetBoolean(prefs::kLiveCaptionEnabled));
  speech::SodaInstaller::GetInstance()->NotifySodaErrorForTesting(en_us());
  EXPECT_FALSE(
      browser()->profile()->GetPrefs()->GetBoolean(prefs::kLiveCaptionEnabled));

  // Live Caption is disabled when there is an error in the SODA binary
  // download.
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kLiveCaptionEnabled,
                                               true);
  EXPECT_TRUE(
      browser()->profile()->GetPrefs()->GetBoolean(prefs::kLiveCaptionEnabled));
  speech::SodaInstaller::GetInstance()->NotifySodaErrorForTesting();
  EXPECT_FALSE(
      browser()->profile()->GetPrefs()->GetBoolean(prefs::kLiveCaptionEnabled));

  // Live Caption is not disabled when there is an error in the SODA download
  // for a language not belonging to Live Caption.
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kLiveCaptionEnabled,
                                               true);
  EXPECT_TRUE(
      browser()->profile()->GetPrefs()->GetBoolean(prefs::kLiveCaptionEnabled));
  speech::SodaInstaller::GetInstance()->NotifySodaErrorForTesting(fr_fr());
  EXPECT_TRUE(
      browser()->profile()->GetPrefs()->GetBoolean(prefs::kLiveCaptionEnabled));
}

IN_PROC_BROWSER_TEST_F(LiveCaptionControllerTest, DispatchTranscription) {
  bool success = DispatchTranscription("A baby spider is called a spiderling.");
  EXPECT_FALSE(success);
  EXPECT_FALSE(HasBubbleController());

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
  EXPECT_FALSE(HasBubbleController());
}

IN_PROC_BROWSER_TEST_F(LiveCaptionControllerTest, OnError) {
  EXPECT_FALSE(HasBubbleController());
  OnError();
  EXPECT_TRUE(HasBubbleController());

  SetLiveCaptionEnabled(true);
  OnError();
  ExpectIsWidgetVisible(true);

  SetLiveCaptionEnabled(false);
  OnError();
  EXPECT_TRUE(HasBubbleController());
}

IN_PROC_BROWSER_TEST_F(LiveCaptionControllerTest, OnAudioStreamEnd) {
  OnAudioStreamEnd();
  EXPECT_FALSE(HasBubbleController());

  SetLiveCaptionEnabled(true);
  DispatchTranscription("Some cicadas appear only once every 17 years.");
  ExpectIsWidgetVisible(true);

  OnAudioStreamEnd();
  ExpectIsWidgetVisible(false);

  SetLiveCaptionEnabled(false);
  OnAudioStreamEnd();
  EXPECT_FALSE(HasBubbleController());
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(LiveCaptionControllerTest, OnToggleFullscreen) {
  OnToggleFullscreen();
  EXPECT_FALSE(HasBubbleController());

  SetLiveCaptionEnabled(true);
  EXPECT_TRUE(HasBubbleController());

  OnToggleFullscreen();
  EXPECT_TRUE(HasBubbleController());

  SetLiveCaptionEnabled(false);
  OnToggleFullscreen();
  EXPECT_FALSE(HasBubbleController());
}
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH)  // No multi-profile on ChromeOS.

IN_PROC_BROWSER_TEST_F(LiveCaptionControllerTest,
                       LiveCaptionEnabledChanged_MultipleProfiles) {
  Profile* profile1 = browser()->profile();
  Profile* profile2 = CreateProfile();

  // The profiles start with no caption bubble controllers.
  EXPECT_FALSE(HasBubbleControllerOnProfile(profile1));
  EXPECT_FALSE(HasBubbleControllerOnProfile(profile2));

  // Enable live caption on profile1.
  SetLiveCaptionEnabled(true);
  EXPECT_TRUE(HasBubbleControllerOnProfile(profile1));
  EXPECT_FALSE(HasBubbleControllerOnProfile(profile2));

  // Enable live caption on profile2.
  SetLiveCaptionEnabledOnProfile(true, profile2);
  EXPECT_TRUE(HasBubbleControllerOnProfile(profile1));
  EXPECT_TRUE(HasBubbleControllerOnProfile(profile2));

  // Disable live caption on profile1.
  SetLiveCaptionEnabled(false);
  EXPECT_FALSE(HasBubbleControllerOnProfile(profile1));
  EXPECT_TRUE(HasBubbleControllerOnProfile(profile2));

  // Disable live caption on profile2.
  SetLiveCaptionEnabledOnProfile(false, profile2);
  EXPECT_FALSE(HasBubbleControllerOnProfile(profile1));
  EXPECT_FALSE(HasBubbleControllerOnProfile(profile2));
}

IN_PROC_BROWSER_TEST_F(LiveCaptionControllerTest,
                       DispatchTranscription_MultipleProfiles) {
  Profile* profile1 = browser()->profile();
  Profile* profile2 = CreateProfile();

  // Enable live caption on both profiles.
  SetLiveCaptionEnabled(true);
  SetLiveCaptionEnabledOnProfile(true, profile2);

  // Dispatch transcription routes the transcription to the right profile.
  bool success =
      DispatchTranscriptionToProfile("Only female mosquitos bite.", profile1);
  EXPECT_TRUE(success);
  ExpectIsWidgetVisibleOnProfile(true, profile1);
  ExpectIsWidgetVisibleOnProfile(false, profile2);
  ExpectBubbleLabelTextOnProfileEquals("Only female mosquitos bite.", profile1);
  ExpectBubbleLabelTextOnProfileEquals("", profile2);

  success = DispatchTranscriptionToProfile(
      "Mosquitos were around at the time of the dinosaurs.", profile2);
  EXPECT_TRUE(success);
  ExpectIsWidgetVisibleOnProfile(true, profile1);
  ExpectIsWidgetVisibleOnProfile(true, profile2);
  ExpectBubbleLabelTextOnProfileEquals("Only female mosquitos bite.", profile1);
  ExpectBubbleLabelTextOnProfileEquals(
      "Mosquitos were around at the time of the dinosaurs.", profile2);
}

IN_PROC_BROWSER_TEST_F(LiveCaptionControllerTest, OnError_MultipleProfiles) {
  Profile* profile1 = browser()->profile();
  Profile* profile2 = CreateProfile();

  // Enable live caption on both profiles.
  SetLiveCaptionEnabled(true);
  SetLiveCaptionEnabledOnProfile(true, profile2);

  // OnError routes to the right profile.
  OnErrorOnProfile(profile1);
  ExpectIsWidgetVisibleOnProfile(true, profile1);
  ExpectIsWidgetVisibleOnProfile(false, profile2);

  OnErrorOnProfile(profile2);
  ExpectIsWidgetVisibleOnProfile(true, profile1);
  ExpectIsWidgetVisibleOnProfile(true, profile2);
}

IN_PROC_BROWSER_TEST_F(LiveCaptionControllerTest,
                       OnAudioStreamEnd_MultipleProfiles) {
  Profile* profile1 = browser()->profile();
  Profile* profile2 = CreateProfile();

  // Enable live caption on both profiles.
  SetLiveCaptionEnabled(true);
  SetLiveCaptionEnabledOnProfile(true, profile2);

  DispatchTranscriptionToProfile(
      "Capybaras are the largest rodents in the world.", profile1);
  DispatchTranscriptionToProfile("Capybaras' teeth grow continuously.",
                                 profile2);
  ExpectIsWidgetVisibleOnProfile(true, profile1);
  ExpectIsWidgetVisibleOnProfile(true, profile2);

  // OnAudioStreamEnd routes to the right profile.
  OnAudioStreamEndOnProfile(profile1);
  ExpectIsWidgetVisibleOnProfile(false, profile1);
  ExpectIsWidgetVisibleOnProfile(true, profile2);

  OnAudioStreamEndOnProfile(profile2);
  ExpectIsWidgetVisibleOnProfile(false, profile1);
  ExpectIsWidgetVisibleOnProfile(false, profile2);
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace captions
