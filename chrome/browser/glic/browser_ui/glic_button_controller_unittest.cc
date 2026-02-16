// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_button_controller.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service_fake.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"
#include "chrome/browser/glic/browser_ui/glic_button_controller_delegate.h"
#include "chrome/browser/glic/browser_ui/glic_vector_icon_manager.h"
#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/test_support/mock_glic_window_controller.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/ui_features.h"
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

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/test/glic_user_session_test_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace glic {
namespace {

class TestingGlicWindowController : public glic::MockGlicWindowController {
 public:
  base::CallbackListSubscription AddGlobalShowHideCallback(
      base::RepeatingClosure callback) override {
    return global_show_hide_subscriptions_.Add(std::move(callback));
  }

  void NotifyShowHide() { global_show_hide_subscriptions_.Notify(); }

 private:
  base::RepeatingClosureList global_show_hide_subscriptions_;
};

class TestingGlicFreController : public glic::GlicFreController {
 public:
  TestingGlicFreController(Profile* profile,
                           signin::IdentityManager* identity_manager)
      : GlicFreController(profile, identity_manager) {}

  base::CallbackListSubscription AddWebUiStateChangedCallback(
      WebUiStateChangedCallback callback) override {
    return webui_state_callback_list_.Add(std::move(callback));
  }

  void NotifyStateChanged() {
    webui_state_callback_list_.Notify(mojom::FreWebUiState::kReady);
  }

  base::RepeatingCallbackList<void(mojom::FreWebUiState)>
      webui_state_callback_list_;
};

class MockGlicKeyedService : public glic::GlicKeyedService {
 public:
  MockGlicKeyedService(
      content::BrowserContext* browser_context,
      signin::IdentityManager* identity_manager,
      ProfileManager* profile_manager,
      GlicProfileManager* glic_profile_manager,
      contextual_cueing::ContextualCueingService* contextual_cueing_service,
      actor::ActorKeyedService* actor_keyed_service)
      : GlicKeyedService(Profile::FromBrowserContext(browser_context),
                         identity_manager,
                         profile_manager,
                         glic_profile_manager,
                         contextual_cueing_service,
                         actor_keyed_service),
        window_controller_(std::make_unique<TestingGlicWindowController>()),
        fre_controller_(std::make_unique<TestingGlicFreController>(
            Profile::FromBrowserContext(browser_context),
            identity_manager)) {}
  MOCK_METHOD(void, TryPreload, (), (override));

  bool IsPanelShowingForBrowser(
      const BrowserWindowInterface& bwi) const override {
    return browser_with_open_panel_ == &bwi;
  }

  bool IsFreShowing() const override { return fre_open_; }

  GlicWindowController& window_controller() const override {
    return *window_controller_;
  }

  GlicFreController& fre_controller() override { return *fre_controller_; }

  // For these tests, pretend only one browser can show the panel at a time.
  void SimulatePanelShownForBrowser(BrowserWindowInterface* bwi) {
    browser_with_open_panel_ = bwi;
    window_controller_->NotifyShowHide();
  }

  void SimulateFREShown(bool open) {
    fre_open_ = open;
    fre_controller_->NotifyStateChanged();
  }

 private:
  raw_ptr<BrowserWindowInterface> browser_with_open_panel_ = nullptr;
  bool fre_open_ = false;
  std::unique_ptr<TestingGlicWindowController> window_controller_;
  std::unique_ptr<TestingGlicFreController> fre_controller_;
};

class MockGlicButtonControllerDelegate
    : public glic::GlicButtonControllerDelegate {
 public:
  void SetGlicShowState(bool show) override { show_state_ = show; }
  void SetGlicPanelIsOpen(bool panel_open) override {
    panel_open_ = panel_open;
  }

  bool show_state() const { return show_state_; }
  bool panel_open() const { return panel_open_; }

 private:
  bool show_state_ = false;
  bool panel_open_ = false;
};

}  // namespace

class GlicButtonControllerTest : public testing::Test {
 public:
  void SetUp() override {
    // Enable kGlic by default for testing.
    scoped_feature_list_.InitWithFeatures(
        {
            features::kGlic,
            features::kGlicRollout,
        },
        {});

    raw_ptr<TestingProfileManager> testing_profile_manager =
        TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
            /*profile_manager=*/true);

#if BUILDFLAG(IS_CHROMEOS)
    glic_user_session_test_helper_.PreProfileSetUp(
        testing_profile_manager->profile_manager());
#endif  // BUILDFLAG(IS_CHROMEOS)

    profile_ = testing_profile_manager->CreateTestingProfile("profile");

    actor_keyed_service_ =
        std::make_unique<actor::ActorKeyedServiceFake>(profile_);

    mock_glic_service_ = std::make_unique<MockGlicKeyedService>(
        profile_, identity_test_environment.identity_manager(),
        testing_profile_manager->profile_manager(), &glic_profile_manager_,
        /*contextual_cueing_service=*/nullptr, actor_keyed_service_.get());

    mock_browser_window_interface_ =
        std::make_unique<MockBrowserWindowInterface>();

    glic_button_controller_ = std::make_unique<GlicButtonController>(
        profile_, *mock_browser_window_interface_,
        &mock_tab_strip_glic_controller_delegate_,
        &mock_toolbar_glic_controller_delegate_, mock_glic_service_.get());

    glic_test_env_.SetupProfile(profile());

    PrefService* prefs = profile()->GetPrefs();
    prefs->SetInteger(
        ::prefs::kGeminiSettings,
        static_cast<int>(glic::prefs::SettingsPolicyState::kEnabled));
    prefs->SetBoolean(glic::prefs::kGlicPinnedToTabstrip, true);
  }

  void TearDown() override {
    glic_button_controller_.reset();
    mock_browser_window_interface_.reset();

    mock_glic_service_.reset();
    actor_keyed_service_.reset();
    profile_ = nullptr;

    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();

#if BUILDFLAG(IS_CHROMEOS)
    glic_user_session_test_helper_.PostProfileTearDown();
#endif  // BUILDFLAG(IS_CHROMEOS)
    scoped_feature_list_.Reset();
  }

  GlicButtonController* controller() { return glic_button_controller_.get(); }

  MockGlicButtonControllerDelegate* tab_strip_controller_delegate() {
    return &mock_tab_strip_glic_controller_delegate_;
  }

  MockGlicButtonControllerDelegate* toolbar_controller_delegate() {
    return &mock_toolbar_glic_controller_delegate_;
  }

  Profile* profile() { return profile_; }
  MockGlicKeyedService* glic_keyed_service() {
    return mock_glic_service_.get();
  }
  BrowserWindowInterface* browser_window_interface() {
    return mock_browser_window_interface_.get();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  GlicUnitTestEnvironment glic_test_env_;
  content::BrowserTaskEnvironment task_environment;

#if BUILDFLAG(IS_CHROMEOS)
  // glic can run only in User session, so it needs to set up user session
  // manually on ChromeOS.
  ash::GlicUserSessionTestHelper glic_user_session_test_helper_;
#endif  // BUILDFLAG(IS_CHROMEOS)
  raw_ptr<Profile> profile_ = nullptr;
  signin::IdentityTestEnvironment identity_test_environment;

  GlicProfileManager glic_profile_manager_;
  MockGlicButtonControllerDelegate mock_tab_strip_glic_controller_delegate_;
  MockGlicButtonControllerDelegate mock_toolbar_glic_controller_delegate_;
  std::unique_ptr<actor::ActorKeyedServiceFake> actor_keyed_service_;
  std::unique_ptr<MockGlicKeyedService> mock_glic_service_;
  std::unique_ptr<GlicButtonController> glic_button_controller_;
  std::unique_ptr<MockBrowserWindowInterface> mock_browser_window_interface_;
};

// Test that settings changes are reflected in the show state of the controller
// delegate.
TEST_F(GlicButtonControllerTest, GlicSettings) {
  PrefService* prefs = profile()->GetPrefs();

  prefs->SetInteger(
      ::prefs::kGeminiSettings,
      static_cast<int>(glic::prefs::SettingsPolicyState::kEnabled));
  prefs->SetBoolean(glic::prefs::kGlicPinnedToTabstrip, true);
  EXPECT_TRUE(tab_strip_controller_delegate()->show_state());
  EXPECT_TRUE(toolbar_controller_delegate()->show_state());

  prefs->SetInteger(
      ::prefs::kGeminiSettings,
      static_cast<int>(glic::prefs::SettingsPolicyState::kDisabled));
  prefs->SetBoolean(glic::prefs::kGlicPinnedToTabstrip, true);
  EXPECT_FALSE(tab_strip_controller_delegate()->show_state());
  EXPECT_FALSE(toolbar_controller_delegate()->show_state());

  prefs->SetInteger(
      ::prefs::kGeminiSettings,
      static_cast<int>(glic::prefs::SettingsPolicyState::kEnabled));
  prefs->SetBoolean(glic::prefs::kGlicPinnedToTabstrip, false);
  EXPECT_FALSE(tab_strip_controller_delegate()->show_state());
  EXPECT_FALSE(toolbar_controller_delegate()->show_state());

  prefs->SetInteger(
      ::prefs::kGeminiSettings,
      static_cast<int>(glic::prefs::SettingsPolicyState::kDisabled));
  prefs->SetBoolean(glic::prefs::kGlicPinnedToTabstrip, false);
  EXPECT_FALSE(tab_strip_controller_delegate()->show_state());
  EXPECT_FALSE(toolbar_controller_delegate()->show_state());
}

TEST_F(GlicButtonControllerTest, PanelStateChangedSameBrowser) {
  EXPECT_TRUE(tab_strip_controller_delegate()->show_state());
  EXPECT_TRUE(toolbar_controller_delegate()->show_state());
  EXPECT_FALSE(tab_strip_controller_delegate()->panel_open());
  EXPECT_FALSE(toolbar_controller_delegate()->panel_open());

  glic_keyed_service()->SimulatePanelShownForBrowser(
      browser_window_interface());
  EXPECT_TRUE(tab_strip_controller_delegate()->panel_open());
  EXPECT_TRUE(toolbar_controller_delegate()->panel_open());

  glic_keyed_service()->SimulatePanelShownForBrowser(nullptr);
  EXPECT_FALSE(tab_strip_controller_delegate()->panel_open());
  EXPECT_FALSE(toolbar_controller_delegate()->panel_open());
}

TEST_F(GlicButtonControllerTest, FREStateChanged) {
  EXPECT_TRUE(tab_strip_controller_delegate()->show_state());
  EXPECT_TRUE(toolbar_controller_delegate()->show_state());
  EXPECT_FALSE(tab_strip_controller_delegate()->panel_open());
  EXPECT_FALSE(toolbar_controller_delegate()->panel_open());

  glic_keyed_service()->SimulateFREShown(true);
  EXPECT_TRUE(tab_strip_controller_delegate()->panel_open());
  EXPECT_TRUE(toolbar_controller_delegate()->panel_open());

  glic_keyed_service()->SimulateFREShown(false);
  EXPECT_FALSE(tab_strip_controller_delegate()->panel_open());
  EXPECT_FALSE(toolbar_controller_delegate()->panel_open());
}

}  // namespace glic
