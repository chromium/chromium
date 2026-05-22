// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/skills/skills_ui_tab_controller.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/skills/public/skill.h"
#include "components/skills/public/skills_metrics.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;

namespace skills {
namespace {

constexpr char kTestSkillId[] = "test_skill_id";

class TestSkillsUiTabController : public SkillsUiTabController {
 public:
  TestSkillsUiTabController(tabs::TabInterface& tab, Profile* profile)
      : SkillsUiTabController(tab), profile_(profile) {}

  raw_ptr<Profile> profile_;
  Skill test_skill_;

  glic::GlicKeyedService* GetGlicService() override {
    return mock_glic_keyed_service_.get();
  }

  void SetMockGlicKeyedService(
      std::unique_ptr<glic::MockGlicKeyedService> mock) {
    mock_glic_keyed_service_ = std::move(mock);
  }

  std::unique_ptr<glic::MockGlicKeyedService> mock_glic_keyed_service_;

  const Skill* GetSkill(std::string_view skill_id) override {
    if (skill_id == kTestSkillId) {
      return &test_skill_;
    }
    return nullptr;
  }
};
}  // namespace

class SkillsUiTabControllerTest : public ChromeViewsTestBase {
 public:
  SkillsUiTabControllerTest() = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    TestingProfileManager* profile_manager =
        TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
            /*profile_manager=*/true);

    EXPECT_CALL(mock_tab_, GetUnownedUserDataHost())
        .WillRepeatedly(ReturnRef(user_data_host_));
    EXPECT_CALL(mock_tab_, GetBrowserWindowInterface())
        .WillRepeatedly(Return(&mock_browser_window_interface_));

    glic::GlicEnabling::SetBypassEnablementChecksForTesting(true);
    TestingProfile* profile =
        profile_manager->CreateTestingProfile("test_profile");
    glic_test_env_.SetupProfile(profile);

    controller_ =
        std::make_unique<TestSkillsUiTabController>(mock_tab_, profile);
    auto mock_service =
        std::make_unique<testing::NiceMock<glic::MockGlicKeyedService>>(
            profile, identity_test_env_.identity_manager(),
            profile_manager->profile_manager(), &glic_profile_manager_, nullptr,
            nullptr);
    controller_->SetMockGlicKeyedService(std::move(mock_service));
  }

  void TearDown() override {
    controller_.reset();
    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
    testing::Mock::VerifyAndClear(&mock_browser_window_interface_);
    testing::Mock::VerifyAndClear(&mock_tab_);

    ChromeViewsTestBase::TearDown();
  }

 protected:
  base::HistogramTester histogram_tester_;
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  glic::GlicProfileManager glic_profile_manager_;
  glic::GlicUnitTestEnvironment glic_test_env_;

  ::ui::UnownedUserDataHost user_data_host_;
  tabs::MockTabInterface mock_tab_;
  NiceMock<MockBrowserWindowInterface> mock_browser_window_interface_;
  signin::IdentityTestEnvironment identity_test_env_;

  std::unique_ptr<TestSkillsUiTabController> controller_;
};

TEST_F(SkillsUiTabControllerTest, InvokeSkill_CallsInvokeWithAutoSubmit) {
  controller_->test_skill_.id = kTestSkillId;
  controller_->test_skill_.prompt = "Test Prompt";

  auto* mock_glic_keyed_service =
      static_cast<glic::MockGlicKeyedService*>(controller_->GetGlicService());
  EXPECT_CALL(*mock_glic_keyed_service,
              InvokeWithAutoSubmit(testing::_, testing::_))
      .WillOnce([](glic::InvokeWithAutoSubmitPasskey,
                   const glic::GlicInvokeOptions& options)
                    -> base::WeakPtr<glic::GlicInstance> {
        EXPECT_EQ(options.skill_id, kTestSkillId);
        EXPECT_EQ(options.prompts.size(), 1u);
        EXPECT_EQ(options.prompts[0], "Test Prompt");
        return base::WeakPtr<glic::GlicInstance>();
      });

  controller_->InvokeSkill(kTestSkillId);
}

TEST_F(SkillsUiTabControllerTest, InvokeSkill_LogsUserCreatedInvokeMetrics) {
  controller_->test_skill_.id = kTestSkillId;
  controller_->test_skill_.source =
      sync_pb::SkillSource::SKILL_SOURCE_USER_CREATED;
  controller_->test_skill_.prompt = "Test Prompt";

  auto* mock_glic_keyed_service =
      static_cast<glic::MockGlicKeyedService*>(controller_->GetGlicService());
  EXPECT_CALL(*mock_glic_keyed_service,
              InvokeWithAutoSubmit(testing::_, testing::_))
      .Times(1);

  controller_->InvokeSkill(kTestSkillId);

  histogram_tester_.ExpectBucketCount("Skills.Invoke.Action",
                                      SkillsInvokeAction::kUserCreated, 1);
  histogram_tester_.ExpectUniqueSample("Skills.Invoke.Result",
                                       SkillsInvokeResult::kSuccess, 1);
}

TEST_F(SkillsUiTabControllerTest, InvokeSkill_LogsFirstPartyInvokeMetrics) {
  controller_->test_skill_.id = kTestSkillId;
  controller_->test_skill_.source =
      sync_pb::SkillSource::SKILL_SOURCE_FIRST_PARTY;
  controller_->test_skill_.prompt = "Test Prompt";

  auto* mock_glic_keyed_service =
      static_cast<glic::MockGlicKeyedService*>(controller_->GetGlicService());
  EXPECT_CALL(*mock_glic_keyed_service,
              InvokeWithAutoSubmit(testing::_, testing::_))
      .Times(1);

  controller_->InvokeSkill(kTestSkillId);

  histogram_tester_.ExpectBucketCount("Skills.Invoke.Action",
                                      SkillsInvokeAction::kFirstParty, 1);
  histogram_tester_.ExpectBucketCount("Skills.Invoke.Action",
                                      SkillsInvokeAction::kUserCreated, 0);
  histogram_tester_.ExpectUniqueSample("Skills.Invoke.Result",
                                       SkillsInvokeResult::kSuccess, 1);
}

TEST_F(SkillsUiTabControllerTest, InvokeSkill_SkillNotFound_LogsMetric) {
  auto* mock_glic_keyed_service =
      static_cast<glic::MockGlicKeyedService*>(controller_->GetGlicService());
  EXPECT_CALL(*mock_glic_keyed_service,
              InvokeWithAutoSubmit(testing::_, testing::_))
      .Times(0);

  controller_->InvokeSkill("some_deleted_skill_id");

  histogram_tester_.ExpectUniqueSample(
      "Skills.Invoke.Result", skills::SkillsInvokeResult::kSkillNotFound, 1);
}
}  // namespace skills
