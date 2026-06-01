// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/suggestions/glic_cue_target.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_keyed_service_fake.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_metrics.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/session_tab_helper_factory.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/optimization_guide/proto/features/contextual_cueing.pb.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/tabs/public/tab_handle_factory.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/test/glic_user_session_test_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace glic {
namespace {

using ::testing::_;
using ::testing::Exactly;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

class GlicCueTargetTest : public testing::Test {
 public:
  void SetUp() override {
    GlicEnabling::SetBypassEnablementChecksForTesting(true);
    raw_ptr<TestingProfileManager> testing_profile_manager =
        TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
            /*profile_manager=*/true);
#if BUILDFLAG(IS_CHROMEOS)
    glic_user_session_test_helper_.PreProfileSetUp(
        testing_profile_manager->profile_manager());
#endif  // BUILDFLAG(IS_CHROMEOS)

    profile_ = testing_profile_manager->CreateTestingProfile("profile");
    mock_browser_window_interface_ =
        std::make_unique<NiceMock<MockBrowserWindowInterface>>();
    ON_CALL(*mock_browser_window_interface_, GetProfile())
        .WillByDefault(Return(profile_));
    ON_CALL(*mock_browser_window_interface_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(user_data_host_));
    actor_keyed_service_ =
        std::make_unique<actor::ActorKeyedServiceFake>(profile_);

    mock_glic_keyed_service_ = std::make_unique<MockGlicKeyedService>(
        profile_, identity_test_environment_.identity_manager(),
        testing_profile_manager->profile_manager(), &glic_profile_manager_,
        /*contextual_cueing_service=*/nullptr, actor_keyed_service_.get());

    tab_strip_model_delegate_ = std::make_unique<TestTabStripModelDelegate>();
    tab_strip_model_delegate_->SetBrowserWindowInterface(
        mock_browser_window_interface_.get());
    tab_strip_model_ = std::make_unique<TabStripModel>(
        tab_strip_model_delegate_.get(), profile_);
  }

  void TearDown() override {
    tab_strip_model_.reset();
    tab_strip_model_delegate_.reset();
    mock_glic_keyed_service_.reset();
    actor_keyed_service_.reset();
    mock_browser_window_interface_.reset();
    profile_ = nullptr;

    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();

#if BUILDFLAG(IS_CHROMEOS)
    glic_user_session_test_helper_.PostProfileTearDown();
#endif  // BUILDFLAG(IS_CHROMEOS)
    GlicEnabling::SetBypassEnablementChecksForTesting(false);
  }

 protected:
  SessionID CreateTab() {
    tab_strip_model_->AppendTab(
        std::make_unique<tabs::TabModel>(
            content::WebContentsTester::CreateTestWebContents(profile_,
                                                              nullptr),
            tab_strip_model_.get()),
        /*foreground=*/true);
    auto* tab_model =
        static_cast<tabs::TabModel*>(tab_strip_model_->GetTabAtIndex(0));
    return sessions::SessionTabHelper::FromWebContents(tab_model->GetContents())
        ->session_id();
  }

  tabs::TabHandle GetTabHandle(SessionID id) {
    return tabs::TabHandle(tabs::SessionMappedTabHandleFactory::GetInstance()
                               .GetHandleForSessionId(id.id()));
  }

  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  signin::IdentityTestEnvironment identity_test_environment_;
  GlicProfileManager glic_profile_manager_;
  raw_ptr<Profile> profile_ = nullptr;
  ui::UnownedUserDataHost user_data_host_;
  std::unique_ptr<NiceMock<MockBrowserWindowInterface>>
      mock_browser_window_interface_;
  std::unique_ptr<actor::ActorKeyedServiceFake> actor_keyed_service_;
  std::unique_ptr<MockGlicKeyedService> mock_glic_keyed_service_;
  std::unique_ptr<TestTabStripModelDelegate> tab_strip_model_delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  const tabs::TabModel::PreventFeatureInitializationForTesting prevent_;
#if BUILDFLAG(IS_CHROMEOS)
  // glic can run only in User session, so it needs to set up user session
  // manually on ChromeOS.
  ash::GlicUserSessionTestHelper glic_user_session_test_helper_;
#endif  // BUILDFLAG(IS_CHROMEOS)
};

TEST_F(GlicCueTargetTest, IsEligible) {
  GlicCueTarget target(*mock_glic_keyed_service_,
                       /*optimization_guide_keyed_service=*/nullptr,
                       *mock_browser_window_interface_);

  // Ineligible profile, don't cue regardless of panel state
  GlicEnabling::SetBypassEnablementChecksForTesting(false);
  EXPECT_CALL(*mock_glic_keyed_service_, IsPanelShowingForBrowser(_))
      .Times(Exactly(0));
  EXPECT_FALSE(target.IsEligible());

  // Eligible profile but the panel is showing
  GlicEnabling::SetBypassEnablementChecksForTesting(true);
  EXPECT_CALL(*mock_glic_keyed_service_, IsPanelShowingForBrowser(_))
      .WillOnce(Return(true));
  EXPECT_FALSE(target.IsEligible());

  // Eligible profile and the panel isn't showing, so we are eligible to cue
  EXPECT_CALL(*mock_glic_keyed_service_, IsPanelShowingForBrowser(_))
      .WillOnce(Return(false));
  EXPECT_TRUE(target.IsEligible());

  // Eligible profile, panel isn't showing, but "Show Gemini at the top of the
  // browser" is explicitly turned off.
  profile_->GetPrefs()->SetBoolean(prefs::kGlicPinnedToTabstrip, false);
  EXPECT_CALL(*mock_glic_keyed_service_, IsPanelShowingForBrowser(_))
      .Times(Exactly(0));
  EXPECT_FALSE(target.IsEligible());
}

TEST_F(GlicCueTargetTest, OnClick_AutoSubmitEnabled) {
  base::test::ScopedFeatureList features(
      features::kGlicContextualCueingV2AutoSubmit);

  GlicCueTarget target(*mock_glic_keyed_service_,
                       /*optimization_guide_keyed_service=*/nullptr,
                       *mock_browser_window_interface_);

  contextual_cueing::GlicCueActionData glic_data;
  glic_data.prompt = "test prompt";
  glic_data.tabs_to_share.emplace_back(123);
  glic_data.tabs_to_share.emplace_back(456);

  contextual_cueing::CueActionData data = glic_data;

  EXPECT_CALL(*mock_glic_keyed_service_, InvokeWithAutoSubmit(_, _))
      .WillOnce([](InvokeWithAutoSubmitPasskey, GlicInvokeOptions options)
                    -> base::WeakPtr<glic::GlicInstance> {
        EXPECT_TRUE(std::holds_alternative<raw_ptr<tabs::TabInterface>>(
            options.target.surface));
        EXPECT_EQ(1u, options.prompts.size());
        EXPECT_EQ("test prompt", options.prompts[0]);
        EXPECT_EQ(glic::mojom::InvocationSource::kAutoOpenedByContextualCue,
                  options.GetInvocationSource());
        EXPECT_TRUE(std::holds_alternative<glic::NewConversation>(
            options.target.conversation));
        EXPECT_EQ(2ul, options.tab_sharing.tabs_to_pin.size());
        EXPECT_EQ(123, options.tab_sharing.tabs_to_pin[0].raw_value());
        EXPECT_EQ(456, options.tab_sharing.tabs_to_pin[1].raw_value());
        EXPECT_EQ(GlicPinTrigger::kContextualCue,
                  options.tab_sharing.pin_trigger);
        return base::WeakPtr<glic::GlicInstance>();
      });

  target.OnClick(data);
}

TEST_F(GlicCueTargetTest, OnClick_AutoSubmitDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kGlicContextualCueingV2AutoSubmit);

  GlicCueTarget target(*mock_glic_keyed_service_,
                       /*optimization_guide_keyed_service=*/nullptr,
                       *mock_browser_window_interface_);

  contextual_cueing::GlicCueActionData glic_data;
  glic_data.prompt = "test prompt";
  glic_data.tabs_to_share.emplace_back(123);
  glic_data.tabs_to_share.emplace_back(456);

  contextual_cueing::CueActionData data = glic_data;

  EXPECT_CALL(*mock_glic_keyed_service_, Invoke(_))
      .WillOnce([](GlicInvokeOptions options)
                    -> base::WeakPtr<glic::GlicInstance> {
        EXPECT_TRUE(std::holds_alternative<raw_ptr<tabs::TabInterface>>(
            options.target.surface));
        EXPECT_EQ(1u, options.prompts.size());
        EXPECT_EQ("test prompt", options.prompts[0]);
        EXPECT_EQ(glic::mojom::InvocationSource::kAutoOpenedByContextualCue,
                  options.GetInvocationSource());
        EXPECT_TRUE(std::holds_alternative<glic::NewConversation>(
            options.target.conversation));
        EXPECT_EQ(2ul, options.tab_sharing.tabs_to_pin.size());
        EXPECT_EQ(123, options.tab_sharing.tabs_to_pin[0].raw_value());
        EXPECT_EQ(456, options.tab_sharing.tabs_to_pin[1].raw_value());
        EXPECT_EQ(GlicPinTrigger::kContextualCue,
                  options.tab_sharing.pin_trigger);
        return base::WeakPtr<glic::GlicInstance>();
      });

  target.OnClick(data);
}

TEST_F(GlicCueTargetTest, OnEditPrompt) {
  GlicCueTarget target(*mock_glic_keyed_service_,
                       /*optimization_guide_keyed_service=*/nullptr,
                       *mock_browser_window_interface_);

  contextual_cueing::GlicCueActionData glic_data;
  glic_data.prompt = "test prompt";
  glic_data.tabs_to_share.emplace_back(123);
  glic_data.tabs_to_share.emplace_back(456);

  contextual_cueing::CueActionData data = glic_data;

  EXPECT_CALL(*mock_glic_keyed_service_, Invoke(_))
      .WillOnce(
          [](GlicInvokeOptions options) -> base::WeakPtr<glic::GlicInstance> {
            EXPECT_TRUE(std::holds_alternative<raw_ptr<tabs::TabInterface>>(
                options.target.surface));
            EXPECT_EQ(1u, options.prompts.size());
            EXPECT_EQ("test prompt", options.prompts[0]);
            EXPECT_EQ(glic::mojom::InvocationSource::kAutoOpenedByContextualCue,
                      options.GetInvocationSource());
            EXPECT_TRUE(std::holds_alternative<glic::NewConversation>(
                options.target.conversation));
            EXPECT_EQ(2ul, options.tab_sharing.tabs_to_pin.size());
            EXPECT_EQ(123, options.tab_sharing.tabs_to_pin[0].raw_value());
            EXPECT_EQ(456, options.tab_sharing.tabs_to_pin[1].raw_value());
            EXPECT_EQ(GlicPinTrigger::kContextualCue,
                      options.tab_sharing.pin_trigger);

            return base::WeakPtr<glic::GlicInstance>();
          });

  target.OnEditPrompt(data);
}

TEST_F(GlicCueTargetTest, GetIcon) {
  GlicCueTarget target(*mock_glic_keyed_service_,
                       /*optimization_guide_keyed_service=*/nullptr,
                       *mock_browser_window_interface_);
  if (ui::ResourceBundle::HasSharedInstance()) {
    EXPECT_FALSE(target.GetAnchoredMessageIcon().IsEmpty());
    EXPECT_FALSE(target.GetOmniboxChipIcon().IsEmpty());
  }
}

TEST_F(GlicCueTargetTest, CueActionDataFromResponse) {
#if BUILDFLAG(IS_CHROMEOS)
  GTEST_SKIP() << "crbug.com/41100311: Disabled on ChromeOS until profile "
                  "loading is fixed for this test.";
#else
  GlicCueTarget target(*mock_glic_keyed_service_,
                       /*optimization_guide_keyed_service=*/nullptr,
                       *mock_browser_window_interface_);

  optimization_guide::proto::ContextualCue cue;
  auto* surface = cue.mutable_gemini_in_chrome_surface();
  surface->set_prompt("response prompt");

  contextual_cueing::CueActionData data =
      target.CueActionDataFromResponse(cue, {});
  ASSERT_TRUE(
      std::holds_alternative<contextual_cueing::GlicCueActionData>(data));
  auto& glic_data = std::get<contextual_cueing::GlicCueActionData>(data);
  EXPECT_EQ("response prompt", glic_data.prompt);

#endif
}

}  // namespace
}  // namespace glic
