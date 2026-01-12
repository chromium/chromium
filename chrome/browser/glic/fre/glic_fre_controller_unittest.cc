// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/fre/glic_fre_controller.h"

#include <memory>

#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/version_info/channel.h"
#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
class GlicFreControllerTest : public testing::Test {
 public:
  GlicFreControllerTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlic, features::kTabstripComboButton},
        /*disabled_features=*/{});
  }

  void SetUp() override {
    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(testing_profile_manager_->SetUp());
    TestingBrowserProcess::GetGlobal()->CreateGlobalFeaturesForTesting();
    identity_env_ = std::make_unique<signin::IdentityTestEnvironment>();
    profile_ = testing_profile_manager_->CreateTestingProfile("profile");

    glic_fre_controller_ = std::make_unique<GlicFreController>(
        profile_, identity_env_->identity_manager());
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->GetFeatures()->Shutdown();
  }

  Profile* profile() { return profile_; }

  GlicFreController& glic_fre_controller() {
    CHECK(glic_fre_controller_);
    return *glic_fre_controller_;
  }

  static void OnCheckIsDefaultBrowserFinished(
      version_info::Channel channel,
      shell_integration::DefaultWebClientState state) {
    GlicLauncherConfiguration::OnCheckIsDefaultBrowserFinished(channel, state);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_env_;
  std::unique_ptr<GlicFreController> glic_fre_controller_;
  raw_ptr<Profile> profile_ = nullptr;
};

TEST_F(GlicFreControllerTest, AcceptFre) {
  base::UserActionTester tester;
  // TODO: Without this line, there's a sequence check error in
  // shell_integration::DefaultWebClientWorker::OnCheckIsDefaultComplete.
  // Likely a problem with the test environment configuration.
  g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                               false);
  PrefService* const profile_pref_service = profile()->GetPrefs();
  glic_fre_controller().AcceptFre(/*handler=*/nullptr);
  EXPECT_EQ(profile_pref_service->GetInteger(prefs::kGlicCompletedFre),
            static_cast<int>(prefs::FreStatus::kCompleted));
  EXPECT_EQ(tester.GetActionCount("Glic.Fre.Accept"), 1);
  EXPECT_EQ(tester.GetActionCount("Glic.Fre.NoThanks"), 0);
}

TEST_F(GlicFreControllerTest, RejectFre) {
  base::UserActionTester tester;
  g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                               false);
  PrefService* const profile_pref_service = profile()->GetPrefs();
  glic_fre_controller().RejectFre();
  EXPECT_EQ(profile_pref_service->GetInteger(prefs::kGlicCompletedFre),
            static_cast<int>(prefs::FreStatus::kNotStarted));
  EXPECT_EQ(tester.GetActionCount("Glic.Fre.NoThanks"), 1);
  EXPECT_EQ(tester.GetActionCount("Glic.Fre.Accept"), 0);
}

TEST_F(GlicFreControllerTest, DismissUninitialized) {
  base::UserActionTester tester;
  glic_fre_controller().SetIsShowingDialogForTesting(true);
  glic_fre_controller().DismissFre(mojom::FreWebUiState::kUninitialized);
  // The FRE can be dismissed for many reasons that are not direct user actions.
  // When it is dismissed by a non-user action it is called with Panel type
  // kUninitialized.
  EXPECT_EQ(tester.GetActionCount("Glic.Fre.Accept"), 0);
  EXPECT_EQ(tester.GetActionCount("Glic.Fre.UninitializedPanelClosed"), 1);
}

TEST_F(GlicFreControllerTest, DismissReady) {
  base::UserActionTester tester;
  glic_fre_controller().SetIsShowingDialogForTesting(true);
  glic_fre_controller().DismissFre(mojom::FreWebUiState::kReady);
  EXPECT_EQ(tester.GetActionCount("Glic.Fre.Accept"), 0);
  EXPECT_EQ(tester.GetActionCount("Glic.Fre.ReadyPanelClosed"), 1);
}

TEST_F(GlicFreControllerTest, DismissOffline) {
  base::UserActionTester tester;
  glic_fre_controller().SetIsShowingDialogForTesting(true);
  glic_fre_controller().DismissFre(mojom::FreWebUiState::kOffline);
  EXPECT_EQ(tester.GetActionCount("Glic.Fre.Accept"), 0);
  EXPECT_EQ(tester.GetActionCount("Glic.Fre.OfflinePanelClosed"), 1);
}

TEST_F(GlicFreControllerTest, DismissLoading) {
  base::UserActionTester tester;
  glic_fre_controller().SetIsShowingDialogForTesting(true);
  glic_fre_controller().DismissFre(mojom::FreWebUiState::kShowLoading);
  EXPECT_EQ(tester.GetActionCount("Glic.Fre.Accept"), 0);
  EXPECT_EQ(tester.GetActionCount("Glic.Fre.LoadingPanelClosed"), 1);
}

TEST_F(GlicFreControllerTest, DismissError) {
  base::UserActionTester tester;
  glic_fre_controller().SetIsShowingDialogForTesting(true);
  glic_fre_controller().DismissFre(mojom::FreWebUiState::kError);
  EXPECT_EQ(tester.GetActionCount("Glic.Fre.Accept"), 0);
  EXPECT_EQ(tester.GetActionCount("Glic.Fre.ErrorPanelClosed"), 1);
}

TEST_F(GlicFreControllerTest, UpdateLauncherOnFreCompletion) {
  PrefService* const pref_service = g_browser_process->local_state();
  ASSERT_FALSE(GlicLauncherConfiguration::IsEnabled());

  // The launcher should remain disabled because another channel is the default
  // browser.
  GlicFreControllerTest::OnCheckIsDefaultBrowserFinished(
      version_info::Channel::STABLE,
      shell_integration::DefaultWebClientState::OTHER_MODE_IS_DEFAULT);
  EXPECT_FALSE(GlicLauncherConfiguration::IsEnabled());

  // The launcher should be enabled even though the channel is not known because
  // the browser is the default.
  GlicFreControllerTest::OnCheckIsDefaultBrowserFinished(
      version_info::Channel::UNKNOWN,
      shell_integration::DefaultWebClientState::IS_DEFAULT);
  EXPECT_TRUE(GlicLauncherConfiguration::IsEnabled());

  // Browser is not on the stable channel and is not the default so the
  // launcher should not be enabled.
  pref_service->SetBoolean(prefs::kGlicLauncherEnabled, false);
  GlicFreControllerTest::OnCheckIsDefaultBrowserFinished(
      version_info::Channel::UNKNOWN,
      shell_integration::DefaultWebClientState::NOT_DEFAULT);
  EXPECT_FALSE(GlicLauncherConfiguration::IsEnabled());

  // Since another browser is the default, a stable channel should allow the
  // launcher to be enabled.
  GlicFreControllerTest::OnCheckIsDefaultBrowserFinished(
      version_info::Channel::STABLE,
      shell_integration::DefaultWebClientState::NOT_DEFAULT);
  EXPECT_TRUE(GlicLauncherConfiguration::IsEnabled());
}

TEST_F(GlicFreControllerTest, OpenLink) {
  base::UserActionTester tester;
  glic_fre_controller().OnLinkClicked(
      GURL("https://support.google.com/clic/answer/13594961"));
  EXPECT_EQ(tester.GetActionCount("Glic.Fre.PrivacyNoticeLinkOpened"), 1);

  glic_fre_controller().OnLinkClicked(
      GURL("https://support.google.com/glic/answer/123456"));
  EXPECT_EQ(tester.GetActionCount("Glic.Fre.HelpCenterLinkOpened"), 1);

  glic_fre_controller().OnLinkClicked(
      GURL("https://policies.google.com/terms"));
  EXPECT_EQ(tester.GetActionCount("Glic.Fre.PolicyLinkOpened"), 1);

  glic_fre_controller().OnLinkClicked(
      GURL("https://myactivity.google.com/product/glic"));
  EXPECT_EQ(tester.GetActionCount("Glic.Fre.MyActivityLinkOpened"), 1);
}

}  // namespace glic
