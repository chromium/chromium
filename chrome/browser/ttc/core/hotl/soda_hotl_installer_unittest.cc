// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/hotl/soda_hotl_installer.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/speech_recognition_service.h"
#include "chrome/browser/ttc/app/public/audio_controller.h"
#include "chrome/browser/ttc/core/features.h"
#include "chrome/browser/ttc/core/hotl/soda_hotl_speech_recognizer.h"
#include "chrome/browser/ttc/core/session_controller.h"
#include "chrome/browser/ttc/core/test_utils.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"
#include "chrome/browser/ttc/core/ttc_keyed_service_factory.h"
#include "chrome/services/speech/buildflags/buildflags.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/soda/constants.h"
#include "components/soda/mock_soda_installer.h"
#include "components/soda/pref_names.h"
#include "components/soda/soda_util.h"
#include "components/ttc/app/public/conversation.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_BROWSER_SPEECH_SERVICE)
#include "chrome/browser/speech/speech_recognition_service_factory.h"
#elif BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#include "chrome/browser/speech/cros_speech_recognition_service_factory.h"
#endif

namespace ttc::hotl {
namespace {

class StubSpeechRecognitionService : public speech::SpeechRecognitionService {
 public:
  void BindSpeechRecognitionContext(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionContext> receiver)
      override {}
  void BindAudioSourceSpeechRecognitionContext(
      mojo::PendingReceiver<media::mojom::AudioSourceSpeechRecognitionContext>
          receiver) override {}
};

void EnableHotlSodaFeatures(base::test::ScopedFeatureList& features) {
  features.InitWithFeatures(
      {
          kTtc,
          kTtcHotlClickConfirmation,
#if BUILDFLAG(IS_CHROMEOS)
          ash::features::kOnDeviceSpeechRecognition,
#endif
      },
      {});
}

class SodaHotlInstallerTest : public testing::Test {
 public:
  SodaHotlInstallerTest() {
    EnableHotlSodaFeatures(hotl_features_);
    service_ = static_cast<TtcKeyedService*>(
        TtcKeyedServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            &profile_,
            base::BindRepeating(&SodaHotlInstallerTest::CreateService,
                                base::Unretained(this))));
  }
  ~SodaHotlInstallerTest() override = default;

 private:
  std::unique_ptr<KeyedService> CreateService(
      content::BrowserContext* context) {
    return std::make_unique<TtcKeyedService>(
        Profile::FromBrowserContext(context),
        base::BindRepeating(&SodaHotlInstallerTest::CreateMockConversation,
                            base::Unretained(this)));
  }

  std::unique_ptr<Conversation> CreateMockConversation(
      SessionController&,
      std::unique_ptr<AudioController>) {
    return std::make_unique<testing::NiceMock<MockConversation>>();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList hotl_features_;
  TestingProfile profile_;
  raw_ptr<TtcKeyedService> service_ = nullptr;
  testing::NiceMock<speech::MockSodaInstaller> mock_soda_installer_;
};

TEST_F(SodaHotlInstallerTest, TriggersSodaInstallOnSessionStart) {
  if (!speech::IsOnDeviceSpeechRecognitionSupported()) {
    GTEST_SKIP();
  }
  ASSERT_FALSE(profile_.GetPrefs()->GetBoolean(prefs::kLiveCaptionEnabled));

  EXPECT_CALL(mock_soda_installer_,
              InstallSoda(g_browser_process->local_state()))
      .Times(1);
  EXPECT_CALL(mock_soda_installer_,
              InstallLanguage(std::string_view("en-US"),
                              g_browser_process->local_state()))
      .Times(1);

  service_->StartSession();
  service_->EndSession();
}

TEST_F(SodaHotlInstallerTest, ResolvesUiLocaleAndExplicitLiveCaptionLanguage) {
  if (!speech::IsOnDeviceSpeechRecognitionSupported()) {
    GTEST_SKIP();
  }

  const std::string original_locale =
      TestingBrowserProcess::GetGlobal()->GetApplicationLocale();
  base::ScopedClosureRunner restore_locale(base::BindOnce(
      [](std::string locale) {
        TestingBrowserProcess::GetGlobal()->SetApplicationLocale(locale);
      },
      original_locale));
  TestingBrowserProcess::GetGlobal()->SetApplicationLocale("ja");
  EXPECT_EQ(GetHotlRecognitionLanguage(profile_), "ja-JP");

  EXPECT_CALL(mock_soda_installer_,
              InstallSoda(g_browser_process->local_state()))
      .Times(1);
  EXPECT_CALL(mock_soda_installer_,
              InstallLanguage(std::string_view("ja-JP"),
                              g_browser_process->local_state()))
      .Times(1);

  service_->StartSession();
  service_->EndSession();
  testing::Mock::VerifyAndClearExpectations(&mock_soda_installer_);

  // An explicit non-default Live Caption language overrides the UI locale, and
  // PrecacheSodaForHotl() maps its language subtag to the matching SODA pack.
  profile_.GetPrefs()->SetString(prefs::kLiveCaptionLanguageCode, "fr-CA");
  EXPECT_EQ(GetHotlRecognitionLanguage(profile_), "fr-CA");

  EXPECT_CALL(mock_soda_installer_,
              InstallSoda(g_browser_process->local_state()))
      .Times(1);
  EXPECT_CALL(mock_soda_installer_,
              InstallLanguage(std::string_view("fr-FR"),
                              g_browser_process->local_state()))
      .Times(1);

  service_->StartSession();
  service_->EndSession();
}

TEST_F(SodaHotlInstallerTest, RefreshesUninstallTimerWhenAlreadyInstalled) {
  if (!speech::IsOnDeviceSpeechRecognitionSupported()) {
    GTEST_SKIP();
  }

  mock_soda_installer_.NotifySodaInstalledForTesting();
  mock_soda_installer_.NotifySodaInstalledForTesting(
      speech::LanguageCode::kEnUs);

  const base::Time old_time = base::Time::Now() - base::Days(1);
  g_browser_process->local_state()->SetTime(
      prefs::kSodaEnUsScheduledDeletionTime, old_time);

  EXPECT_CALL(mock_soda_installer_, InstallSoda(testing::_)).Times(0);
  EXPECT_CALL(mock_soda_installer_, InstallLanguage(testing::_, testing::_))
      .Times(0);

  service_->StartSession();
  service_->EndSession();
  EXPECT_GT(g_browser_process->local_state()->GetTime(
                prefs::kSodaEnUsScheduledDeletionTime),
            old_time);
}

TEST_F(SodaHotlInstallerTest, PrecachesUpdatedLanguageOnRecognizerStart) {
  if (!speech::IsOnDeviceSpeechRecognitionSupported()) {
    GTEST_SKIP();
  }

#if BUILDFLAG(ENABLE_BROWSER_SPEECH_SERVICE)
  SpeechRecognitionServiceFactory::GetInstanceForTest()
      ->SetTestingFactoryAndUse(
          &profile_,
          base::BindRepeating(
              [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
                return std::make_unique<StubSpeechRecognitionService>();
              }));
#elif BUILDFLAG(IS_CHROMEOS)
  CrosSpeechRecognitionServiceFactory::GetInstanceForTest()
      ->SetTestingFactoryAndUse(
          &profile_,
          base::BindRepeating(
              [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
                return std::make_unique<StubSpeechRecognitionService>();
              }));
#else
  GTEST_SKIP();
#endif

  mock_soda_installer_.NotifySodaInstalledForTesting();
  mock_soda_installer_.NotifySodaInstalledForTesting(
      speech::LanguageCode::kEnUs);

  service_->StartSession();
  testing::Mock::VerifyAndClearExpectations(&mock_soda_installer_);

  // Simulate a mid-session Live Caption language change while Live Caption is
  // disabled, then start a HOTL recognition flow.
  profile_.GetPrefs()->SetString(prefs::kLiveCaptionLanguageCode, "es-ES");
  const std::string locale = GetHotlRecognitionLanguage(profile_);
  EXPECT_EQ(locale, "es-ES");

  EXPECT_CALL(mock_soda_installer_,
              InstallSoda(g_browser_process->local_state()))
      .Times(1);
  EXPECT_CALL(mock_soda_installer_,
              InstallLanguage(std::string_view("es-ES"),
                              g_browser_process->local_state()))
      .Times(1);

  SodaHotlSpeechRecognizer recognizer(profile_);
  recognizer.Start(locale, base::DoNothing(), base::DoNothing());
  recognizer.Stop();
  service_->EndSession();
}

TEST_F(SodaHotlInstallerTest, SkipsSodaInstallWhenHotlFlagDisabled) {
  base::test::ScopedFeatureList disabled_hotl;
  disabled_hotl.InitWithFeatures({kTtc}, {kTtcHotlClickConfirmation});

  EXPECT_CALL(mock_soda_installer_, InstallSoda(testing::_)).Times(0);
  EXPECT_CALL(mock_soda_installer_, InstallLanguage(testing::_, testing::_))
      .Times(0);

  service_->StartSession();
  service_->EndSession();
}

}  // namespace
}  // namespace ttc::hotl
