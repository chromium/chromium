// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/accessibility/live_caption/live_caption_speech_recognition_host_browsertest.h"
#include "chrome/browser/browser_process.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/soda/constants.h"
#include "components/soda/pref_names.h"

namespace captions {

class LiveCaptionLanguageIdentificationTest
    : public LiveCaptionSpeechRecognitionHostTest {
 public:
  LiveCaptionLanguageIdentificationTest() = default;
  ~LiveCaptionLanguageIdentificationTest() override = default;
  LiveCaptionLanguageIdentificationTest(
      const LiveCaptionLanguageIdentificationTest&) = delete;
  LiveCaptionLanguageIdentificationTest& operator=(
      const LiveCaptionLanguageIdentificationTest&) = delete;
};

// Verify that 10 consecutive highly confident language identification events
// for an auto-switched language extends the uninstallation timer for the
// language pack.
IN_PROC_BROWSER_TEST_F(LiveCaptionLanguageIdentificationTest,
                       ExtendsLanguagePackUninstallation) {
  content::RenderFrameHost* frame_host = browser()
                                             ->tab_strip_model()
                                             ->GetActiveWebContents()
                                             ->GetPrimaryMainFrame();
  CreateLiveCaptionSpeechRecognitionHost(frame_host);

  PrefService* local_state = g_browser_process->local_state();
  // The time should be null initially.
  ASSERT_TRUE(
      local_state->GetTime(prefs::kSodaDeDeScheduledDeletionTime).is_null());

  base::RunLoop run_loop;
  PrefChangeRegistrar pref_change_registrar;
  pref_change_registrar.Init(local_state);
  // Quit the run loop when the deletion time is no longer null.
  pref_change_registrar.Add(
      prefs::kSodaDeDeScheduledDeletionTime,
      base::BindLambdaForTesting([&](const std::string& pref_name) {
        if (!local_state->GetTime(prefs::kSodaDeDeScheduledDeletionTime)
                 .is_null()) {
          run_loop.Quit();
        }
      }));

  SetLiveCaptionEnabled(true);
  // Switch to German. This sets language_auto_switched_ to true.
  OnLanguageIdentificationEvent(
      frame_host, "de-DE", media::mojom::ConfidenceLevel::kHighlyConfident,
      media::mojom::AsrSwitchResult::kSwitchSucceeded);

  for (int i = 0; i < 9; ++i) {
    OnLanguageIdentificationEvent(
        frame_host, "de-DE", media::mojom::ConfidenceLevel::kHighlyConfident,
        media::mojom::AsrSwitchResult::kDefaultNoSwitch);
  }

  run_loop.Run();
  base::Time new_deletion_time =
      local_state->GetTime(prefs::kSodaDeDeScheduledDeletionTime);
  EXPECT_FALSE(new_deletion_time.is_null());
  // The timer is set to 30 days from now. Check that it's within a reasonable
  // range.
  EXPECT_GT(new_deletion_time, base::Time::Now() + base::Days(29));
  EXPECT_LT(new_deletion_time, base::Time::Now() + base::Days(31));
}

// Verify that 10 consecutive highly coonfident language identification events
// do not extend the uninstallation if they're not accompanied by a successful
// language switch event.
IN_PROC_BROWSER_TEST_F(LiveCaptionLanguageIdentificationTest, NoSwitch) {
  content::RenderFrameHost* frame_host = browser()
                                             ->tab_strip_model()
                                             ->GetActiveWebContents()
                                             ->GetPrimaryMainFrame();
  CreateLiveCaptionSpeechRecognitionHost(frame_host);

  PrefService* local_state = g_browser_process->local_state();
  // The time should be null initially.
  ASSERT_TRUE(
      local_state->GetTime(prefs::kSodaDeDeScheduledDeletionTime).is_null());

  SetLiveCaptionEnabled(true);

  for (int i = 0; i < 10; ++i) {
    OnLanguageIdentificationEvent(
        frame_host, "de-DE", media::mojom::ConfidenceLevel::kHighlyConfident,
        media::mojom::AsrSwitchResult::kDefaultNoSwitch);
  }

  base::RunLoop().RunUntilIdle();
  base::Time new_deletion_time =
      local_state->GetTime(prefs::kSodaDeDeScheduledDeletionTime);
  EXPECT_TRUE(new_deletion_time.is_null());
}

// Verify that 10 non-consecutive highly confident language identification
// events do not extend the uninstallation.
IN_PROC_BROWSER_TEST_F(LiveCaptionLanguageIdentificationTest, NonConsecutive) {
  content::RenderFrameHost* frame_host = browser()
                                             ->tab_strip_model()
                                             ->GetActiveWebContents()
                                             ->GetPrimaryMainFrame();
  CreateLiveCaptionSpeechRecognitionHost(frame_host);

  PrefService* local_state = g_browser_process->local_state();
  // The time should be null initially.
  ASSERT_TRUE(
      local_state->GetTime(prefs::kSodaDeDeScheduledDeletionTime).is_null());

  SetLiveCaptionEnabled(true);

  // Switch to German. This sets language_auto_switched_ to true.
  OnLanguageIdentificationEvent(
      frame_host, "de-DE", media::mojom::ConfidenceLevel::kHighlyConfident,
      media::mojom::AsrSwitchResult::kSwitchSucceeded);
  for (int i = 0; i < 5; ++i) {
    OnLanguageIdentificationEvent(
        frame_host, "de-DE", media::mojom::ConfidenceLevel::kHighlyConfident,
        media::mojom::AsrSwitchResult::kDefaultNoSwitch);
  }

  // Interrupt consecutiveness with a not confident language identification
  // event.
  OnLanguageIdentificationEvent(
      frame_host, "de-DE", media::mojom::ConfidenceLevel::kNotConfident,
      media::mojom::AsrSwitchResult::kDefaultNoSwitch);
  for (int i = 0; i < 5; ++i) {
    OnLanguageIdentificationEvent(
        frame_host, "de-DE", media::mojom::ConfidenceLevel::kHighlyConfident,
        media::mojom::AsrSwitchResult::kDefaultNoSwitch);
  }

  // Interrupt consecutiveness with a highly confident language identification
  // event of another language.
  OnLanguageIdentificationEvent(
      frame_host, "ja-JP", media::mojom::ConfidenceLevel::kHighlyConfident,
      media::mojom::AsrSwitchResult::kDefaultNoSwitch);
  for (int i = 0; i < 5; ++i) {
    OnLanguageIdentificationEvent(
        frame_host, "de-DE", media::mojom::ConfidenceLevel::kHighlyConfident,
        media::mojom::AsrSwitchResult::kDefaultNoSwitch);
  }

  base::RunLoop().RunUntilIdle();
  base::Time new_deletion_time =
      local_state->GetTime(prefs::kSodaDeDeScheduledDeletionTime);
  EXPECT_TRUE(new_deletion_time.is_null());
}
}  // namespace captions
