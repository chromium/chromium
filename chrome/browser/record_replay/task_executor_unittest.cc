// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/record_replay/task_executor.h"

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/record_replay/core/browser/task_definition.pb.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace record_replay {

// Custom GMock matcher to verify GlicInvokeOptions contains the expected prompt
// and target window.
MATCHER_P2(HasPromptAndTargetWindow, expected_prompt, expected_window, "") {
  if (arg.prompts.empty() || arg.prompts[0] != expected_prompt) {
    *result_listener << "Prompt mismatch. Actual: "
                     << (arg.prompts.empty() ? "empty" : arg.prompts[0]);
    return false;
  }

  if (!std::holds_alternative<glic::NewTab>(arg.target.surface)) {
    *result_listener << "Target surface is not NewTab";
    return false;
  }

  auto actual_window = std::get<glic::NewTab>(arg.target.surface).window;
  if (actual_window != expected_window) {
    *result_listener << "Target window mismatch";
    return false;
  }

  return true;
}

// Custom GMock matcher to verify GlicInvokeOptions contains the expected prompt
// and target active tab.
MATCHER_P2(HasPromptAndTargetTab, expected_prompt, expected_tab, "") {
  if (arg.prompts.empty() || arg.prompts[0] != expected_prompt) {
    *result_listener << "Prompt mismatch. Actual: "
                     << (arg.prompts.empty() ? "empty" : arg.prompts[0]);
    return false;
  }

  if (!std::holds_alternative<raw_ptr<tabs::TabInterface>>(
          arg.target.surface)) {
    *result_listener << "Target surface is not raw_ptr<tabs::TabInterface>";
    return false;
  }

  auto actual_tab =
      std::get<raw_ptr<tabs::TabInterface>>(arg.target.surface).get();
  if (actual_tab != expected_tab) {
    *result_listener << "Target tab mismatch";
    return false;
  }

  return true;
}

class TaskExecutorTest : public testing::Test {
 public:
  TaskExecutorTest() : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    glic::GlicEnabling::SetBypassEnablementChecksForTesting(true);
  }

  ~TaskExecutorTest() override {
    glic::GlicEnabling::SetBypassEnablementChecksForTesting(false);
  }

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());

    // Create testing profile with custom testing factories
    TestingProfile::TestingFactories factories;
    factories.push_back(
        {glic::GlicKeyedServiceFactory::GetInstance(),
         base::BindRepeating(&TaskExecutorTest::CreateMockGlicService,
                             base::Unretained(this))});

    profile_ = profile_manager_.CreateTestingProfile("test_profile",
                                                     std::move(factories));

    ON_CALL(test_window_, GetProfile())
        .WillByDefault(testing::Return(profile_));
  }

  void TearDown() override {
    mock_service_ = nullptr;
    profile_ = nullptr;
  }

  std::unique_ptr<KeyedService> CreateMockGlicService(
      content::BrowserContext* context) {
    Profile* profile_ptr = Profile::FromBrowserContext(context);
    auto service =
        std::make_unique<testing::NiceMock<glic::MockGlicKeyedService>>(
            context, IdentityManagerFactory::GetForProfile(profile_ptr),
            profile_manager_.profile_manager(), &glic_profile_manager_, nullptr,
            nullptr);
    mock_service_ = service.get();
    return service;
  }

  Profile* profile() { return profile_; }
  MockBrowserWindowInterface* window() { return &test_window_; }
  glic::MockGlicKeyedService* mock_glic_service() { return mock_service_; }
  void ClearMockService() { mock_service_ = nullptr; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  glic::GlicProfileManager glic_profile_manager_;
  raw_ptr<Profile> profile_ = nullptr;
  testing::NiceMock<MockBrowserWindowInterface> test_window_;
  raw_ptr<glic::MockGlicKeyedService> mock_service_ = nullptr;
};

TEST_F(TaskExecutorTest, ExecutesTaskWithCorrectPrompt) {
  // 1. Set up test TaskDefinition and Parameter Values
  TaskDefinition definition;
  definition.set_title("Book Flight");
  definition.set_description("Find a flight to Paris");

  TaskStep* step = definition.add_task_steps();
  step->set_step_index(0);
  step->set_description("Enter destination");
  step->set_url("https://flights.example.com");

  TaskParameter* param = step->add_parameters();
  param->set_id(123);
  param->set_key("dest_input");
  param->set_name("Destination");

  TaskParameter param_val;
  param_val.set_id(123);
  param_val.set_key("dest_input");
  param_val.set_value("CDG");
  std::vector<TaskParameter> values = {param_val};

  // Expected prompt generated by TaskExecutor
  std::string expected_prompt =
      "Perform this task for me: \"Book Flight\".\n"
      "Instructions: Find a flight to Paris\n\n"
      "Steps to follow:\n"
      "- Step 1: Enter destination (URL: https://flights.example.com)\n"
      "  * Destination: CDG\n";

  // 2. Set GMock expectation on GlicKeyedService::Invoke
  EXPECT_CALL(*mock_glic_service(),
              Invoke(HasPromptAndTargetWindow(expected_prompt, window())))
      .Times(1);

  // 3. Run TaskExecutor
  TaskExecutor::ExecuteTask(profile(), window(), definition, values);
}

TEST_F(TaskExecutorTest, ExecutesTaskTargetingActiveTabSidePanel) {
  // 1. Set up test TaskDefinition and Parameter Values
  TaskDefinition definition;
  definition.set_title("Book Flight");
  definition.set_description("Find a flight to Paris");

  TaskStep* step = definition.add_task_steps();
  step->set_step_index(0);
  step->set_description("Enter destination");
  step->set_url("https://flights.example.com");

  std::vector<TaskParameter> values;

  // Expected prompt generated by TaskExecutor
  std::string expected_prompt =
      "Perform this task for me: \"Book Flight\".\n"
      "Instructions: Find a flight to Paris\n\n"
      "Steps to follow:\n"
      "- Step 1: Enter destination (URL: https://flights.example.com)\n";

  // 2. Instantiate MockTabInterface and mock window's GetActiveTabInterface.
  tabs::MockTabInterface mock_tab;
  EXPECT_CALL(*window(), GetActiveTabInterface())
      .WillOnce(testing::Return(&mock_tab));

  // 3. Set GMock expectation on GlicKeyedService::Invoke targeting the tab
  EXPECT_CALL(*mock_glic_service(),
              Invoke(HasPromptAndTargetTab(expected_prompt, &mock_tab)))
      .Times(1);

  // 4. Run TaskExecutor
  TaskExecutor::ExecuteTask(profile(), window(), definition, values);
}

TEST_F(TaskExecutorTest, DoesNotInvokeWhenGlicDisabled) {
  // Clear mock_service_ pointer since overwriting testing factory will destroy
  // it.
  ClearMockService();
  // Overwrite testing factory to return nullptr (simulating Glic disabled)
  glic::GlicKeyedServiceFactory::GetInstance()->SetTestingFactory(
      profile(), glic::GlicKeyedServiceFactory::TestingFactory());

  TaskDefinition definition;
  definition.set_title("Test");
  std::vector<TaskParameter> values;

  // ExecuteTask should safely no-op without crashing
  TaskExecutor::ExecuteTask(profile(), window(), definition, values);
}

}  // namespace record_replay
