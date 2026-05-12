// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/background/glic/os_icon_provider_mac.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/background/glic/glic_status_icon.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace glic {
namespace {

static const char kOtherAppID[] = "org.chromium.OtherApp";

class MockGlicStatusIcon : public GlicStatusIcon {
 public:
  MockGlicStatusIcon() : GlicStatusIcon(nullptr, nullptr) {}
  ~MockGlicStatusIcon() override = default;
  MOCK_METHOD(void, SetIcon, (const gfx::ImageSkia&));
};

class TestingDelegate : public OSIconProviderMac::Delegate {
 public:
  ~TestingDelegate() override = default;

  bool IsAppRunning(const std::string& bundle_id) const override {
    return running_;
  }

  void SetRunning(bool running) { running_ = running; }

 private:
  bool running_ = false;
};

}  // namespace

class OSIconProviderMacUnitTest : public testing::Test {
 public:
  OSIconProviderMacUnitTest() {
    ::glic::prefs::RegisterLocalStatePrefs(prefs_.registry());
  }
  ~OSIconProviderMacUnitTest() override = default;
  void SetUp() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicChromeStatusIcon,
        {{features::kGlicChromeStatusIconOtherAppID.name, kOtherAppID},
         {features::kGlicChromeStatusIconLogOnly.name, "false"}});
    CreateOSIconProviderMac(std::make_unique<TestingDelegate>());
  }

  void TearDown() override {
    delegate_ = nullptr;
    os_icon_provider_mac_.reset();
  }

 protected:
  void CreateOSIconProviderMac(std::unique_ptr<TestingDelegate> delegate) {
    delegate_ = delegate.get();
    os_icon_provider_mac_ = std::make_unique<OSIconProviderMac>(
        prefs_, glic_status_icon_, std::move(delegate));
  }

  base::test::ScopedFeatureList feature_list_;
  TestingPrefServiceSimple prefs_;
  ::testing::NiceMock<MockGlicStatusIcon> glic_status_icon_;
  std::unique_ptr<OSIconProviderMac> os_icon_provider_mac_;
  raw_ptr<TestingDelegate> delegate_ = nullptr;
};

TEST_F(OSIconProviderMacUnitTest, FlagEnabledOtherAppNotRunning) {
  EXPECT_FALSE(prefs_.GetBoolean(prefs::kGlicUseAltOSIcon));
  base::HistogramTester histogram_tester;

  CreateOSIconProviderMac(std::make_unique<TestingDelegate>());
  // Alt icon pref not set: other app wasn't running.
  histogram_tester.ExpectUniqueSample("Glic.SetAltOSIcon.OnChromeStart", false,
                                      1);
}

TEST_F(OSIconProviderMacUnitTest,
       FlagEnabledOtherAppAlreadyRunning_PrefNotSet) {
  EXPECT_FALSE(prefs_.GetBoolean(prefs::kGlicUseAltOSIcon));
  base::HistogramTester histogram_tester;

  auto delegate = std::make_unique<TestingDelegate>();
  delegate->SetRunning(true);
  CreateOSIconProviderMac(std::move(delegate));
  EXPECT_TRUE(prefs_.GetBoolean(prefs::kGlicUseAltOSIcon));
  // Alt icon pref was set.
  histogram_tester.ExpectUniqueSample("Glic.SetAltOSIcon.OnChromeStart", true,
                                      1);
}

TEST_F(OSIconProviderMacUnitTest, FlagEnabledOtherAppAlreadyRunning_PrefSet) {
  EXPECT_FALSE(prefs_.GetBoolean(prefs::kGlicUseAltOSIcon));
  prefs_.SetBoolean(prefs::kGlicUseAltOSIcon, true);
  base::HistogramTester histogram_tester;

  auto delegate = std::make_unique<TestingDelegate>();
  delegate->SetRunning(true);
  CreateOSIconProviderMac(std::move(delegate));
  EXPECT_TRUE(prefs_.GetBoolean(prefs::kGlicUseAltOSIcon));
  // Alt icon pref was already true.
  histogram_tester.ExpectUniqueSample("Glic.SetAltOSIcon.OnChromeStart", false,
                                      1);
}

TEST_F(OSIconProviderMacUnitTest, FlagEnabledOtherAppNotRunning_PrefSet) {
  EXPECT_FALSE(prefs_.GetBoolean(prefs::kGlicUseAltOSIcon));
  prefs_.SetBoolean(prefs::kGlicUseAltOSIcon, true);
  base::HistogramTester histogram_tester;

  auto delegate = std::make_unique<TestingDelegate>();
  delegate->SetRunning(false);
  CreateOSIconProviderMac(std::move(delegate));
  EXPECT_TRUE(prefs_.GetBoolean(prefs::kGlicUseAltOSIcon));
  // Alt icon pref was already true.
  histogram_tester.ExpectUniqueSample("Glic.SetAltOSIcon.OnChromeStart", false,
                                      1);
}

TEST_F(OSIconProviderMacUnitTest, FlagEnabledOtherAppStarts_PrefNotSet) {
  EXPECT_FALSE(prefs_.GetBoolean(prefs::kGlicUseAltOSIcon));
  base::HistogramTester histogram_tester;

  os_icon_provider_mac_->OnRunningAppsUpdated("com.example.SomeApp");
  EXPECT_FALSE(prefs_.GetBoolean(prefs::kGlicUseAltOSIcon));

  EXPECT_CALL(glic_status_icon_, SetIcon(_));
  os_icon_provider_mac_->OnRunningAppsUpdated(kOtherAppID);
  EXPECT_TRUE(prefs_.GetBoolean(prefs::kGlicUseAltOSIcon));
  // Pref went from false to true, so log true.
  histogram_tester.ExpectUniqueSample("Glic.SetAltOSIcon.OnOtherAppStart", true,
                                      1);
}

TEST_F(OSIconProviderMacUnitTest, FlagEnabledOtherAppStarts_PrefSet) {
  EXPECT_FALSE(prefs_.GetBoolean(prefs::kGlicUseAltOSIcon));
  prefs_.SetBoolean(prefs::kGlicUseAltOSIcon, true);
  base::HistogramTester histogram_tester;

  os_icon_provider_mac_->OnRunningAppsUpdated("com.example.SomeApp");

  // No call to SetIcon() because the pref was already set, and
  // OSIconProviderMac::GetIcon() will should have returned the correct icon
  // based on the pref.
  EXPECT_CALL(glic_status_icon_, SetIcon(_)).Times(0);
  os_icon_provider_mac_->OnRunningAppsUpdated(kOtherAppID);
  EXPECT_TRUE(prefs_.GetBoolean(prefs::kGlicUseAltOSIcon));
  // Pref was already true, so log false, since it was not changed.
  histogram_tester.ExpectUniqueSample("Glic.SetAltOSIcon.OnOtherAppStart",
                                      false, 1);
}

TEST_F(OSIconProviderMacUnitTest, FlagDisabled) {
  prefs_.SetBoolean(prefs::kGlicUseAltOSIcon, true);

  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kGlicChromeStatusIcon);
  base::HistogramTester histogram_tester;

  CreateOSIconProviderMac(nullptr);

  // Pref should have been cleared.
  EXPECT_FALSE(prefs_.HasPrefPath(prefs::kGlicUseAltOSIcon));
  EXPECT_FALSE(prefs_.GetBoolean(prefs::kGlicUseAltOSIcon));

  histogram_tester.ExpectTotalCount("Glic.SetAltOSIcon.OnChromeStart", 0);
  histogram_tester.ExpectTotalCount("Glic.SetAltOSIcon.OnOtherAppStart", 0);
}

TEST_F(OSIconProviderMacUnitTest, FlagEnabledOtherAppStarts_LogOnly) {
  EXPECT_FALSE(prefs_.GetBoolean(prefs::kGlicUseAltOSIcon));
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kGlicChromeStatusIcon,
      {{features::kGlicChromeStatusIconOtherAppID.name, kOtherAppID},
       {features::kGlicChromeStatusIconLogOnly.name, "true"}});

  os_icon_provider_mac_->OnRunningAppsUpdated("com.example.SomeApp");
  EXPECT_FALSE(prefs_.GetBoolean(prefs::kGlicUseAltOSIcon));

  EXPECT_CALL(glic_status_icon_, SetIcon(_)).Times(0);
  os_icon_provider_mac_->OnRunningAppsUpdated(kOtherAppID);
  // Pref isn't written when LogOnly is true.
  EXPECT_FALSE(prefs_.GetBoolean(prefs::kGlicUseAltOSIcon));
  histogram_tester.ExpectUniqueSample("Glic.SetAltOSIcon.OnOtherAppStart", true,
                                      1);
}

}  // namespace glic
