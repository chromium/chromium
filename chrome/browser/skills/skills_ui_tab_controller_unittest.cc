// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/skills/skills_ui_tab_controller.h"

#include <memory>
#include <string>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chrome/test/views/chrome_views_test_base.h"
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
  explicit TestSkillsUiTabController(tabs::TabInterface& tab)
      : SkillsUiTabController(tab) {}

  Skill test_skill_;

  MOCK_METHOD(void, ShowGlicPanel, (), (override));
  MOCK_METHOD(bool, IsClientReady, (), (override));
  MOCK_METHOD(void, NotifySkillToInvokeChanged, (), (override));

  void CallRealNotifySkillToInvokeChanged() {
    SkillsUiTabController::NotifySkillToInvokeChanged();
  }

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

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    EXPECT_CALL(mock_tab_, GetUnownedUserDataHost())
        .WillRepeatedly(ReturnRef(user_data_host_));
    EXPECT_CALL(mock_tab_, GetBrowserWindowInterface())
        .WillRepeatedly(Return(&mock_browser_window_interface_));

    controller_ = std::make_unique<TestSkillsUiTabController>(mock_tab_);
  }

  void TearDown() override {
    controller_.reset();

    testing::Mock::VerifyAndClear(&mock_browser_window_interface_);
    testing::Mock::VerifyAndClear(&mock_tab_);

    profile_manager_.reset();

    ChromeViewsTestBase::TearDown();
  }

 protected:
  base::HistogramTester histogram_tester_;
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  std::unique_ptr<TestingProfileManager> profile_manager_;

  ::ui::UnownedUserDataHost user_data_host_;
  tabs::MockTabInterface mock_tab_;
  NiceMock<MockBrowserWindowInterface> mock_browser_window_interface_;

  std::unique_ptr<TestSkillsUiTabController> controller_;
};

TEST_F(SkillsUiTabControllerTest, InvokeSkill_TriggersToggleUI) {
  EXPECT_CALL(*controller_, ShowGlicPanel()).Times(1);
  EXPECT_CALL(*controller_, IsClientReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(*controller_, NotifySkillToInvokeChanged()).Times(1);

  controller_->InvokeSkill(kTestSkillId);
}

TEST_F(SkillsUiTabControllerTest, InvokeSkill_ClientNotReady_Waits) {
  EXPECT_CALL(*controller_, ShowGlicPanel()).Times(1);
  EXPECT_CALL(*controller_, IsClientReady()).WillRepeatedly(Return(false));
  EXPECT_CALL(*controller_, NotifySkillToInvokeChanged()).Times(0);

  controller_->InvokeSkill(kTestSkillId);
}

TEST_F(SkillsUiTabControllerTest,
       InvokeSkill_ClientBecomesReady_SendsNotification) {
  EXPECT_CALL(*controller_, ShowGlicPanel()).Times(1);
  EXPECT_CALL(*controller_, IsClientReady())
      .WillOnce(Return(false))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*controller_, NotifySkillToInvokeChanged()).Times(1);

  controller_->InvokeSkill(kTestSkillId);

  task_environment()->FastForwardBy(base::Milliseconds(100));
}

TEST_F(SkillsUiTabControllerTest, InvokeSkill_Timeout_GivesUp) {
  EXPECT_CALL(*controller_, ShowGlicPanel()).Times(1);
  EXPECT_CALL(*controller_, IsClientReady()).WillRepeatedly(Return(false));
  EXPECT_CALL(*controller_, NotifySkillToInvokeChanged()).Times(0);

  controller_->InvokeSkill(kTestSkillId);

  task_environment()->FastForwardBy(base::Seconds(61));
  histogram_tester_.ExpectUniqueSample("Skills.Invoke.Result",
                                       skills::SkillsInvokeResult::kTimeout, 1);
}

TEST_F(SkillsUiTabControllerTest, InvokeSkill_LogsUserCreatedInvokeMetrics) {
  TestingProfile* profile =
      profile_manager_->CreateTestingProfile("test_profile");
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile, nullptr);

  EXPECT_CALL(mock_tab_, GetContents())
      .WillRepeatedly(Return(web_contents.get()));

  // Setup dummy skill data.
  controller_->test_skill_.id = kTestSkillId;
  controller_->test_skill_.source =
      sync_pb::SkillSource::SKILL_SOURCE_USER_CREATED;
  controller_->test_skill_.prompt = "Test Prompt";

  // Force "Ready" state
  EXPECT_CALL(*controller_, IsClientReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(*controller_, ShowGlicPanel()).Times(1);

  // Run it
  EXPECT_CALL(*controller_, NotifySkillToInvokeChanged()).WillOnce([this]() {
    controller_->CallRealNotifySkillToInvokeChanged();
  });
  controller_->InvokeSkill(kTestSkillId);

  // Verify Metrics
  histogram_tester_.ExpectBucketCount("Skills.Invoke.Action",
                                      SkillsInvokeAction::kUserCreated, 1);
  histogram_tester_.ExpectUniqueSample("Skills.Invoke.Result",
                                       SkillsInvokeResult::kSuccess, 1);
}

TEST_F(SkillsUiTabControllerTest, InvokeSkill_LogsFirstPartyInvokeMetrics) {
  TestingProfile* profile =
      profile_manager_->CreateTestingProfile("test_profile");
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile, nullptr);

  EXPECT_CALL(mock_tab_, GetContents())
      .WillRepeatedly(Return(web_contents.get()));

  controller_->test_skill_.id = kTestSkillId;
  controller_->test_skill_.source =
      sync_pb::SkillSource::SKILL_SOURCE_FIRST_PARTY;
  controller_->test_skill_.prompt = "Test Prompt";

  EXPECT_CALL(*controller_, IsClientReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(*controller_, ShowGlicPanel()).Times(1);

  EXPECT_CALL(*controller_, NotifySkillToInvokeChanged()).WillOnce([this]() {
    controller_->CallRealNotifySkillToInvokeChanged();
  });
  controller_->InvokeSkill(kTestSkillId);

  // Verify Metrics
  histogram_tester_.ExpectBucketCount("Skills.Invoke.Action",
                                      SkillsInvokeAction::kFirstParty, 1);
  histogram_tester_.ExpectBucketCount("Skills.Invoke.Action",
                                      SkillsInvokeAction::kUserCreated, 0);
  histogram_tester_.ExpectUniqueSample("Skills.Invoke.Result",
                                       SkillsInvokeResult::kSuccess, 1);
}

TEST_F(SkillsUiTabControllerTest, InvokeSkill_SkillNotFound_LogsMetric) {
  // Force the client to be ready so it immediately tries to invoke
  EXPECT_CALL(*controller_, IsClientReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(*controller_, ShowGlicPanel()).Times(1);

  // Allow the real notification method to run
  EXPECT_CALL(*controller_, NotifySkillToInvokeChanged()).WillOnce([this]() {
    controller_->CallRealNotifySkillToInvokeChanged();
  });

  // Pass an ID that the mock GetSkill() doesn't recognize
  controller_->InvokeSkill("some_deleted_skill_id");

  // Assert the error metric fired safely
  histogram_tester_.ExpectUniqueSample(
      "Skills.Invoke.Result", skills::SkillsInvokeResult::kSkillNotFound, 1);
}
}  // namespace skills
