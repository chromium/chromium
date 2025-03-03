// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_button_controller.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_button_controller_delegate.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/glic_test_util.h"
#include "chrome/browser/glic/glic_vector_icon_manager.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/vector_icon_types.h"

namespace glic {
namespace {

class MockGlicKeyedService : public glic::GlicKeyedService {
 public:
  MockGlicKeyedService(content::BrowserContext* browser_context,
                       signin::IdentityManager* identity_manager,
                       GlicProfileManager* profile_manager)
      : GlicKeyedService(Profile::FromBrowserContext(browser_context),
                         identity_manager,
                         profile_manager) {}
  MOCK_METHOD(void, TryPreload, (), (override));
};

class MockGlicButtonControllerDelegate
    : public glic::GlicButtonControllerDelegate {
 public:
  void SetShowState(bool show) override { show_state_ = show; }
  void SetIcon(const gfx::VectorIcon& icon) override { icon_ = &icon; }

  bool show_state() const { return show_state_; }
  const gfx::VectorIcon* icon() const { return icon_; }

 private:
  bool show_state_ = false;
  raw_ptr<const gfx::VectorIcon> icon_ = nullptr;
};

}  // namespace

class GlicButtonControllerTest : public testing::Test {
 public:
  void SetUp() override {
    // Enable kGlic and kTabstripComboButton by default for testing.
    scoped_feature_list_.InitWithFeatures(
        {features::kGlic, features::kTabstripComboButton}, {});

    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(testing_profile_manager_->SetUp());
    TestingBrowserProcess::GetGlobal()->CreateGlobalFeaturesForTesting();
    profile_ = testing_profile_manager_->CreateTestingProfile("profile");

    mock_glic_service_ = std::make_unique<MockGlicKeyedService>(
        profile_, identity_test_environment.identity_manager(),
        &glic_profile_manager_);

    glic_button_controller_ = std::make_unique<GlicButtonController>(
        profile_, &mock_glic_controller_delegate_, mock_glic_service_.get());
    ForceSigninAndModelExecutionCapability(profile_);
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->GetFeatures()->Shutdown();
    scoped_feature_list_.Reset();
  }

  GlicButtonController* controller() { return glic_button_controller_.get(); }

  MockGlicButtonControllerDelegate* controller_delegate() {
    return &mock_glic_controller_delegate_;
  }

  Profile* profile() { return profile_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  content::BrowserTaskEnvironment task_environment;
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  raw_ptr<Profile> profile_ = nullptr;
  signin::IdentityTestEnvironment identity_test_environment;

  GlicProfileManager glic_profile_manager_;
  MockGlicButtonControllerDelegate mock_glic_controller_delegate_;
  std::unique_ptr<MockGlicKeyedService> mock_glic_service_;
  std::unique_ptr<GlicButtonController> glic_button_controller_;
};

// Test that settings changes are reflected in the show state of the controller
// delegate.
TEST_F(GlicButtonControllerTest, GlicSettings) {
  PrefService* prefs = profile()->GetPrefs();

  prefs->SetInteger(
      ::prefs::kGeminiSettings,
      static_cast<int>(glic::prefs::SettingsPolicyState::kEnabled));
  prefs->SetBoolean(glic::prefs::kGlicPinnedToTabstrip, true);
  EXPECT_TRUE(controller_delegate()->show_state());

  prefs->SetInteger(
      ::prefs::kGeminiSettings,
      static_cast<int>(glic::prefs::SettingsPolicyState::kDisabled));
  prefs->SetBoolean(glic::prefs::kGlicPinnedToTabstrip, true);
  EXPECT_FALSE(controller_delegate()->show_state());

  prefs->SetInteger(
      ::prefs::kGeminiSettings,
      static_cast<int>(glic::prefs::SettingsPolicyState::kEnabled));
  prefs->SetBoolean(glic::prefs::kGlicPinnedToTabstrip, false);
  EXPECT_FALSE(controller_delegate()->show_state());

  prefs->SetInteger(
      ::prefs::kGeminiSettings,
      static_cast<int>(glic::prefs::SettingsPolicyState::kDisabled));
  prefs->SetBoolean(glic::prefs::kGlicPinnedToTabstrip, false);
  EXPECT_FALSE(controller_delegate()->show_state());
}

// Test that when the glic window is detached, the button is shown regardless of
// settings state.
TEST_F(GlicButtonControllerTest, GlicDetachedOverridesSettings) {
  PrefService* prefs = profile()->GetPrefs();
  prefs->SetInteger(
      ::prefs::kGeminiSettings,
      static_cast<int>(glic::prefs::SettingsPolicyState::kDisabled));
  prefs->SetBoolean(glic::prefs::kGlicPinnedToTabstrip, false);

  mojom::PanelState panel_state;
  panel_state.kind = mojom::PanelState_Kind::kAttached;
  controller()->PanelStateChanged(panel_state, nullptr);
  ASSERT_FALSE(controller_delegate()->show_state());

  panel_state.kind = mojom::PanelState_Kind::kDetached;
  controller()->PanelStateChanged(panel_state, nullptr);
  EXPECT_TRUE(controller_delegate()->show_state());
}

// Test the panel state of the glic window reflects the icon state
// of the controller delegate.
TEST_F(GlicButtonControllerTest, GlicWindowPanelState) {
  mojom::PanelState panel_state;

  panel_state.kind = mojom::PanelState_Kind::kHidden;
  const auto& hidden_icon =
      GlicVectorIconManager::GetVectorIcon(IDR_GLIC_BUTTON_VECTOR_ICON);
  controller()->PanelStateChanged(panel_state, nullptr);
  EXPECT_EQ(controller_delegate()->icon()->reps.data(),
            hidden_icon.reps.data());

  const auto& attach_icon =
      GlicVectorIconManager::GetVectorIcon(IDR_GLIC_BUTTON_VECTOR_ICON);
  panel_state.kind = mojom::PanelState_Kind::kAttached;
  controller()->PanelStateChanged(panel_state, nullptr);
  EXPECT_EQ(controller_delegate()->icon()->reps.data(),
            attach_icon.reps.data());

  const auto& detach_icon =
      GlicVectorIconManager::GetVectorIcon(IDR_GLIC_ATTACH_BUTTON_VECTOR_ICON);
  panel_state.kind = mojom::PanelState_Kind::kDetached;
  controller()->PanelStateChanged(panel_state, nullptr);
  EXPECT_EQ(controller_delegate()->icon()->reps.data(),
            detach_icon.reps.data());
}

}  // namespace glic
