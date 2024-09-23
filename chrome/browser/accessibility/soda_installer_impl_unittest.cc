// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/soda_installer_impl.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/soda/constants.h"
#include "components/soda/pref_names.h"
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
  }

  void SetUninstallTimer() {
    soda_installer_impl_->SetUninstallTimer(pref_service_.get(),
                                            pref_service_.get());
  }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  void SetLiveCaptionEnabled(bool enabled) {
    pref_service_->SetManagedPref(prefs::kLiveCaptionEnabled,
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
  SetUninstallTimer();
  ASSERT_TRUE(IsSodaInstalled());

  // If 30 days pass without the uninstall time being reset, SODA will be
  // uninstalled the next time Init() is called.
  // Set SodaInstaller initialized state to false to mimic a browser shutdown.
  SetSodaInstallerInitialized(false);
  FastForwardBy(kSodaUninstallTime);
  ASSERT_TRUE(IsSodaInstalled());

  // The uninstallation process doesn't start until Init() is called again.
  Init();
  ASSERT_FALSE(IsSodaInstalled());
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

}  // namespace speech
