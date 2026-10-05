// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/background/glic/os_icon_provider_mac.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/background/glic/glic_status_icon.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

class MockGlicStatusIcon : public GlicStatusIcon {
 public:
  MockGlicStatusIcon() : GlicStatusIcon(nullptr, nullptr) {}
  ~MockGlicStatusIcon() override = default;
  MOCK_METHOD(void, SetIcon, (const gfx::ImageSkia&));
};

}  // namespace

class OSIconProviderMacUnitTest : public testing::Test {
 public:
  OSIconProviderMacUnitTest() {
    ::glic::prefs::RegisterLocalStatePrefs(prefs_.registry());
  }
  ~OSIconProviderMacUnitTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};

  TestingPrefServiceSimple prefs_;
  ::testing::NiceMock<MockGlicStatusIcon> glic_status_icon_;
};

TEST_F(OSIconProviderMacUnitTest, ClearsAltIconPrefOnConstruction) {
  prefs_.SetBoolean(prefs::kGlicUseAltOSIcon, true);

  OSIconProviderMac provider(prefs_, glic_status_icon_);

  EXPECT_FALSE(prefs_.HasPrefPath(prefs::kGlicUseAltOSIcon));
  EXPECT_FALSE(prefs_.GetBoolean(prefs::kGlicUseAltOSIcon));
}

TEST_F(OSIconProviderMacUnitTest, GetIconWithOSIconVariantEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGlicOSIconVariant);

  OSIconProviderMac provider(prefs_, glic_status_icon_);
  EXPECT_FALSE(provider.GetIcon().isNull());
}

TEST_F(OSIconProviderMacUnitTest, GetIconWithOSIconVariantDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kGlicOSIconVariant);

  OSIconProviderMac provider(prefs_, glic_status_icon_);
  EXPECT_FALSE(provider.GetIcon().isNull());
}

}  // namespace glic
