// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/suggestions/glic_cue_target.h"

#include "chrome/browser/actor/actor_keyed_service_fake.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/optimization_guide/proto/features/contextual_cueing.pb.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/test/glic_user_session_test_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace glic {
namespace {

using ::testing::_;
using ::testing::Exactly;
using ::testing::NiceMock;
using ::testing::Return;

class MockGlicKeyedService : public GlicKeyedService {
 public:
  // TODO(crbug.com/500407600): Possibly deduplicate with other
  // MockGlicKeyedServices.
  MockGlicKeyedService(Profile* profile,
                       signin::IdentityManager* identity_manager,
                       ProfileManager* profile_manager,
                       GlicProfileManager* glic_profile_manager,
                       glic::ContextualCueingService* contextual_cueing_service,
                       actor::ActorKeyedService* actor_keyed_service)
      : GlicKeyedService(profile,
                         identity_manager,
                         profile_manager,
                         glic_profile_manager,
                         contextual_cueing_service,
                         actor_keyed_service) {}
  MOCK_METHOD(bool,
              IsPanelShowingForBrowser,
              (const BrowserWindowInterface&),
              (const, override));
  MOCK_METHOD(void,
              InvokeWithAutoSubmit,
              (InvokeWithAutoSubmitPasskey, GlicInvokeOptions),
              (override));
};

class GlicCueTargetTest : public testing::Test {
 public:
  void SetUp() override {
    GlicEnabling::SetBypassEnablementChecksForTesting(true);
    ON_CALL(mock_browser_window_interface_, GetProfile())
        .WillByDefault(Return(profile_));
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

    mock_glic_keyed_service_ = std::make_unique<MockGlicKeyedService>(
        profile_, identity_test_environment_.identity_manager(),
        testing_profile_manager->profile_manager(), &glic_profile_manager_,
        /*contextual_cueing_service=*/nullptr, actor_keyed_service_.get());
  }

  void TearDown() override {
    mock_glic_keyed_service_.reset();
    actor_keyed_service_.reset();
    profile_ = nullptr;

    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();

#if BUILDFLAG(IS_CHROMEOS)
    glic_user_session_test_helper_.PostProfileTearDown();
#endif  // BUILDFLAG(IS_CHROMEOS)
    GlicEnabling::SetBypassEnablementChecksForTesting(false);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_environment_;
  GlicProfileManager glic_profile_manager_;
  raw_ptr<Profile> profile_ = nullptr;
  NiceMock<MockBrowserWindowInterface> mock_browser_window_interface_;
  std::unique_ptr<actor::ActorKeyedServiceFake> actor_keyed_service_;
  std::unique_ptr<MockGlicKeyedService> mock_glic_keyed_service_;
#if BUILDFLAG(IS_CHROMEOS)
  // glic can run only in User session, so it needs to set up user session
  // manually on ChromeOS.
  ash::GlicUserSessionTestHelper glic_user_session_test_helper_;
#endif  // BUILDFLAG(IS_CHROMEOS)
};

TEST_F(GlicCueTargetTest, IsEligible) {
  GlicCueTarget target(*mock_glic_keyed_service_,
                       mock_browser_window_interface_);

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
}

TEST_F(GlicCueTargetTest, OnClick) {
  GlicCueTarget target(*mock_glic_keyed_service_,
                       mock_browser_window_interface_);

  contextual_cueing::GlicCueActionData glic_data;
  glic_data.prompt = "test prompt";
  contextual_cueing::CueActionData data = glic_data;

  EXPECT_CALL(*mock_glic_keyed_service_, InvokeWithAutoSubmit(_, _))
      .WillOnce([](InvokeWithAutoSubmitPasskey, GlicInvokeOptions options) {
        EXPECT_TRUE(std::holds_alternative<raw_ptr<tabs::TabInterface>>(
            options.target.surface));
        EXPECT_EQ(1u, options.prompts.size());
        EXPECT_EQ("test prompt", options.prompts[0]);
        EXPECT_EQ(glic::mojom::InvocationSource::kAutoOpenedByContextualCue,
                  options.invocation_source);
        EXPECT_TRUE(std::holds_alternative<glic::NewConversation>(
            options.target.conversation));
      });

  target.OnClick(data);
}

TEST_F(GlicCueTargetTest, GetIcon) {
  GlicCueTarget target(*mock_glic_keyed_service_,
                       mock_browser_window_interface_);
  if (ui::ResourceBundle::HasSharedInstance()) {
    EXPECT_FALSE(target.GetAnchoredMessageIcon().IsEmpty());
    EXPECT_FALSE(target.GetOmniboxChipIcon().IsEmpty());
  }
}

TEST_F(GlicCueTargetTest, CueActionDataFromResponse) {
  GlicCueTarget target(*mock_glic_keyed_service_,
                       mock_browser_window_interface_);

  optimization_guide::proto::ContextualCueingResponse response;
  auto* surface = response.mutable_gemini_in_chrome_surface();
  surface->set_prompt("response prompt");
  surface->add_tabs_to_share()->set_tab_id(1234l);
  surface->add_tabs_to_share()->set_tab_id(5678l);

  contextual_cueing::CueActionData data =
      target.CueActionDataFromResponse(response);
  ASSERT_TRUE(
      std::holds_alternative<contextual_cueing::GlicCueActionData>(data));
  auto& glic_data = std::get<contextual_cueing::GlicCueActionData>(data);
  EXPECT_EQ("response prompt", glic_data.prompt);
  EXPECT_EQ(2ul, glic_data.tabs_to_share.size());
  EXPECT_EQ(1234, glic_data.tabs_to_share[0].raw_value());
  EXPECT_EQ(5678, glic_data.tabs_to_share[1].raw_value());
}

}  // namespace
}  // namespace glic
