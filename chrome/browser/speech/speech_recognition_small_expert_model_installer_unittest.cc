// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_recognition_small_expert_model_installer.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/accessibility/live_caption/live_caption_controller_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "media/base/media_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace speech {

namespace {

using State = SpeechRecognitionSmallExpertModelInstaller::
    SpeechRecognitionSmallExpertModelState;

class MockObserver
    : public SpeechRecognitionSmallExpertModelInstaller::Observer {
 public:
  MockObserver() = default;
  ~MockObserver() override = default;

  MOCK_METHOD(void,
              OnSpeechRecognitionSmallExpertModelInstalled,
              (),
              (override));
  MOCK_METHOD(void,
              OnSpeechRecognitionSmallExpertModelInstallError,
              (),
              (override));
  MOCK_METHOD(void,
              OnSpeechRecognitionSmallExpertModelProgress,
              (int, double, base::TimeDelta),
              (override));
  MOCK_METHOD(void,
              OnSpeechRecognitionSmallExpertModelStateChanged,
              (State),
              (override));
};

class TestSpeechRecognitionSmallExpertModelInstaller
    : public SpeechRecognitionSmallExpertModelInstaller {
 public:
  using SpeechRecognitionSmallExpertModelInstaller::
      DeleteSpeechRecognitionSmallExpertModelFilesAsync;
  using SpeechRecognitionSmallExpertModelInstaller::
      HandleSpeechRecognitionSmallExpertModelError;
  using SpeechRecognitionSmallExpertModelInstaller::
      OnSpeechRecognitionSmallExpertModelClientReady;
  using SpeechRecognitionSmallExpertModelInstaller::
      OnSpeechRecognitionSmallExpertModelInstalled;
};

class SpeechRecognitionSmallExpertModelInstallerTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    if (!pref_service()->FindPreference(
            prefs::kSpeechRecognitionSmallExpertModelPath)) {
      SpeechRecognitionSmallExpertModelInstaller::RegisterLocalStatePrefs(
          pref_service()->registry());
    }
    installer_ =
        std::make_unique<TestSpeechRecognitionSmallExpertModelInstaller>();
  }

  void TearDown() override {
    if (pref_service()->FindPreference(
            prefs::kSpeechRecognitionSmallExpertModelPath)) {
      pref_service()->ClearPref(prefs::kSpeechRecognitionSmallExpertModelPath);
    }
    installer_.reset();
  }

  TestSpeechRecognitionSmallExpertModelInstaller* installer() {
    return installer_.get();
  }
  TestingPrefServiceSimple* pref_service() {
    return TestingBrowserProcess::GetGlobal()->GetTestingLocalState();
  }
  TestingProfileManager* profile_manager() { return &profile_manager_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  std::unique_ptr<TestSpeechRecognitionSmallExpertModelInstaller> installer_;
};

TEST_F(SpeechRecognitionSmallExpertModelInstallerTest, InitialState) {
  EXPECT_EQ(installer()->GetSpeechRecognitionSmallExpertModelState(),
            State::kNotInstalled);
  EXPECT_FALSE(installer()->IsSpeechRecognitionSmallExpertModelInstalled());
  EXPECT_FALSE(installer()->IsSpeechRecognitionSmallExpertModelDownloading());
  EXPECT_EQ(installer()->GetSpeechRecognitionSmallExpertModelRetryCount(), 0);
  EXPECT_TRUE(installer()->GetSpeechRecognitionSmallExpertModelPath().empty());
}

TEST_F(SpeechRecognitionSmallExpertModelInstallerTest,
       StateTransitionsAndObserverNotifications) {
  testing::NiceMock<MockObserver> observer;
  base::ScopedObservation<SpeechRecognitionSmallExpertModelInstaller,
                          SpeechRecognitionSmallExpertModelInstaller::Observer>
      observation(&observer);
  observation.Observe(installer());

  // Transition to kDownloading.
  EXPECT_CALL(observer, OnSpeechRecognitionSmallExpertModelStateChanged(
                            State::kDownloading))
      .Times(1);
  installer()->NotifySpeechRecognitionSmallExpertModelStateChangedForTesting(
      State::kDownloading);
  EXPECT_EQ(installer()->GetSpeechRecognitionSmallExpertModelState(),
            State::kDownloading);
  EXPECT_TRUE(installer()->IsSpeechRecognitionSmallExpertModelDownloading());
  EXPECT_FALSE(installer()->IsSpeechRecognitionSmallExpertModelInstalled());

  // Download progress while in kDownloading state.
  EXPECT_CALL(observer,
              OnSpeechRecognitionSmallExpertModelProgress(50, 0.0, testing::_))
      .Times(1);
  installer()->OnDownloadProgressUpdate(50, 100);

  EXPECT_CALL(observer,
              OnSpeechRecognitionSmallExpertModelProgress(100, 0.0, testing::_))
      .Times(1);
  installer()->OnDownloadProgressUpdate(100, 100);

  // Transition to kInstalled.
  EXPECT_CALL(observer, OnSpeechRecognitionSmallExpertModelInstalled())
      .Times(1);
  EXPECT_CALL(observer, OnSpeechRecognitionSmallExpertModelStateChanged(
                            State::kInstalled))
      .Times(1);
  installer()->NotifySpeechRecognitionSmallExpertModelInstalledForTesting();
  EXPECT_EQ(installer()->GetSpeechRecognitionSmallExpertModelState(),
            State::kInstalled);
  EXPECT_TRUE(installer()->IsSpeechRecognitionSmallExpertModelInstalled());
  EXPECT_FALSE(installer()->IsSpeechRecognitionSmallExpertModelDownloading());

  // Download progress ignored when not kDownloading.
  EXPECT_CALL(observer, OnSpeechRecognitionSmallExpertModelProgress(
                            testing::_, testing::_, testing::_))
      .Times(0);
  installer()->OnDownloadProgressUpdate(75, 100);
}

TEST_F(SpeechRecognitionSmallExpertModelInstallerTest,
       DownloadProgressIgnoresZeroTotalBytes) {
  testing::NiceMock<MockObserver> observer;
  base::ScopedObservation<SpeechRecognitionSmallExpertModelInstaller,
                          SpeechRecognitionSmallExpertModelInstaller::Observer>
      observation(&observer);
  observation.Observe(installer());

  installer()->NotifySpeechRecognitionSmallExpertModelStateChangedForTesting(
      State::kDownloading);

  EXPECT_CALL(observer, OnSpeechRecognitionSmallExpertModelProgress(
                            testing::_, testing::_, testing::_))
      .Times(0);
  installer()->OnDownloadProgressUpdate(0, 0);
}

TEST_F(SpeechRecognitionSmallExpertModelInstallerTest,
       ErrorHandlingAndMaxRetries) {
  testing::NiceMock<MockObserver> observer;
  base::ScopedObservation<SpeechRecognitionSmallExpertModelInstaller,
                          SpeechRecognitionSmallExpertModelInstaller::Observer>
      observation(&observer);
  observation.Observe(installer());

  // First download attempt and error.
  EXPECT_CALL(observer, OnSpeechRecognitionSmallExpertModelStateChanged(
                            State::kDownloading))
      .Times(1);
  installer()->NotifySpeechRecognitionSmallExpertModelStateChangedForTesting(
      State::kDownloading);

  EXPECT_CALL(observer, OnSpeechRecognitionSmallExpertModelInstallError())
      .Times(1);
  EXPECT_CALL(observer,
              OnSpeechRecognitionSmallExpertModelStateChanged(State::kError))
      .Times(1);
  installer()->HandleSpeechRecognitionSmallExpertModelError();
  EXPECT_EQ(installer()->GetSpeechRecognitionSmallExpertModelState(),
            State::kError);
  EXPECT_EQ(installer()->GetSpeechRecognitionSmallExpertModelRetryCount(), 1);

  // Second download retry and error.
  EXPECT_CALL(observer, OnSpeechRecognitionSmallExpertModelStateChanged(
                            State::kDownloading))
      .Times(1);
  installer()->NotifySpeechRecognitionSmallExpertModelStateChangedForTesting(
      State::kDownloading);

  EXPECT_CALL(observer, OnSpeechRecognitionSmallExpertModelInstallError())
      .Times(1);
  EXPECT_CALL(observer,
              OnSpeechRecognitionSmallExpertModelStateChanged(State::kError))
      .Times(1);
  installer()->HandleSpeechRecognitionSmallExpertModelError();
  EXPECT_EQ(installer()->GetSpeechRecognitionSmallExpertModelState(),
            State::kError);
  EXPECT_EQ(installer()->GetSpeechRecognitionSmallExpertModelRetryCount(), 2);

  // Third download retry and error: reaches kMaxInstallRetries (3) ->
  // kErrorCorruptPersistent.
  EXPECT_CALL(observer, OnSpeechRecognitionSmallExpertModelStateChanged(
                            State::kDownloading))
      .Times(1);
  installer()->NotifySpeechRecognitionSmallExpertModelStateChangedForTesting(
      State::kDownloading);

  EXPECT_CALL(observer, OnSpeechRecognitionSmallExpertModelInstallError())
      .Times(1);
  EXPECT_CALL(observer, OnSpeechRecognitionSmallExpertModelStateChanged(
                            State::kErrorCorruptPersistent))
      .Times(1);
  installer()->HandleSpeechRecognitionSmallExpertModelError();
  EXPECT_EQ(installer()->GetSpeechRecognitionSmallExpertModelState(),
            State::kErrorCorruptPersistent);
  EXPECT_EQ(installer()->GetSpeechRecognitionSmallExpertModelRetryCount(), 3);
}

TEST_F(SpeechRecognitionSmallExpertModelInstallerTest,
       NotifyErrorForTestingHelper) {
  testing::NiceMock<MockObserver> observer;
  base::ScopedObservation<SpeechRecognitionSmallExpertModelInstaller,
                          SpeechRecognitionSmallExpertModelInstaller::Observer>
      observation(&observer);
  observation.Observe(installer());

  EXPECT_CALL(observer, OnSpeechRecognitionSmallExpertModelInstallError())
      .Times(1);
  EXPECT_CALL(observer,
              OnSpeechRecognitionSmallExpertModelStateChanged(State::kError))
      .Times(1);
  installer()->NotifySpeechRecognitionSmallExpertModelErrorForTesting();
  EXPECT_EQ(installer()->GetSpeechRecognitionSmallExpertModelState(),
            State::kError);
}

TEST_F(SpeechRecognitionSmallExpertModelInstallerTest,
       NotifyProgressForTestingHelper) {
  testing::NiceMock<MockObserver> observer;
  base::ScopedObservation<SpeechRecognitionSmallExpertModelInstaller,
                          SpeechRecognitionSmallExpertModelInstaller::Observer>
      observation(&observer);
  observation.Observe(installer());

  EXPECT_CALL(observer,
              OnSpeechRecognitionSmallExpertModelProgress(42, 10.5, testing::_))
      .Times(1);
  installer()->NotifySpeechRecognitionSmallExpertModelProgressForTesting(
      42, 10.5, base::Seconds(5));
}

TEST_F(SpeechRecognitionSmallExpertModelInstallerTest, TeardownResetsState) {
  testing::NiceMock<MockObserver> observer;
  base::ScopedObservation<SpeechRecognitionSmallExpertModelInstaller,
                          SpeechRecognitionSmallExpertModelInstaller::Observer>
      observation(&observer);
  observation.Observe(installer());

  installer()->NotifySpeechRecognitionSmallExpertModelInstalledForTesting();
  EXPECT_EQ(installer()->GetSpeechRecognitionSmallExpertModelState(),
            State::kInstalled);

  EXPECT_CALL(observer, OnSpeechRecognitionSmallExpertModelStateChanged(
                            State::kNotInstalled))
      .Times(1);
  installer()->TeardownSpeechRecognitionSmallExpertModel();
  EXPECT_EQ(installer()->GetSpeechRecognitionSmallExpertModelState(),
            State::kNotInstalled);
  EXPECT_EQ(installer()->GetSpeechRecognitionSmallExpertModelRetryCount(), 0);
}

TEST_F(SpeechRecognitionSmallExpertModelInstallerTest,
       InstalledFailureEmptyPath) {
  installer()->NotifySpeechRecognitionSmallExpertModelStateChangedForTesting(
      State::kDownloading);

  testing::NiceMock<MockObserver> observer;
  base::ScopedObservation<SpeechRecognitionSmallExpertModelInstaller,
                          SpeechRecognitionSmallExpertModelInstaller::Observer>
      observation(&observer);
  observation.Observe(installer());

  // Empty path triggers error during kDownloading.
  EXPECT_CALL(observer, OnSpeechRecognitionSmallExpertModelInstallError())
      .Times(1);
  EXPECT_CALL(observer,
              OnSpeechRecognitionSmallExpertModelStateChanged(State::kError))
      .Times(1);
  installer()->OnSpeechRecognitionSmallExpertModelInstalled(base::FilePath());
  EXPECT_EQ(installer()->GetSpeechRecognitionSmallExpertModelState(),
            State::kError);
}

TEST_F(SpeechRecognitionSmallExpertModelInstallerTest,
       InstalledSuccessOptimizationGuidePath) {
  installer()->NotifySpeechRecognitionSmallExpertModelStateChangedForTesting(
      State::kDownloading);

  testing::NiceMock<MockObserver> observer;
  base::ScopedObservation<SpeechRecognitionSmallExpertModelInstaller,
                          SpeechRecognitionSmallExpertModelInstaller::Observer>
      observation(&observer);
  observation.Observe(installer());

  // optimization_guide path triggers installed.
  EXPECT_CALL(observer, OnSpeechRecognitionSmallExpertModelInstalled())
      .Times(1);
  EXPECT_CALL(observer, OnSpeechRecognitionSmallExpertModelStateChanged(
                            State::kInstalled))
      .Times(1);
  installer()->OnSpeechRecognitionSmallExpertModelInstalled(
      base::FilePath(FILE_PATH_LITERAL("optimization_guide")));
  EXPECT_EQ(installer()->GetSpeechRecognitionSmallExpertModelState(),
            State::kInstalled);
}

TEST_F(SpeechRecognitionSmallExpertModelInstallerTest, LocalStatePersistence) {
  base::FilePath sample_path =
      base::FilePath(FILE_PATH_LITERAL("/path/to/speech_model"));
  pref_service()->SetFilePath(prefs::kSpeechRecognitionSmallExpertModelPath,
                              sample_path);
  EXPECT_EQ(installer()->GetSpeechRecognitionSmallExpertModelPath(),
            sample_path);
}

TEST_F(SpeechRecognitionSmallExpertModelInstallerTest,
       DeleteDirectorySanitizationRejectsNonMatchingBaseName) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Create a directory whose ancestor contains the substring, but base name
  // differs.
  base::FilePath ancestor_with_substr =
      temp_dir.GetPath().AppendASCII("speech_recognition_small_expert_model");
  ASSERT_TRUE(base::CreateDirectory(ancestor_with_substr));
  base::FilePath sub_dir = ancestor_with_substr.AppendASCII("unrelated_folder");
  ASSERT_TRUE(base::CreateDirectory(sub_dir));
  ASSERT_TRUE(base::PathExists(sub_dir));

  // Should NOT delete because BaseName is "unrelated_folder".
  TestSpeechRecognitionSmallExpertModelInstaller::
      DeleteSpeechRecognitionSmallExpertModelFilesAsync(sub_dir);
  EXPECT_TRUE(base::PathExists(sub_dir));

  // Should delete because BaseName is "speech_recognition_small_expert_model".
  TestSpeechRecognitionSmallExpertModelInstaller::
      DeleteSpeechRecognitionSmallExpertModelFilesAsync(ancestor_with_substr);
  EXPECT_FALSE(base::PathExists(ancestor_with_substr));
}

TEST_F(SpeechRecognitionSmallExpertModelInstallerTest,
       StateInitializedFromPrefsWithFeatureEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      media::kLiveCaptionSpeechRecognitionSmallExpertModel);

  base::FilePath sample_path =
      base::FilePath(FILE_PATH_LITERAL("/path/to/speech_model"));
  pref_service()->SetFilePath(prefs::kSpeechRecognitionSmallExpertModelPath,
                              sample_path);
  auto fresh_installer =
      std::make_unique<TestSpeechRecognitionSmallExpertModelInstaller>();
  EXPECT_EQ(fresh_installer->GetSpeechRecognitionSmallExpertModelState(),
            State::kInstalled);
  EXPECT_TRUE(fresh_installer->IsSpeechRecognitionSmallExpertModelInstalled());
}

TEST_F(SpeechRecognitionSmallExpertModelInstallerTest,
       StateNotInstalledFromPrefsWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      media::kLiveCaptionSpeechRecognitionSmallExpertModel);

  base::FilePath sample_path =
      base::FilePath(FILE_PATH_LITERAL("/path/to/speech_model"));
  pref_service()->SetFilePath(prefs::kSpeechRecognitionSmallExpertModelPath,
                              sample_path);
  auto fresh_installer =
      std::make_unique<TestSpeechRecognitionSmallExpertModelInstaller>();
  EXPECT_EQ(fresh_installer->GetSpeechRecognitionSmallExpertModelState(),
            State::kNotInstalled);
  EXPECT_FALSE(fresh_installer->IsSpeechRecognitionSmallExpertModelInstalled());
}

TEST_F(SpeechRecognitionSmallExpertModelInstallerTest,
       SuccessfulInstallResetsRetryCount) {
  installer()->HandleSpeechRecognitionSmallExpertModelError();
  EXPECT_EQ(installer()->GetSpeechRecognitionSmallExpertModelRetryCount(), 1);
  EXPECT_EQ(installer()->GetSpeechRecognitionSmallExpertModelState(),
            State::kError);

  // Transition to downloading for the install attempt.
  installer()->NotifySpeechRecognitionSmallExpertModelStateChangedForTesting(
      State::kDownloading);
  EXPECT_EQ(installer()->GetSpeechRecognitionSmallExpertModelRetryCount(), 1);

  installer()->OnSpeechRecognitionSmallExpertModelInstalled(
      base::FilePath(FILE_PATH_LITERAL("optimization_guide")));
  EXPECT_EQ(installer()->GetSpeechRecognitionSmallExpertModelState(),
            State::kInstalled);
  EXPECT_EQ(installer()->GetSpeechRecognitionSmallExpertModelRetryCount(), 0);
}

TEST_F(SpeechRecognitionSmallExpertModelInstallerTest,
       NoDuplicateNotificationWhenAlreadyNotInstalled) {
  testing::NiceMock<MockObserver> observer;
  base::ScopedObservation<SpeechRecognitionSmallExpertModelInstaller,
                          SpeechRecognitionSmallExpertModelInstaller::Observer>
      observation(&observer);
  observation.Observe(installer());

  // Installer is already kNotInstalled; passing nullptr should not notify.
  EXPECT_CALL(observer,
              OnSpeechRecognitionSmallExpertModelStateChanged(testing::_))
      .Times(0);
  installer()->InstallSpeechRecognitionSmallExpertModel(nullptr);
  EXPECT_EQ(installer()->GetSpeechRecognitionSmallExpertModelState(),
            State::kNotInstalled);
}

TEST_F(SpeechRecognitionSmallExpertModelInstallerTest,
       UninstallClearsPrefsResetsStateAndRetries) {
  testing::NiceMock<MockObserver> observer;
  base::ScopedObservation<SpeechRecognitionSmallExpertModelInstaller,
                          SpeechRecognitionSmallExpertModelInstaller::Observer>
      observation(&observer);
  observation.Observe(installer());

  // Set up an installed state with pref and simulated retries.
  base::FilePath sample_path =
      base::FilePath(FILE_PATH_LITERAL("/path/to/speech_model"));
  pref_service()->SetFilePath(prefs::kSpeechRecognitionSmallExpertModelPath,
                              sample_path);
  installer()->NotifySpeechRecognitionSmallExpertModelInstalledForTesting();
  installer()->HandleSpeechRecognitionSmallExpertModelError();
  EXPECT_EQ(installer()->GetSpeechRecognitionSmallExpertModelRetryCount(), 1);
  EXPECT_EQ(installer()->GetSpeechRecognitionSmallExpertModelPath(),
            sample_path);

  EXPECT_CALL(observer, OnSpeechRecognitionSmallExpertModelStateChanged(
                            State::kNotInstalled))
      .Times(1);

  installer()->UninstallSpeechRecognitionSmallExpertModel();

  EXPECT_TRUE(installer()->GetSpeechRecognitionSmallExpertModelPath().empty());
  EXPECT_EQ(installer()->GetSpeechRecognitionSmallExpertModelState(),
            State::kNotInstalled);
  EXPECT_FALSE(installer()->IsSpeechRecognitionSmallExpertModelInstalled());
  EXPECT_EQ(installer()->GetSpeechRecognitionSmallExpertModelRetryCount(), 0);
}

TEST_F(SpeechRecognitionSmallExpertModelInstallerTest,
       InstallWithDisabledLiveCaptionPrefDoesNotDownload) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      media::kLiveCaptionSpeechRecognitionSmallExpertModel);

  TestingProfile* profile =
      profile_manager()->CreateTestingProfile("disabled_caption_profile");
  profile->GetPrefs()->SetBoolean(prefs::kLiveCaptionEnabled, false);
  profile->GetPrefs()->SetBoolean(prefs::kHeadlessCaptionEnabled, false);

  testing::NiceMock<MockObserver> observer;
  base::ScopedObservation<SpeechRecognitionSmallExpertModelInstaller,
                          SpeechRecognitionSmallExpertModelInstaller::Observer>
      observation(&observer);
  observation.Observe(installer());

  // Observers should not receive download or error callbacks.
  EXPECT_CALL(observer, OnSpeechRecognitionSmallExpertModelStateChanged(
                            State::kDownloading))
      .Times(0);
  EXPECT_CALL(observer, OnSpeechRecognitionSmallExpertModelInstallError())
      .Times(0);

  installer()->InstallSpeechRecognitionSmallExpertModel(profile);

  EXPECT_EQ(installer()->GetSpeechRecognitionSmallExpertModelState(),
            State::kNotInstalled);
  EXPECT_FALSE(installer()->IsSpeechRecognitionSmallExpertModelDownloading());
  EXPECT_FALSE(installer()->IsSpeechRecognitionSmallExpertModelInstalled());
}

TEST_F(SpeechRecognitionSmallExpertModelInstallerTest,
       InstallWithDisabledLiveCaptionPrefResetsStateToNotInstalled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      media::kLiveCaptionSpeechRecognitionSmallExpertModel);

  TestingProfile* profile =
      profile_manager()->CreateTestingProfile("reset_state_profile");
  profile->GetPrefs()->SetBoolean(prefs::kLiveCaptionEnabled, false);
  profile->GetPrefs()->SetBoolean(prefs::kHeadlessCaptionEnabled, false);

  installer()->NotifySpeechRecognitionSmallExpertModelErrorForTesting();
  EXPECT_EQ(installer()->GetSpeechRecognitionSmallExpertModelState(),
            State::kError);

  testing::NiceMock<MockObserver> observer;
  base::ScopedObservation<SpeechRecognitionSmallExpertModelInstaller,
                          SpeechRecognitionSmallExpertModelInstaller::Observer>
      observation(&observer);
  observation.Observe(installer());

  EXPECT_CALL(observer, OnSpeechRecognitionSmallExpertModelStateChanged(
                            State::kNotInstalled))
      .Times(1);

  installer()->InstallSpeechRecognitionSmallExpertModel(profile);

  EXPECT_EQ(installer()->GetSpeechRecognitionSmallExpertModelState(),
            State::kNotInstalled);
  EXPECT_FALSE(installer()->IsSpeechRecognitionSmallExpertModelDownloading());
  EXPECT_FALSE(installer()->IsSpeechRecognitionSmallExpertModelInstalled());
}

TEST_F(SpeechRecognitionSmallExpertModelInstallerTest,
       InstallWithNullOptGuideDoesNotIncrementRetryCount) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      media::kLiveCaptionSpeechRecognitionSmallExpertModel);

  // A profile without OptimizationGuideKeyedService (e.g. Guest profile)
  TestingProfile* profile =
      profile_manager()->CreateTestingProfile("no_opt_guide_profile");
  profile->GetPrefs()->SetBoolean(prefs::kHeadlessCaptionEnabled, true);

  EXPECT_EQ(installer()->GetSpeechRecognitionSmallExpertModelRetryCount(), 0);

  testing::NiceMock<MockObserver> observer;
  base::ScopedObservation<SpeechRecognitionSmallExpertModelInstaller,
                          SpeechRecognitionSmallExpertModelInstaller::Observer>
      observation(&observer);
  observation.Observe(installer());

  EXPECT_CALL(observer, OnSpeechRecognitionSmallExpertModelInstallError())
      .Times(0);

  installer()->InstallSpeechRecognitionSmallExpertModel(profile);

  EXPECT_EQ(installer()->GetSpeechRecognitionSmallExpertModelState(),
            State::kNotInstalled);
  EXPECT_EQ(installer()->GetSpeechRecognitionSmallExpertModelRetryCount(), 0);
  EXPECT_FALSE(installer()->IsSpeechRecognitionSmallExpertModelDownloading());
  EXPECT_FALSE(installer()->IsSpeechRecognitionSmallExpertModelInstalled());
}

}  // namespace
}  // namespace speech
