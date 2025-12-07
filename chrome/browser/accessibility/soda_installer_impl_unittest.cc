// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/soda_installer_impl.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/component_updater/soda_component_installer.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/soda/constants.h"
#include "components/soda/pref_names.h"
#include "components/soda/soda_installer.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const speech::LanguageCode kEnglishLocale = speech::LanguageCode::kEnUs;
const speech::LanguageCode kJapaneseLocale = speech::LanguageCode::kJaJp;
const base::TimeDelta kSodaUninstallTime = base::Days(30);

constexpr char kSodaEnglishLanguageInstallationResult[] =
    "SodaInstaller.Language.en-US.InstallationResult";
}  // namespace

namespace speech {

class MockSodaInstallerImpl : public SodaInstallerImpl {
 public:
  MockSodaInstallerImpl() = default;
  ~MockSodaInstallerImpl() override = default;

  void InstallSoda(PrefService* global_prefs) override {
    OnSodaBinaryInstalled();
  }

  void InstallLanguage(const std::string& language,
                       PrefService* global_prefs) override {
    SodaInstaller::RegisterLanguage(language, global_prefs);
    OnSodaLanguagePackInstalled(speech::GetLanguageCode(language));
  }
};

class SodaInstallerImplTest : public testing::Test {
 protected:
  void SetUp() override {
    soda_installer_impl_ = std::make_unique<MockSodaInstallerImpl>();
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    soda_installer_impl_->RegisterLocalStatePrefs(pref_service_->registry());
    pref_service_->registry()->RegisterBooleanPref(prefs::kLiveCaptionEnabled,
                                                   true);
    pref_service_->registry()->RegisterBooleanPref(
        prefs::kHeadlessCaptionEnabled, false);
    pref_service_->registry()->RegisterStringPref(
        prefs::kLiveCaptionLanguageCode, kUsEnglishLocale);
  }

  void TearDown() override {
    soda_installer_impl_.reset();
    pref_service_.reset();
  }

  SodaInstallerImpl* GetInstance() { return soda_installer_impl_.get(); }

  bool IsSodaInstalled(
      speech::LanguageCode language_code = speech::LanguageCode::kEnUs) {
    return soda_installer_impl_->IsSodaInstalled(language_code);
  }

  bool IsLanguagePackInstalled(speech::LanguageCode language_code) {
    return soda_installer_impl_->IsLanguageInstalled(language_code);
  }

  bool IsSodaDownloading(
      speech::LanguageCode language_code = speech::LanguageCode::kEnUs) {
    return soda_installer_impl_->IsSodaDownloading(language_code);
  }

  void InstallLanguage(const std::string& language) {
    return soda_installer_impl_->InstallLanguage(language, pref_service_.get());
  }

  void UninstallLanguage(const std::string& language) {
    return soda_installer_impl_->UninstallLanguage(language,
                                                   pref_service_.get());
  }

  void Init() {
    soda_installer_impl_->Init(pref_service_.get(), pref_service_.get());
    task_environment_.RunUntilIdle();
  }

  void SetUninstallTimer(
      speech::LanguageCode language_code = speech::LanguageCode::kEnUs) {
    soda_installer_impl_->SetUninstallTimer(pref_service_.get(),
                                            GetLanguageName(language_code));
  }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  void SetLiveCaptionEnabled(bool enabled) {
    pref_service_->SetManagedPref(prefs::kLiveCaptionEnabled,
                                  std::make_unique<base::Value>(enabled));
  }

  void SetHeadlessCaptionEnabled(bool enabled) {
    pref_service_->SetManagedPref(prefs::kHeadlessCaptionEnabled,
                                  std::make_unique<base::Value>(enabled));
  }

  void SetSodaInstallerInitialized(bool initialized) {
    soda_installer_impl_->soda_installer_initialized_ = initialized;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<MockSodaInstallerImpl> soda_installer_impl_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
};

TEST_F(SodaInstallerImplTest, IsSodaInstalled) {
  base::HistogramTester histogram_tester;

  ASSERT_FALSE(IsSodaInstalled());
  Init();
  ASSERT_TRUE(IsSodaInstalled());

  // SODA binary and english language installation never failed.
  histogram_tester.ExpectBucketCount(kSodaBinaryInstallationResult, 0, 0);
  histogram_tester.ExpectBucketCount(kSodaEnglishLanguageInstallationResult, 0,
                                     0);

  // SODA binary and english language installation succeeded once.
  histogram_tester.ExpectBucketCount(kSodaBinaryInstallationResult, 1, 1);
  histogram_tester.ExpectBucketCount(kSodaEnglishLanguageInstallationResult, 1,
                                     1);
}

TEST_F(SodaInstallerImplTest, IsDownloading) {
  ASSERT_FALSE(IsSodaDownloading());
  Init();
  ASSERT_FALSE(IsSodaDownloading());
}

TEST_F(SodaInstallerImplTest, IsLanguagePackInstalled) {
  ASSERT_FALSE(IsLanguagePackInstalled(kEnglishLocale));
  Init();
  ASSERT_TRUE(IsLanguagePackInstalled(kEnglishLocale));
  ASSERT_FALSE(IsLanguagePackInstalled(kJapaneseLocale));
}

TEST_F(SodaInstallerImplTest, UninstallLanguagePacks) {
  ASSERT_FALSE(IsLanguagePackInstalled(kEnglishLocale));
  ASSERT_FALSE(IsLanguagePackInstalled(kJapaneseLocale));
  Init();
  ASSERT_TRUE(IsLanguagePackInstalled(kEnglishLocale));
  ASSERT_FALSE(IsLanguagePackInstalled(kJapaneseLocale));

  InstallLanguage(speech::GetLanguageName(kJapaneseLocale));
  ASSERT_TRUE(IsLanguagePackInstalled(kEnglishLocale));
  ASSERT_TRUE(IsLanguagePackInstalled(kJapaneseLocale));

  UninstallLanguage(speech::GetLanguageName(kEnglishLocale));
  ASSERT_FALSE(IsLanguagePackInstalled(kEnglishLocale));
  ASSERT_TRUE(IsLanguagePackInstalled(kJapaneseLocale));
}

TEST_F(SodaInstallerImplTest, AvailableLanguagesTest) {
  auto actual_available_langs = soda_installer_impl_->GetAvailableLanguages();
  auto expected_available_langs =
      soda_installer_impl_->GetLiveCaptionEnabledLanguages();
  EXPECT_THAT(actual_available_langs,
              ::testing::UnorderedElementsAreArray(expected_available_langs));
}

TEST_F(SodaInstallerImplTest, UninstallSodaAfterThirtyDays) {
  Init();
  ASSERT_TRUE(IsSodaInstalled());

  // Turn off features that use SODA so that the uninstall timer can be set.
  SetLiveCaptionEnabled(false);
  InstallLanguage(speech::GetLanguageName(kJapaneseLocale));
  InstallLanguage(speech::GetLanguageName(kEnglishLocale));
  SetUninstallTimer(kEnglishLocale);
  ASSERT_TRUE(IsSodaInstalled(kEnglishLocale));
  ASSERT_TRUE(IsSodaInstalled(kJapaneseLocale));

  // If 30 days pass without the uninstall time being reset, SODA will be
  // uninstalled the next time Init() is called.
  // Set SodaInstaller initialized state to false to mimic a browser shutdown.
  SetSodaInstallerInitialized(false);
  FastForwardBy(kSodaUninstallTime / 2);

  // Simulate the usage of the Japanese language pack by pushing the
  // uninstallation time back.
  SetUninstallTimer(kJapaneseLocale);
  FastForwardBy(kSodaUninstallTime / 2);
  ASSERT_TRUE(IsSodaInstalled(kEnglishLocale));
  ASSERT_TRUE(IsSodaInstalled(kJapaneseLocale));

  // The uninstallation process doesn't start until Init() is called again.
  Init();
  ASSERT_FALSE(IsSodaInstalled(kEnglishLocale));
  ASSERT_TRUE(IsSodaInstalled(kJapaneseLocale));
}

TEST_F(SodaInstallerImplTest, ReregisterSodaWithinThirtyDays) {
  Init();
  ASSERT_TRUE(IsSodaInstalled());

  // Turn off features that use SODA so that the uninstall timer can be set.
  SetLiveCaptionEnabled(false);
  SetUninstallTimer();
  ASSERT_TRUE(IsSodaInstalled());

  // Fast forward SODA and manually uninstall SODA to simulate a browser
  // restart.
  SetSodaInstallerInitialized(false);
  FastForwardBy(base::Days(1));
  GetInstance()->UninstallSodaForTesting();
  ASSERT_FALSE(IsSodaInstalled());

  // SODA should be registered because so it recently used within the last 30
  // days.
  Init();
  ASSERT_TRUE(IsSodaInstalled());
}

// Tests that SODA stays installed if thirty days pass and a feature using SODA
// is enabled.
TEST_F(SodaInstallerImplTest,
       SodaStaysInstalledAfterThirtyDaysIfFeatureEnabled) {
  Init();
  ASSERT_TRUE(IsSodaInstalled());

  SetUninstallTimer();
  ASSERT_TRUE(IsSodaInstalled());

  // Set SodaInstaller initialized state to false to mimic a browser shutdown.
  FastForwardBy(kSodaUninstallTime);
  ASSERT_TRUE(IsSodaInstalled());

  Init();
  ASSERT_TRUE(IsSodaInstalled());
}

// Tests that SODA can be reinstalled after previously being uninstalled.
TEST_F(SodaInstallerImplTest, ReinstallSoda) {
  Init();
  ASSERT_TRUE(IsSodaInstalled());

  // Turn off features that use SODA so that the uninstall timer can be set.
  SetLiveCaptionEnabled(false);
  SetUninstallTimer();
  ASSERT_TRUE(IsSodaInstalled());

  // If 30 days pass without the uninstall time being pushed, SODA will be
  // uninstalled the next time Init() is called.
  // Set SodaInstaller initialized state to false to mimic a browser shutdown.
  FastForwardBy(kSodaUninstallTime);
  ASSERT_TRUE(IsSodaInstalled());

  // Set SodaInstaller initialized state to false to mimic a browser shutdown.
  SetSodaInstallerInitialized(false);

  // The uninstallation process doesn't start until Init() is called again.
  Init();
  ASSERT_FALSE(IsSodaInstalled());

  SetLiveCaptionEnabled(true);
  Init();
  ASSERT_TRUE(IsSodaInstalled());
}

// Tests that SODA is not installed if nothing is using it.
TEST_F(SodaInstallerImplTest, NotInstalledIfNoConsumers) {
  SetLiveCaptionEnabled(false);
  SetHeadlessCaptionEnabled(false);
  Init();
  ASSERT_FALSE(IsSodaInstalled());
}

// Tests that headless captions installs SODA.
TEST_F(SodaInstallerImplTest, InstalledForHeadlessCaption) {
  SetLiveCaptionEnabled(false);
  SetHeadlessCaptionEnabled(true);
  Init();
  ASSERT_TRUE(IsSodaInstalled());
}

class SodaInstallerImplProgressTest : public testing::Test,
                                      public SodaInstaller::Observer {
 protected:
  void SetUp() override {
    soda_installer_impl_ = std::make_unique<SodaInstallerImpl>();
    soda_installer_impl_->AddObserver(this);
  }

  void TearDown() override {
    soda_installer_impl_->RemoveObserver(this);
    soda_installer_impl_.reset();
  }

  // SodaInstaller::Observer
  void OnSodaInstalled(LanguageCode language_code) override {}
  void OnSodaInstallError(LanguageCode language_code,
                          SodaInstaller::ErrorCode error_code) override {}
  void OnSodaProgress(LanguageCode language_code, int progress) override {
    last_progress_ = progress;
  }

  std::unique_ptr<SodaInstallerImpl> soda_installer_impl_;
  std::optional<int> last_progress_;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(SodaInstallerImplProgressTest,
       UpdateAndNotifyOnSodaProgressClampsProgress) {
  update_client::CrxUpdateItem item;
  item.id = component_updater::SodaComponentInstallerPolicy::GetExtensionId();
  item.state = update_client::ComponentState::kDownloading;
  item.total_bytes = 100;
  item.downloaded_bytes = 110;

  soda_installer_impl_->OnEvent(item);
  ASSERT_TRUE(last_progress_.has_value());
  EXPECT_EQ(100, last_progress_.value());
}

}  // namespace speech
