// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/live_caption/live_caption_speech_recognition_host.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/accessibility/live_caption/live_caption_controller_factory.h"
#include "chrome/browser/accessibility/live_caption/live_translate_controller_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/live_caption/caption_bubble_controller.h"
#include "components/live_caption/caption_controller_base.h"
#include "components/live_caption/features.h"
#include "components/live_caption/live_caption_controller.h"
#include "components/live_caption/live_translate_controller.h"
#include "components/live_caption/pref_names.h"
#include "components/live_caption/views/translation_view_wrapper_base.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/native_theme/caption_style.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#endif

namespace captions {
namespace {

constexpr char kTestUrl[] = "https://example.com";

class MockCaptionBubbleController : public CaptionBubbleController {
 public:
  MockCaptionBubbleController() = default;
  ~MockCaptionBubbleController() override = default;

  bool OnTranscription(content::RenderFrameHost*,
                       CaptionBubbleContext*,
                       const media::SpeechRecognitionResult&) override {
    return true;
  }
  void OnAudioStreamEnd(content::RenderFrameHost*,
                        CaptionBubbleContext*) override {}
  void OnLanguageIdentificationEvent(
      content::RenderFrameHost*,
      CaptionBubbleContext*,
      const media::mojom::LanguageIdentificationEventPtr&) override {}
  void OnError(CaptionBubbleContext*,
               CaptionBubbleErrorType,
               OnErrorClickedCallback,
               OnDoNotShowAgainClickedCallback) override {}
  void UpdateCaptionStyle(std::optional<ui::CaptionStyle>) override {}
  bool IsWidgetVisibleForTesting() override { return false; }
  bool IsGenericErrorMessageVisibleForTesting() override { return false; }
  std::string GetBubbleLabelTextForTesting() override { return ""; }
  void CloseActiveModelForTesting() override {}
};

class MockCaptionControllerDelegate : public CaptionControllerBase::Delegate {
 public:
  MockCaptionControllerDelegate() = default;
  ~MockCaptionControllerDelegate() override = default;

  std::unique_ptr<CaptionBubbleController> CreateCaptionBubbleController(
      CaptionBubbleSettings* caption_bubble_settings,
      const std::string& application_locale,
      std::unique_ptr<TranslationViewWrapperBase> translation_view_wrapper)
      override {
    return std::make_unique<MockCaptionBubbleController>();
  }

  void AddCaptionStyleObserver(ui::NativeThemeObserver* observer) override {}
  void RemoveCaptionStyleObserver(ui::NativeThemeObserver* observer) override {}
};

class MockLiveTranslateController : public LiveTranslateController {
 public:
  explicit MockLiveTranslateController(PrefService* profile_prefs)
      : LiveTranslateController(profile_prefs,
                                /*translation_dispatcher=*/nullptr,
                                /*google_api_dispatcher=*/nullptr) {}
  ~MockLiveTranslateController() override = default;

  void GetTranslation(const std::string& result,
                      std::string source_language,
                      std::string target_language,
                      TranslateEventCallback callback) override {
    translation_requests_.push_back(result);
    std::move(callback).Run(TranslateEvent(result));
  }

  const std::vector<std::string>& translation_requests() const {
    return translation_requests_;
  }

  void ClearTranslationRequests() { translation_requests_.clear(); }

 private:
  std::vector<std::string> translation_requests_;
};

}  // namespace

class LiveCaptionSpeechRecognitionHostTest
    : public ChromeRenderViewHostTestHarness {
 public:
  LiveCaptionSpeechRecognitionHostTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~LiveCaptionSpeechRecognitionHostTest() override = default;

 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    content::RenderFrameHostTester::For(main_rfh())
        ->InitializeRenderFrameIfNeeded();
    content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                               GURL(kTestUrl));

    InitFeatures();

    LiveCaptionControllerFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          auto delegate = std::make_unique<MockCaptionControllerDelegate>();
          return std::make_unique<LiveCaptionController>(
              Profile::FromBrowserContext(context)->GetPrefs(),
              /*global_prefs=*/nullptr, "en-US", context, std::move(delegate));
        }));

    LiveTranslateControllerFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&LiveCaptionSpeechRecognitionHostTest::
                                           BuildMockLiveTranslateController,
                                       base::Unretained(this)));

    PrefService* prefs = profile()->GetPrefs();
    prefs->SetBoolean(prefs::kLiveCaptionEnabled, true);
    prefs->SetBoolean(prefs::kLiveTranslateEnabled, true);
    prefs->SetString(prefs::kLiveTranslateTargetLanguageCode, "es");

    mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizerClient>
        receiver;
    remote_.Bind(receiver.InitWithNewPipeAndPassRemote());
    LiveCaptionSpeechRecognitionHost::Create(main_rfh(), std::move(receiver));
  }

  void TearDown() override {
    remote_.reset();
    mock_translate_controller_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  virtual void InitFeatures() {
    scoped_feature_list_.InitWithFeatures(
        {live_caption::kLiveTranslateTimeGating,
#if BUILDFLAG(IS_CHROMEOS)
         media::kFeatureManagementLiveTranslateCrOS,
         ash::features::kOnDeviceSpeechRecognition
#endif
        },
        {});
  }

  void SendSpeechResult(const std::string& text, bool is_final = false) {
    remote_->OnSpeechRecognitionRecognitionEvent(
        media::SpeechRecognitionResult(text, is_final), base::DoNothing());
    remote_.FlushForTesting();
  }

  void SendSpeechStopped() {
    remote_->OnSpeechRecognitionStopped();
    remote_.FlushForTesting();
  }

  void SendSpeechError() {
    remote_->OnSpeechRecognitionError();
    remote_.FlushForTesting();
  }

  const std::vector<std::string>& translation_requests() const {
    CHECK(mock_translate_controller_);
    return mock_translate_controller_->translation_requests();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  std::unique_ptr<KeyedService> BuildMockLiveTranslateController(
      content::BrowserContext* context) {
    auto controller = std::make_unique<MockLiveTranslateController>(
        Profile::FromBrowserContext(context)->GetPrefs());
    mock_translate_controller_ = controller.get();
    return controller;
  }

  mojo::Remote<media::mojom::SpeechRecognitionRecognizerClient> remote_;
  raw_ptr<MockLiveTranslateController> mock_translate_controller_ = nullptr;
};

class LiveCaptionSpeechRecognitionHostFeatureDisabledTest
    : public LiveCaptionSpeechRecognitionHostTest {
 protected:
  void InitFeatures() override {
    scoped_feature_list_.InitWithFeatures(
        {
#if BUILDFLAG(IS_CHROMEOS)
            media::kFeatureManagementLiveTranslateCrOS,
            ash::features::kOnDeviceSpeechRecognition
#endif
        },
        {live_caption::kLiveTranslateTimeGating});
  }
};

TEST_F(LiveCaptionSpeechRecognitionHostTest, DebouncesUnpunctuatedPartial) {
  SendSpeechResult("Hello", /*is_final=*/false);

  // Before the 500ms debounce expires, nothing is dispatched.
  task_environment()->FastForwardBy(base::Milliseconds(200));
  EXPECT_TRUE(translation_requests().empty());

  task_environment()->FastForwardBy(base::Milliseconds(299));
  EXPECT_TRUE(translation_requests().empty());

  // Once 500ms has elapsed, the buffered partial is dispatched.
  task_environment()->FastForwardBy(base::Milliseconds(1));
  ASSERT_EQ(1u, translation_requests().size());
  EXPECT_EQ("Hello", translation_requests().back());
}

TEST_F(LiveCaptionSpeechRecognitionHostTest,
       OverwritesGatedPartialBeforeDebounceExpires) {
  SendSpeechResult("Hello", /*is_final=*/false);
  task_environment()->FastForwardBy(base::Milliseconds(200));
  EXPECT_TRUE(translation_requests().empty());

  // An updated partial arrives before the debounce timer fires.
  SendSpeechResult("Hello world", /*is_final=*/false);

  // At t = 500ms (when the original debounce timer would have expired),
  // nothing fires because the timer was reset on the second partial.
  task_environment()->FastForwardBy(base::Milliseconds(300));
  EXPECT_TRUE(translation_requests().empty());

  // At t = 700ms the latest partial flushes via debounce.
  task_environment()->FastForwardBy(base::Milliseconds(200));
  ASSERT_EQ(1u, translation_requests().size());
  EXPECT_EQ("Hello world", translation_requests().back());
}

TEST_F(LiveCaptionSpeechRecognitionHostTest,
       DispatchesImmediatelyOnPunctuation) {
  SendSpeechResult("Hello", /*is_final=*/false);
  task_environment()->FastForwardBy(base::Milliseconds(300));
  EXPECT_TRUE(translation_requests().empty());

  // Punctuation triggers immediate dispatch without waiting for the timer.
  SendSpeechResult("Hello world.", /*is_final=*/false);
  ASSERT_EQ(1u, translation_requests().size());
  EXPECT_EQ("Hello world.", translation_requests().back());

  // Further time passage does not cause redundant dispatches.
  task_environment()->FastForwardBy(base::Milliseconds(2000));
  EXPECT_EQ(1u, translation_requests().size());
}

TEST_F(LiveCaptionSpeechRecognitionHostTest,
       DispatchesImmediatelyOnFinalResult) {
  SendSpeechResult("Hello", /*is_final=*/false);
  task_environment()->FastForwardBy(base::Milliseconds(300));
  EXPECT_TRUE(translation_requests().empty());

  // Final results dispatch immediately even without punctuation.
  SendSpeechResult("Hello world", /*is_final=*/true);
  ASSERT_EQ(1u, translation_requests().size());
  EXPECT_EQ("Hello world", translation_requests().back());

  task_environment()->FastForwardBy(base::Milliseconds(2000));
  EXPECT_EQ(1u, translation_requests().size());
}

TEST_F(LiveCaptionSpeechRecognitionHostTest, MaxDurationForcesFlush) {
  // t = 0ms: First partial starts the batch.
  SendSpeechResult("Streaming", /*is_final=*/false);
  EXPECT_TRUE(translation_requests().empty());

  // t = 300ms: Second partial. Elapsed = 300ms < 800ms.
  task_environment()->FastForwardBy(base::Milliseconds(300));
  SendSpeechResult("Streaming words", /*is_final=*/false);
  EXPECT_TRUE(translation_requests().empty());

  // t = 600ms: Third partial. Elapsed = 600ms < 800ms.
  task_environment()->FastForwardBy(base::Milliseconds(300));
  SendSpeechResult("Streaming words without end", /*is_final=*/false);
  EXPECT_TRUE(translation_requests().empty());

  // At t = 800ms, max duration elapses and forces a flush.
  task_environment()->FastForwardBy(base::Milliseconds(200));
  ASSERT_EQ(1u, translation_requests().size());
  EXPECT_EQ("Streaming words without end", translation_requests().back());
}

TEST_F(LiveCaptionSpeechRecognitionHostTest,
       SpeechRecognitionStoppedCancelsTimerAndClearsBuffer) {
  SendSpeechResult("Incomplete phrase", /*is_final=*/false);
  task_environment()->FastForwardBy(base::Milliseconds(300));
  EXPECT_TRUE(translation_requests().empty());

  SendSpeechStopped();

  // Advancing time past the debounce duration should not dispatch anything.
  task_environment()->FastForwardBy(base::Milliseconds(5000));
  EXPECT_TRUE(translation_requests().empty());
}

TEST_F(LiveCaptionSpeechRecognitionHostTest,
       SpeechRecognitionErrorCancelsTimerAndClearsBuffer) {
  SendSpeechResult("Incomplete phrase", /*is_final=*/false);
  task_environment()->FastForwardBy(base::Milliseconds(300));
  EXPECT_TRUE(translation_requests().empty());

  SendSpeechError();

  // Advancing time past the debounce duration should not dispatch anything.
  task_environment()->FastForwardBy(base::Milliseconds(5000));
  EXPECT_TRUE(translation_requests().empty());
}

TEST_F(LiveCaptionSpeechRecognitionHostFeatureDisabledTest,
       FeatureDisabledDispatchesImmediately) {
  // When kLiveTranslateTimeGating is disabled, partial results without
  // punctuation dispatch immediately without delay.
  SendSpeechResult("Hello", /*is_final=*/false);
  ASSERT_EQ(1u, translation_requests().size());
  EXPECT_EQ("Hello", translation_requests().back());
}

TEST_F(LiveCaptionSpeechRecognitionHostTest,
       IdeographicPunctuationDispatchesImmediately) {
  SendSpeechResult("你好", /*is_final=*/false);
  task_environment()->FastForwardBy(base::Milliseconds(300));
  EXPECT_TRUE(translation_requests().empty());

  // Full-width ideographic punctuation should dispatch immediately.
  SendSpeechResult("你好世界。", /*is_final=*/false);
  ASSERT_EQ(1u, translation_requests().size());
  EXPECT_EQ("你好世界。", translation_requests().back());
}

TEST_F(LiveCaptionSpeechRecognitionHostTest,
       MultiSentencePartialDebouncedWhenTrailingSentenceUnpunctuated) {
  SendSpeechResult("Hello world.", /*is_final=*/false);
  ASSERT_EQ(1u, translation_requests().size());
  EXPECT_EQ("Hello world.", translation_requests().back());

  // Next partial begins a second sentence without punctuation.
  SendSpeechResult("Hello world. How", /*is_final=*/false);

  // Before debounce expires, no new translation request is sent.
  task_environment()->FastForwardBy(base::Milliseconds(300));
  EXPECT_EQ(1u, translation_requests().size());

  // Once debounce expires, the updated partial is dispatched.
  task_environment()->FastForwardBy(base::Milliseconds(200));
  ASSERT_EQ(2u, translation_requests().size());
  EXPECT_EQ("Hello world. How", translation_requests().back());
}

TEST_F(LiveCaptionSpeechRecognitionHostTest,
       LiveTranslateDisabledCancelsGatedTranslation) {
  SendSpeechResult("Hello", /*is_final=*/false);
  task_environment()->FastForwardBy(base::Milliseconds(300));
  EXPECT_TRUE(translation_requests().empty());

  // Disable Live Translate while timer is running.
  profile()->GetPrefs()->SetBoolean(prefs::kLiveTranslateEnabled, false);

  // Advancing time past debounce duration should not dispatch any translation
  // requests.
  task_environment()->FastForwardBy(base::Milliseconds(5000));
  EXPECT_TRUE(translation_requests().empty());
}

TEST_F(LiveCaptionSpeechRecognitionHostTest,
       TargetLanguageMatchesSourceLanguageCancelsGatedTranslation) {
  SendSpeechResult("Hello", /*is_final=*/false);
  task_environment()->FastForwardBy(base::Milliseconds(300));
  EXPECT_TRUE(translation_requests().empty());

  // Change target language to match source language ("en") while timer is
  // running.
  profile()->GetPrefs()->SetString(prefs::kLiveTranslateTargetLanguageCode,
                                   "en");

  // Advancing time past debounce duration should not dispatch any translation
  // requests.
  task_environment()->FastForwardBy(base::Milliseconds(5000));
  EXPECT_TRUE(translation_requests().empty());
}

}  // namespace captions
