// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/skills/skills_ui_tab_controller.h"

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/test_renderer_host.h"
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

  MOCK_METHOD(void, ShowGlicPanel, (), (override));
  MOCK_METHOD(bool, IsClientReady, (), (override));
  MOCK_METHOD(void, NotifySkillToInvokeChanged, (), (override));
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
}

}  // namespace skills
