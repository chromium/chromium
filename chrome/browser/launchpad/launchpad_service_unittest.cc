// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/launchpad/launchpad_service.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/glic/test_support/mock_glic_instance.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"
#include "chrome/browser/launchpad/launchpad_task.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/personal_context/core/context_memory_error.h"
#include "components/personal_context/core/mock_personal_context_service.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/personal_context/proto/context_memory_service.pb.h"
#include "components/personal_context/proto/features/auto_todos.pb.h"
#include "components/personal_context/proto/features/common_data.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace launchpad {

namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::FieldsAre;
using ::testing::VariantWith;

class LaunchpadServiceTest : public testing::Test {
 public:
  LaunchpadServiceTest()
      : service_(&profile_, &mock_personal_context_service_) {}
  ~LaunchpadServiceTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  personal_context::MockPersonalContextService mock_personal_context_service_;
  LaunchpadService service_;
};

TEST_F(LaunchpadServiceTest, GenerateTasks_ServiceSuccess) {
  personal_context::proto::AutoTodosResponse expected_response;
  auto* todo = expected_response.add_todos();
  todo->set_id("task_1");
  todo->set_title("Test Todo");
  todo->set_description("Test Description");
  todo->set_actionable_url("https://example.com/action");
  todo->set_importance_score(0.85f);
  todo->set_status(personal_context::proto::AutoTodoItem::STATUS_COMPLETED);

  personal_context::proto::SourceReference* ref_gmail =
      todo->add_source_references();
  ref_gmail->mutable_gmail()->mutable_message_urls()->set_desktop_web_url(
      "https://mail.google.com/mail/u/0/#inbox/123");
  ref_gmail->mutable_gmail()->set_subject("Subject 1");

  personal_context::proto::SourceReference* ref_history =
      todo->add_source_references();
  ref_history->mutable_chrome_history_entry()->set_url(
      "https://example.com/history");
  ref_history->mutable_chrome_history_entry()->set_title("History Title");
  ref_history->mutable_chrome_history_entry()
      ->mutable_visit_timestamp()
      ->set_seconds(1700000000);
  ref_history->mutable_chrome_history_entry()
      ->mutable_visit_timestamp()
      ->set_nanos(500000000);

  personal_context::proto::Any any_response;
  expected_response.SerializeToString(any_response.mutable_value());

  EXPECT_CALL(
      mock_personal_context_service_,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
          base::ok(std::move(any_response)))));

  base::test::TestFuture<const std::optional<std::vector<LaunchpadTask>>&>
      future;
  service_.GenerateTasks(future.GetCallback());

  auto result = future.Get();
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 1u);
  EXPECT_EQ(result->at(0).id, "task_1");
  EXPECT_EQ(result->at(0).title, "Test Todo");
  EXPECT_EQ(result->at(0).description, "Test Description");
  EXPECT_FLOAT_EQ(result->at(0).importance_score, 0.85f);
  EXPECT_EQ(result->at(0).status, LaunchpadTask::Status::kCompleted);
  EXPECT_EQ(result->at(0).actionable_url, GURL("https://example.com/action"));
  ASSERT_EQ(result->at(0).source_references.size(), 2u);
  EXPECT_THAT(
      result->at(0).source_references[0],
      VariantWith<GmailReference>(FieldsAre(
          GURL("https://mail.google.com/mail/u/0/#inbox/123"), "Subject 1")));
  EXPECT_THAT(result->at(0).source_references[1],
              VariantWith<ChromeHistoryEntry>(FieldsAre(
                  GURL("https://example.com/history"), "History Title",
                  base::Time::UnixEpoch() + base::Seconds(1700000000) +
                      base::Nanoseconds(500000000))));

  base::test::TestFuture<std::vector<LaunchpadTask>> get_future;
  service_.GetAllTasks(get_future.GetCallback());
  std::vector<LaunchpadTask> cached_tasks = get_future.Take();
  ASSERT_EQ(cached_tasks.size(), 1u);
  EXPECT_EQ(cached_tasks[0].id, "task_1");
  EXPECT_EQ(cached_tasks[0].title, "Test Todo");
  EXPECT_EQ(cached_tasks[0].status, LaunchpadTask::Status::kCompleted);
}

TEST_F(LaunchpadServiceTest, GenerateTasks_FiltersEmptyIdAndInvalidUrls) {
  personal_context::proto::AutoTodosResponse expected_response;

  // Task with empty ID should be dropped by `TasksStore` and omitted from the
  // `GenerateTasks` callback.
  auto* empty_id_todo = expected_response.add_todos();
  empty_id_todo->set_title("Missing ID");

  // Task with valid ID but non-HTTP(S) URLs and sub-second Unix timestamp.
  auto* todo = expected_response.add_todos();
  todo->set_id("task_valid");
  todo->set_title("Valid Task");
  todo->set_actionable_url("javascript:alert(1)");
  todo->set_status(personal_context::proto::AutoTodoItem::STATUS_DISMISSED);

  auto* bad_gmail = todo->add_source_references();
  bad_gmail->mutable_gmail()->mutable_message_urls()->set_desktop_web_url(
      "chrome://settings");
  bad_gmail->mutable_gmail()->set_subject("Bad Gmail");

  auto* bad_history = todo->add_source_references();
  bad_history->mutable_chrome_history_entry()->set_url("file:///etc/passwd");
  bad_history->mutable_chrome_history_entry()->set_title("Bad History");

  auto* valid_history = todo->add_source_references();
  valid_history->mutable_chrome_history_entry()->set_url(
      "https://example.com/subsecond");
  valid_history->mutable_chrome_history_entry()->set_title("Subsecond History");
  valid_history->mutable_chrome_history_entry()
      ->mutable_visit_timestamp()
      ->set_seconds(0);
  valid_history->mutable_chrome_history_entry()
      ->mutable_visit_timestamp()
      ->set_nanos(500000000);

  // Gmail reference with only legacy `message_url` populated should fall back
  // to `message_url`.
  auto* legacy_gmail = todo->add_source_references();
  legacy_gmail->mutable_gmail()->set_message_url(
      "https://mail.google.com/mail/u/0/#inbox/legacy");
  legacy_gmail->mutable_gmail()->set_subject("Legacy Gmail");

  personal_context::proto::Any any_response;
  expected_response.SerializeToString(any_response.mutable_value());

  EXPECT_CALL(
      mock_personal_context_service_,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
          base::ok(std::move(any_response)))));

  base::test::TestFuture<const std::optional<std::vector<LaunchpadTask>>&>
      future;
  service_.GenerateTasks(future.GetCallback());

  auto result = future.Get();
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 1u);
  EXPECT_EQ(result->at(0).id, "task_valid");
  EXPECT_EQ(result->at(0).status, LaunchpadTask::Status::kDismissed);
  EXPECT_FALSE(result->at(0).actionable_url.has_value());
  ASSERT_EQ(result->at(0).source_references.size(), 2u);
  EXPECT_THAT(result->at(0).source_references[0],
              VariantWith<ChromeHistoryEntry>(FieldsAre(
                  GURL("https://example.com/subsecond"), "Subsecond History",
                  base::Time::UnixEpoch() + base::Nanoseconds(500000000))));
  EXPECT_THAT(result->at(0).source_references[1],
              VariantWith<GmailReference>(FieldsAre(
                  GURL("https://mail.google.com/mail/u/0/#inbox/legacy"),
                  "Legacy Gmail")));

  base::test::TestFuture<std::vector<LaunchpadTask>> get_future;
  service_.GetAllTasks(get_future.GetCallback());
  std::vector<LaunchpadTask> cached_tasks = get_future.Take();
  ASSERT_EQ(cached_tasks.size(), 1u);
  EXPECT_EQ(cached_tasks[0].id, "task_valid");
}

TEST_F(LaunchpadServiceTest, GenerateTasks_ReplacesPreviouslyCachedTasks) {
  personal_context::proto::AutoTodosResponse first_response;
  first_response.add_todos()->set_id("task_1");
  personal_context::proto::Any first_any;
  first_response.SerializeToString(first_any.mutable_value());

  personal_context::proto::AutoTodosResponse second_response;
  second_response.add_todos()->set_id("task_2");
  personal_context::proto::Any second_any;
  second_response.SerializeToString(second_any.mutable_value());

  EXPECT_CALL(
      mock_personal_context_service_,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .WillOnce(RunOnceCallback<3>(
          personal_context::FetchContextResult(base::ok(std::move(first_any)))))
      .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
          base::ok(std::move(second_any)))));

  base::test::TestFuture<const std::optional<std::vector<LaunchpadTask>>&>
      first_future;
  service_.GenerateTasks(first_future.GetCallback());
  ASSERT_TRUE(first_future.Get().has_value());

  base::test::TestFuture<const std::optional<std::vector<LaunchpadTask>>&>
      second_future;
  service_.GenerateTasks(second_future.GetCallback());
  ASSERT_TRUE(second_future.Get().has_value());

  base::test::TestFuture<std::vector<LaunchpadTask>> get_future;
  service_.GetAllTasks(get_future.GetCallback());
  std::vector<LaunchpadTask> cached_tasks = get_future.Take();
  ASSERT_EQ(cached_tasks.size(), 1u);
  EXPECT_EQ(cached_tasks[0].id, "task_2");
}

TEST_F(LaunchpadServiceTest, GenerateTasks_TracksIsGeneratingTasksState) {
  personal_context::FetchContextCallback saved_callback;
  EXPECT_CALL(
      mock_personal_context_service_,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .WillOnce([&](personal_context::proto::ContextMemoryFeature,
                    const google::protobuf::MessageLite&,
                    const personal_context::ContextMemoryRequestOptions&,
                    personal_context::FetchContextCallback callback) {
        saved_callback = std::move(callback);
      });

  EXPECT_FALSE(service_.IsGeneratingTasks());

  base::test::TestFuture<const std::optional<std::vector<LaunchpadTask>>&>
      future_1;
  service_.GenerateTasks(future_1.GetCallback());
  EXPECT_TRUE(service_.IsGeneratingTasks());

  // While a request is in flight, a second call is rejected without sending
  // another RPC.
  base::test::TestFuture<const std::optional<std::vector<LaunchpadTask>>&>
      future_2;
  service_.GenerateTasks(future_2.GetCallback());
  EXPECT_FALSE(future_2.Get().has_value());
  EXPECT_TRUE(service_.IsGeneratingTasks());

  personal_context::proto::AutoTodosResponse response;
  response.add_todos()->set_id("task_1");
  personal_context::proto::Any any_response;
  response.SerializeToString(any_response.mutable_value());
  std::move(saved_callback)
      .Run(personal_context::FetchContextResult(
          base::ok(std::move(any_response))));

  EXPECT_FALSE(service_.IsGeneratingTasks());
  ASSERT_TRUE(future_1.Get().has_value());
  EXPECT_EQ(future_1.Get()->size(), 1u);
}

TEST_F(LaunchpadServiceTest, GenerateTasks_ServiceErrorPreservesCache) {
  personal_context::proto::AutoTodosResponse initial_response;
  initial_response.add_todos()->set_id("task_1");
  personal_context::proto::Any initial_any;
  initial_response.SerializeToString(initial_any.mutable_value());

  personal_context::ContextMemoryError expected_error =
      personal_context::ContextMemoryError::FromExecutionError(
          personal_context::ContextMemoryError::ExecutionError::
              kGenericFailure);

  EXPECT_CALL(
      mock_personal_context_service_,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
          base::ok(std::move(initial_any)))))
      .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
          base::unexpected(expected_error))));

  base::test::TestFuture<const std::optional<std::vector<LaunchpadTask>>&>
      initial_future;
  service_.GenerateTasks(initial_future.GetCallback());
  ASSERT_TRUE(initial_future.Get().has_value());

  base::test::TestFuture<const std::optional<std::vector<LaunchpadTask>>&>
      error_future;
  service_.GenerateTasks(error_future.GetCallback());
  EXPECT_FALSE(error_future.Get().has_value());
  EXPECT_FALSE(service_.IsGeneratingTasks());

  base::test::TestFuture<std::vector<LaunchpadTask>> get_future;
  service_.GetAllTasks(get_future.GetCallback());
  std::vector<LaunchpadTask> cached_tasks = get_future.Take();
  ASSERT_EQ(cached_tasks.size(), 1u);
  EXPECT_EQ(cached_tasks[0].id, "task_1");
}

TEST_F(LaunchpadServiceTest, GenerateTasks_ParseError) {
  personal_context::proto::Any any_response;
  any_response.set_value("corrupted proto data");

  EXPECT_CALL(
      mock_personal_context_service_,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
          base::ok(std::move(any_response)))));

  base::test::TestFuture<const std::optional<std::vector<LaunchpadTask>>&>
      future;
  service_.GenerateTasks(future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
  EXPECT_FALSE(service_.IsGeneratingTasks());
}

class LaunchpadServiceRunTaskTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());

    profile_ = profile_manager_.CreateTestingProfile(
        "launchpad_test",
        {TestingProfile::TestingFactory{
            glic::GlicKeyedServiceFactory::GetInstance(),
            base::BindRepeating(
                &LaunchpadServiceRunTaskTest::CreateMockGlicService,
                base::Unretained(this))}});
    ASSERT_TRUE(profile_);

    mock_glic_service_ = static_cast<glic::MockGlicKeyedService*>(
        glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile_,
                                                           /*create=*/true));
    ASSERT_TRUE(mock_glic_service_);

    service_ = std::make_unique<LaunchpadService>(
        profile_, &mock_personal_context_service_);
  }

  content::BrowserTaskEnvironment task_environment_;
  std::optional<glic::GlicEnabling::ScopedBypassEnablementChecksForTesting>
      scoped_glic_bypass_{std::in_place};
  glic::GlicProfileManager glic_profile_manager_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  personal_context::MockPersonalContextService mock_personal_context_service_;
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_;
  raw_ptr<TestingProfile> profile_ = nullptr;
  raw_ptr<glic::MockGlicKeyedService> mock_glic_service_ = nullptr;
  std::unique_ptr<LaunchpadService> service_;

 private:
  std::unique_ptr<KeyedService> CreateMockGlicService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    return std::make_unique<testing::NiceMock<glic::MockGlicKeyedService>>(
        profile, IdentityManagerFactory::GetForProfile(profile),
        profile_manager_.profile_manager(), &glic_profile_manager_,
        /*contextual_cueing_service=*/nullptr,
        /*actor_keyed_service=*/nullptr);
  }
};

TEST_F(LaunchpadServiceRunTaskTest, RunTask_InvokesGlicWithAutoSubmit) {
  testing::NiceMock<glic::MockGlicInstance> mock_instance;
  std::optional<glic::GlicInvokeOptions> captured_options;
  std::optional<glic::GlicInvokeWithAutoSubmitOptions>
      captured_auto_submit_options;
  EXPECT_CALL(
      *mock_glic_service_,
      InvokeWithAutoSubmit(
          _, _, testing::Matcher<glic::GlicInvokeWithAutoSubmitOptions>(_)))
      .WillOnce([&](glic::InvokeWithAutoSubmitPasskey,
                    glic::GlicInvokeOptions options,
                    glic::GlicInvokeWithAutoSubmitOptions auto_submit_options) {
        captured_options = std::move(options);
        captured_auto_submit_options = std::move(auto_submit_options);
        return mock_instance.GetWeakPtr();
      });

  EXPECT_TRUE(service_->RunTask("Book apartment tour https://example.com/a",
                                &mock_browser_window_));

  ASSERT_TRUE(captured_options.has_value());
  EXPECT_EQ(captured_options->GetInvocationSource(),
            glic::mojom::InvocationSource::kContextHubAutoTodos);
  EXPECT_THAT(
      captured_options->prompts,
      testing::ElementsAre("Book apartment tour https://example.com/a"));
  EXPECT_EQ(captured_options->feature_mode,
            glic::mojom::FeatureMode::kActuation);
  EXPECT_THAT(captured_options->target.actuation_target,
              VariantWith<glic::mojom::ActuationTarget>(
                  glic::mojom::ActuationTarget::kAgentDecides));
  EXPECT_TRUE(std::holds_alternative<glic::NewConversation>(
      captured_options->target.conversation));
  const auto* surface =
      std::get_if<glic::DefaultSurface>(&captured_options->target.surface);
  ASSERT_TRUE(surface);
  EXPECT_EQ(surface->browser, &mock_browser_window_);

  ASSERT_TRUE(captured_auto_submit_options.has_value());
  EXPECT_TRUE(captured_auto_submit_options->show_panel);
}

TEST_F(LaunchpadServiceRunTaskTest, RunTask_FailsWhenInvocationRejected) {
  EXPECT_CALL(
      *mock_glic_service_,
      InvokeWithAutoSubmit(
          _, _, testing::Matcher<glic::GlicInvokeWithAutoSubmitOptions>(_)))
      .WillOnce(testing::Return(glic::GlicInvocationHandle()));

  EXPECT_FALSE(
      service_->RunTask("Task: Reply to Alice", &mock_browser_window_));
}

TEST_F(LaunchpadServiceRunTaskTest, RunTask_FailsWhenPromptEmpty) {
  EXPECT_CALL(
      *mock_glic_service_,
      InvokeWithAutoSubmit(
          _, _, testing::Matcher<glic::GlicInvokeWithAutoSubmitOptions>(_)))
      .Times(0);

  EXPECT_FALSE(service_->RunTask("", &mock_browser_window_));
}

TEST_F(LaunchpadServiceRunTaskTest, RunTask_FailsWhenGlicDisabledForProfile) {
  scoped_glic_bypass_.reset();
  EXPECT_CALL(
      *mock_glic_service_,
      InvokeWithAutoSubmit(
          _, _, testing::Matcher<glic::GlicInvokeWithAutoSubmitOptions>(_)))
      .Times(0);

  EXPECT_FALSE(
      service_->RunTask("Task: Reply to Alice", &mock_browser_window_));
}

TEST_F(LaunchpadServiceRunTaskTest, RunTaskForProfile_FailsWhenProfileNull) {
  EXPECT_CALL(
      *mock_glic_service_,
      InvokeWithAutoSubmit(
          _, _, testing::Matcher<glic::GlicInvokeWithAutoSubmitOptions>(_)))
      .Times(0);

  EXPECT_FALSE(LaunchpadService::RunTaskForProfile(
      /*profile=*/nullptr, "Task: Reply to Alice", &mock_browser_window_));
}

}  // namespace
}  // namespace launchpad
