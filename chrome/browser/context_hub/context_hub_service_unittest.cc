// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/context_hub_service.h"

#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/power_monitor_test.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/context_hub/auto_todos/auto_todo_entry.h"
#include "chrome/browser/context_hub/auto_todos/in_memory_auto_todos_store.h"
#include "chrome/browser/context_hub/features.h"
#include "chrome/browser/context_hub/memory_bank/in_memory_memory_bank.h"
#include "chrome/browser/context_hub/storage/context_hub_backend.h"
#include "chrome/browser/context_hub/tab_group_store/in_memory_tab_group_store.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_tab_visit_tracker.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom-features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/optimization_guide/core/model_execution/test/mock_remote_model_executor.h"
#include "components/optimization_guide/proto/features/context_hub.pb.h"
#include "components/page_content_annotations/content/mock_page_content_services.h"
#include "components/page_content_annotations/core/page_content_extraction_types.h"
#include "components/personal_context/core/context_memory_error.h"
#include "components/personal_context/core/mock_personal_context_service.h"
#include "components/personal_context/proto/features/auto_todos.pb.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/test_support/fake_tab_group_sync_service.h"
#include "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/tab_list/tab_removed_reason.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#endif

namespace context_hub {

namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::FieldsAre;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

class MockServiceObserver : public ContextHubService::Observer {
 public:
  MockServiceObserver() = default;
  MockServiceObserver(const MockServiceObserver&) = delete;
  MockServiceObserver& operator=(const MockServiceObserver&) = delete;
  ~MockServiceObserver() override = default;

  MOCK_METHOD(void,
              OnAutoTodosChanged,
              (base::span<const AutoTodoEntry>),
              (override));
  MOCK_METHOD(void,
              OnFirstPartyAutoTodosGenerationStateChanged,
              (bool),
              (override));
  MOCK_METHOD(void,
              OnThirdPartyAutoTodosGenerationStateChanged,
              (bool),
              (override));
};

class MockPageContentExtractionService
    : public page_content_annotations::MockPageContentExtractionService {
 public:
  MockPageContentExtractionService() = default;
  ~MockPageContentExtractionService() override = default;

  MOCK_METHOD(void,
              GetExtractedPageContentAndEligibilityForPageAsync,
              (content::Page&,
               page_content_annotations::PageContentExtractionService::
                   GetExtractedPageContentAndEligibilityCallback,
               bool),
              (override));
};

class ContextHubServiceTest : public testing::Test {
 public:
  ContextHubServiceTest()
      : service_(&profile_,
                 identity_test_environment_.identity_manager(),
                 &mock_personal_context_service_,
                 &mock_remote_model_executor_,
                 &fake_tab_group_sync_service_,
                 &mock_page_content_extraction_service_,
                 std::make_unique<InMemoryMemoryBank>(),
                 std::make_unique<InMemoryTabGroupStore>(),
                 /*context_hub_backend=*/nullptr,
                 std::make_unique<InMemoryAutoTodosStore>()) {
    // Advance mock clock past Unix epoch so that relative timestamps (e.g.
    // `base::Time::Now() - base::Hours(3)`) are positive regardless of host
    // uptime.
    task_environment_.AdvanceClock(base::Days(100));

    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {
            browser::context_hub::mojom::kAutoTodos,
            browser::context_hub::mojom::kAutoTabGroups,
        },
        /*disabled_features=*/{});

    ON_CALL(mock_remote_model_executor_, ExecuteModel)
        .WillByDefault(
            [this](optimization_guide::ModelBasedCapabilityKey feature,
                   const google::protobuf::MessageLite& request_metadata,
                   const optimization_guide::ModelExecutionOptions& options,
                   optimization_guide::
                       OptimizationGuideModelExecutionResultCallback callback) {
              base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE,
                  base::BindOnce(
                      std::move(callback),
                      CreateContextHubResponseResult(
                          "Todo title",
                          optimization_guide::proto::BrowserBasedTodosResponse::
                              GROUP_TYPE_READING_LIST),
                      nullptr));
            });
  }
  ~ContextHubServiceTest() override = default;

 protected:
  std::unique_ptr<content::WebContents> CreateEligibleTab(
      const GURL& url = GURL("https://example.com"),
      base::TimeDelta inactive_time = base::Hours(3),
      bool is_visible = false) {
    auto web_contents =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    sessions::SessionTabHelper::CreateForWebContents(
        web_contents.get(), sessions::SessionTabHelper::DelegateLookup());
    content::WebContentsTester::For(web_contents.get())->NavigateAndCommit(url);
    content::WebContentsTester::For(web_contents.get())
        ->SetLastActiveTime(base::Time::Now() - inactive_time);
    if (is_visible) {
      web_contents->WasShown();
    } else {
      web_contents->WasHidden();
    }
    return web_contents;
  }

  void MockPageContentExtraction(content::WebContents* web_contents,
                                 const std::string& title = "Page Title") {
    scoped_refptr<page_content_annotations::RefCountedAnnotatedPageContent>
        apc = base::MakeRefCounted<
            page_content_annotations::RefCountedAnnotatedPageContent>();
    apc->data.mutable_main_frame_data()->set_title(title);
    page_content_annotations::ExtractedPageContentResult extracted_result(
        std::move(apc), base::Time::Now(),
        /*is_eligible_for_server_upload=*/true,
        /*screenshot_data=*/{});
    EXPECT_CALL(mock_page_content_extraction_service_,
                GetExtractedPageContentAndEligibilityForPageAsync(
                    testing::Ref(web_contents->GetPrimaryPage()), _, true))
        .WillOnce(RunOnceCallback<1>(std::move(extracted_result)));
  }

  std::unique_ptr<content::WebContents> CreateEligibleTabWithMockExtraction(
      const GURL& url = GURL("https://example.com"),
      const std::string& title = "Page Title") {
    auto web_contents = CreateEligibleTab(url);
    MockPageContentExtraction(web_contents.get(), title);
    return web_contents;
  }

  optimization_guide::OptimizationGuideModelExecutionResult
  CreateContextHubResponseResult(
      const std::string& todo_title,
      optimization_guide::proto::BrowserBasedTodosResponse::GroupType
          group_type) {
    optimization_guide::proto::ContextHubResponse response;
    auto* todo_proto = response.mutable_browser_based_todos_response();
    todo_proto->set_todo_title(todo_title);
    todo_proto->set_group_type(group_type);

    optimization_guide::proto::Any any_response;
    any_response.set_type_url(
        "type.googleapis.com/optimization_guide.proto.ContextHubResponse");
    response.SerializeToString(any_response.mutable_value());
    return optimization_guide::OptimizationGuideModelExecutionResult(
        base::ok(std::move(any_response)), nullptr);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  TestingProfile profile_;
  signin::IdentityTestEnvironment identity_test_environment_;
  personal_context::MockPersonalContextService mock_personal_context_service_;
  optimization_guide::MockRemoteModelExecutor mock_remote_model_executor_;
  tab_groups::FakeTabGroupSyncService fake_tab_group_sync_service_;
  MockPageContentExtractionService mock_page_content_extraction_service_;
  ContextHubService service_;
};

TEST_F(ContextHubServiceTest, GenerateFirstPartyAutoTodos_ServiceSuccess) {
  // No previous generation time.
  EXPECT_TRUE(service_.GetLastFirstPartyGenerationTime().is_null());

  personal_context::proto::AutoTodosResponse expected_response;
  auto* todo = expected_response.add_todos();
  todo->set_title("Test Todo");
  todo->set_description("Test Description");

  personal_context::proto::Any any_response;
  expected_response.SerializeToString(any_response.mutable_value());

  EXPECT_CALL(
      mock_personal_context_service_,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
          base::ok(std::move(any_response)))));

  MockServiceObserver observer;
  base::ScopedObservation<ContextHubService, ContextHubService::Observer>
      observation(&observer);
  observation.Observe(&service_);

  EXPECT_CALL(observer, OnFirstPartyAutoTodosGenerationStateChanged(true));
  // Notification after adding the todos.
  EXPECT_CALL(observer,
              OnAutoTodosChanged(ElementsAre(AllOf(
                  Field(&AutoTodoEntry::id, "todo_1"),
                  Field(&AutoTodoEntry::title, "Test Todo"),
                  Field(&AutoTodoEntry::description, "Test Description")))));
  EXPECT_CALL(observer, OnFirstPartyAutoTodosGenerationStateChanged(false));

  base::test::TestFuture<bool> future;
  service_.GenerateFirstPartyAutoTodos(future.GetCallback());

  EXPECT_TRUE(future.Get());
  EXPECT_EQ(service_.GetLastFirstPartyGenerationTime(), base::Time::Now());
}

TEST_F(ContextHubServiceTest,
       TimerTriggerDeletesExpiredTodosEvenWhenNotSignedIn) {
  AutoTodoEntry valid_todo;
  valid_todo.id = "todo_1";
  valid_todo.title = "Valid Todo";
  base::test::TestFuture<bool> add_future;
  service_.UpdateAutoTodo(std::move(valid_todo), add_future.GetCallback());
  EXPECT_TRUE(add_future.Get());

  // Fast forward close to expiration (29 days).
  task_environment_.FastForwardBy(base::Days(29));

  MockServiceObserver observer;
  base::ScopedObservation<ContextHubService, ContextHubService::Observer>
      observation(&observer);
  observation.Observe(&service_);

  // Crossing expiration threshold and triggering the daily background timer
  // should delete the expired item and notify observers.
  EXPECT_CALL(observer, OnAutoTodosChanged(IsEmpty()));
  task_environment_.FastForwardBy(base::Days(2));

  base::test::TestFuture<std::vector<AutoTodoEntry>> get_future;
  service_.GetAutoTodos(get_future.GetCallback());
  EXPECT_TRUE(get_future.Get().empty());
}

TEST_F(ContextHubServiceTest,
       GenerateFirstPartyAutoTodos_ConcurrentRequestsQueueAndResolve) {
  personal_context::proto::AutoTodosResponse expected_response;
  auto* todo = expected_response.add_todos();
  todo->set_title("Test Todo");
  todo->set_description("Test Description");

  personal_context::proto::Any any_response;
  expected_response.SerializeToString(any_response.mutable_value());

  personal_context::FetchContextCallback captured_callback;
  EXPECT_CALL(
      mock_personal_context_service_,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .WillOnce(testing::WithArg<3>(
          [&](personal_context::FetchContextCallback callback) {
            captured_callback = std::move(callback);
          }));

  base::test::TestFuture<bool> future1;
  base::test::TestFuture<bool> future2;

  // First request starts the fetch.
  service_.GenerateFirstPartyAutoTodos(future1.GetCallback());
  EXPECT_TRUE(service_.IsGeneratingFirstPartyAutoTodos());

  // Second request arrives while first is in-flight. It should queue and not
  // send duplicate fetch.
  service_.GenerateFirstPartyAutoTodos(future2.GetCallback());

  // Complete the backend fetch.
  ASSERT_TRUE(captured_callback);
  std::move(captured_callback)
      .Run(personal_context::FetchContextResult(
          base::ok(std::move(any_response))));

  // Both callers receive success.
  EXPECT_TRUE(future1.Get());
  EXPECT_TRUE(future2.Get());
  EXPECT_FALSE(service_.IsGeneratingFirstPartyAutoTodos());
}

TEST_F(ContextHubServiceTest,
       GenerateFirstPartyAutoTodos_UpdatesExistingTodoWithId) {
  // Add an existing first-party todo to the cache.
  AutoTodoEntry first_party_entry;
  first_party_entry.id = "todo_1";
  first_party_entry.title = "Existing First Party Todo";
  first_party_entry.description = "Existing Description";
  first_party_entry.importance_score = 0.85f;
  first_party_entry.status = AutoTodoEntry::Status::kCompleted;
  first_party_entry.data = FirstPartyData{
      .source_references = {{.url = GURL("https://mail.google.com/123"),
                             .subject = "Test Subject"}},
      .actionable_url = GURL("https://example.com/action")};

  base::test::TestFuture<bool> add_fp_future;
  service_.UpdateAutoTodo(std::move(first_party_entry),
                          add_fp_future.GetCallback());
  EXPECT_TRUE(add_fp_future.Get());

  // Add an existing third-party tab todo to verify it is filtered out from the
  // request, but preserved in the cache.
  AutoTodoEntry third_party_entry;
  third_party_entry.id = "todo_2";
  third_party_entry.title = "Tab Todo";
  third_party_entry.data = ThirdPartyData{.tab_id = 42};

  base::test::TestFuture<bool> add_tp_future;
  service_.UpdateAutoTodo(std::move(third_party_entry),
                          add_tp_future.GetCallback());
  EXPECT_TRUE(add_tp_future.Get());

  // Prepare a mock response returning todo_1 with its existing metadata
  // preserved with the addition of a second source reference.
  personal_context::proto::AutoTodosResponse expected_response;
  auto* todo = expected_response.add_todos();
  todo->set_id("todo_1");
  todo->set_title("Existing First Party Todo");
  todo->set_description("Existing Description");
  todo->set_importance_score(0.85f);
  todo->set_actionable_url("https://example.com/action");
  personal_context::proto::GmailReference* ref1 =
      todo->add_source_references()->mutable_gmail();
  ref1->set_message_url("https://mail.google.com/123");
  ref1->set_subject("Test Subject");
  personal_context::proto::GmailReference* ref2 =
      todo->add_source_references()->mutable_gmail();
  ref2->set_message_url("https://mail.google.com/456");
  ref2->set_subject("Additional Subject");

  personal_context::proto::Any any_response;
  expected_response.SerializeToString(any_response.mutable_value());

  // Verify that the request correctly populates existing first-party todos with
  // all their fields, and excludes third-party todos.
  EXPECT_CALL(
      mock_personal_context_service_,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .WillOnce(
          [&](personal_context::proto::ContextMemoryFeature feature,
              const google::protobuf::MessageLite& request_metadata,
              const personal_context::ContextMemoryRequestOptions& options,
              personal_context::FetchContextCallback callback) {
            const auto& auto_todos_request =
                static_cast<const personal_context::proto::AutoTodosRequest&>(
                    request_metadata);
            EXPECT_EQ(auto_todos_request.existing_todos_size(), 1);
            if (auto_todos_request.existing_todos_size() == 1) {
              const auto& existing = auto_todos_request.existing_todos(0);
              EXPECT_EQ(existing.id(), "todo_1");
              EXPECT_EQ(existing.title(), "Existing First Party Todo");
              EXPECT_EQ(existing.description(), "Existing Description");
              EXPECT_FLOAT_EQ(existing.importance_score(), 0.85f);
              EXPECT_EQ(
                  existing.status(),
                  personal_context::proto::AutoTodoItem::STATUS_COMPLETED);
              EXPECT_EQ(existing.actionable_url(),
                        "https://example.com/action");
              EXPECT_EQ(existing.source_references_size(), 1);
              if (existing.source_references_size() == 1) {
                EXPECT_EQ(existing.source_references(0).gmail().message_url(),
                          "https://mail.google.com/123");
                EXPECT_EQ(existing.source_references(0).gmail().subject(),
                          "Test Subject");
              }
            }
            std::move(callback).Run(personal_context::FetchContextResult(
                base::ok(std::move(any_response))));
          });

  // Set up observers to verify generation state changes and updated todo list.
  MockServiceObserver observer;
  base::ScopedObservation<ContextHubService, ContextHubService::Observer>
      observation(&observer);
  observation.Observe(&service_);

  EXPECT_CALL(observer, OnFirstPartyAutoTodosGenerationStateChanged(true));
  EXPECT_CALL(
      observer,
      OnAutoTodosChanged(UnorderedElementsAre(
          AllOf(Field(&AutoTodoEntry::id, "todo_1"),
                Field(&AutoTodoEntry::title, "Existing First Party Todo"),
                Field(&AutoTodoEntry::description, "Existing Description"),
                Field(&AutoTodoEntry::importance_score, 0.85f)),
          AllOf(Field(&AutoTodoEntry::id, "todo_2"),
                Field(&AutoTodoEntry::title, "Tab Todo")))));
  EXPECT_CALL(observer, OnFirstPartyAutoTodosGenerationStateChanged(false));

  // Trigger generation and verify completion.
  base::test::TestFuture<bool> future;
  service_.GenerateFirstPartyAutoTodos(future.GetCallback());
  EXPECT_TRUE(future.Get());
  EXPECT_EQ(service_.GetLastFirstPartyGenerationTime(), base::Time::Now());

  // Verify the cache contains both the updated 1p todo (including the
  // updated source references) and unchanged 3p todo.
  base::test::TestFuture<std::vector<AutoTodoEntry>> get_future;
  service_.GetAutoTodos(get_future.GetCallback());
  auto items = get_future.Get();
  ASSERT_EQ(items.size(), 2u);

  EXPECT_EQ(items[0].id, "todo_1");
  EXPECT_EQ(items[0].title, "Existing First Party Todo");
  EXPECT_EQ(items[0].description, "Existing Description");
  EXPECT_FLOAT_EQ(items[0].importance_score, 0.85f);
  ASSERT_TRUE(items[0].is_first_party());
  const auto* fp_data = std::get_if<FirstPartyData>(&items[0].data);
  ASSERT_TRUE(fp_data);
  EXPECT_EQ(fp_data->actionable_url, GURL("https://example.com/action"));
  ASSERT_EQ(fp_data->source_references.size(), 2u);
  EXPECT_EQ(fp_data->source_references[0].url,
            GURL("https://mail.google.com/123"));
  EXPECT_EQ(fp_data->source_references[0].subject, "Test Subject");
  EXPECT_EQ(fp_data->source_references[1].url,
            GURL("https://mail.google.com/456"));
  EXPECT_EQ(fp_data->source_references[1].subject, "Additional Subject");
}

TEST_F(ContextHubServiceTest,
       GenerateFirstPartyAutoTodos_AddsNewTodoWithoutId) {
  // Add an existing first-party todo to the cache (store will assign "todo_1"
  // and advance the auto-id counter).
  AutoTodoEntry first_party_entry;
  first_party_entry.title = "Existing First Party Todo";

  base::test::TestFuture<bool> add_fp_future;
  service_.UpdateAutoTodo(std::move(first_party_entry),
                          add_fp_future.GetCallback());
  EXPECT_TRUE(add_fp_future.Get());

  // Response returns a new todo without an ID.
  personal_context::proto::AutoTodosResponse expected_response;
  auto* todo = expected_response.add_todos();
  todo->set_title("New Auto Todo");
  todo->set_description("New Description");

  personal_context::proto::Any any_response;
  expected_response.SerializeToString(any_response.mutable_value());

  EXPECT_CALL(
      mock_personal_context_service_,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
          base::ok(std::move(any_response)))));

  MockServiceObserver observer;
  base::ScopedObservation<ContextHubService, ContextHubService::Observer>
      observation(&observer);
  observation.Observe(&service_);

  EXPECT_CALL(observer, OnFirstPartyAutoTodosGenerationStateChanged(true));
  EXPECT_CALL(
      observer,
      OnAutoTodosChanged(UnorderedElementsAre(
          AllOf(Field(&AutoTodoEntry::id, "todo_1"),
                Field(&AutoTodoEntry::title, "Existing First Party Todo")),
          AllOf(Field(&AutoTodoEntry::id, "todo_2"),
                Field(&AutoTodoEntry::title, "New Auto Todo"),
                Field(&AutoTodoEntry::description, "New Description")))));
  EXPECT_CALL(observer, OnFirstPartyAutoTodosGenerationStateChanged(false));

  base::test::TestFuture<bool> future;
  service_.GenerateFirstPartyAutoTodos(future.GetCallback());
  EXPECT_TRUE(future.Get());
  EXPECT_EQ(service_.GetLastFirstPartyGenerationTime(), base::Time::Now());

  // Verify that the new todo is in the cache, along with the existing todo.
  base::test::TestFuture<std::vector<AutoTodoEntry>> get_future;
  service_.GetAutoTodos(get_future.GetCallback());
  auto items = get_future.Get();
  EXPECT_EQ(items.size(), 2u);
}

TEST_F(ContextHubServiceTest, GenerateFirstPartyAutoTodos_ServiceError) {
  personal_context::ContextMemoryError expected_error =
      personal_context::ContextMemoryError::FromExecutionError(
          personal_context::ContextMemoryError::ExecutionError::
              kGenericFailure);

  EXPECT_CALL(
      mock_personal_context_service_,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
          base::unexpected(expected_error))));

  MockServiceObserver observer;
  base::ScopedObservation<ContextHubService, ContextHubService::Observer>
      observation(&observer);
  observation.Observe(&service_);

  EXPECT_CALL(observer, OnFirstPartyAutoTodosGenerationStateChanged(true));
  EXPECT_CALL(observer, OnAutoTodosChanged(_)).Times(0);
  EXPECT_CALL(observer, OnFirstPartyAutoTodosGenerationStateChanged(false));

  base::test::TestFuture<bool> future;
  service_.GenerateFirstPartyAutoTodos(future.GetCallback());

  EXPECT_FALSE(future.Get());
  EXPECT_TRUE(service_.GetLastFirstPartyGenerationTime().is_null());
}

TEST_F(ContextHubServiceTest, GenerateFirstPartyAutoTodos_ParseError) {
  personal_context::proto::Any any_response;
  any_response.set_value("corrupted proto data");

  EXPECT_CALL(
      mock_personal_context_service_,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
          base::ok(std::move(any_response)))));

  MockServiceObserver observer;
  base::ScopedObservation<ContextHubService, ContextHubService::Observer>
      observation(&observer);
  observation.Observe(&service_);

  EXPECT_CALL(observer, OnFirstPartyAutoTodosGenerationStateChanged(true));
  EXPECT_CALL(observer, OnAutoTodosChanged(_)).Times(0);
  EXPECT_CALL(observer, OnFirstPartyAutoTodosGenerationStateChanged(false));

  base::test::TestFuture<bool> future;
  service_.GenerateFirstPartyAutoTodos(future.GetCallback());

  EXPECT_FALSE(future.Get());
  EXPECT_TRUE(service_.GetLastFirstPartyGenerationTime().is_null());
}

TEST_F(ContextHubServiceTest, IsGeneratingStateAccessors) {
  EXPECT_FALSE(service_.IsGeneratingFirstPartyAutoTodos());

  personal_context::FetchContextCallback saved_fetch_callback;
  EXPECT_CALL(
      mock_personal_context_service_,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .WillOnce([&](personal_context::proto::ContextMemoryFeature,
                    const google::protobuf::MessageLite&,
                    const personal_context::ContextMemoryRequestOptions&,
                    personal_context::FetchContextCallback callback) {
        saved_fetch_callback = std::move(callback);
      });

  service_.GenerateFirstPartyAutoTodos(base::DoNothing());
  EXPECT_TRUE(service_.IsGeneratingFirstPartyAutoTodos());

  std::move(saved_fetch_callback)
      .Run(personal_context::FetchContextResult(base::unexpected(
          personal_context::ContextMemoryError::FromExecutionError(
              personal_context::ContextMemoryError::ExecutionError::
                  kUnknown))));
  EXPECT_FALSE(service_.IsGeneratingFirstPartyAutoTodos());
}

TEST_F(ContextHubServiceTest,
       GenerateTabBasedTodos_TabWithNullLastActiveTimeNotEligible) {
  // Tab without last active time is not eligible.
  auto web_contents =
      content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
  sessions::SessionTabHelper::CreateForWebContents(
      web_contents.get(), sessions::SessionTabHelper::DelegateLookup());
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(GURL("https://example.com"));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastActiveTime(base::Time());
  web_contents->WasHidden();

  EXPECT_CALL(mock_page_content_extraction_service_,
              GetExtractedPageContentAndEligibilityForPageAsync(_, _, _))
      .Times(0);

  base::test::TestFuture<bool> future;
  service_.GenerateTabBasedTodos({web_contents->GetWeakPtr()},
                                 future.GetCallback());

  EXPECT_TRUE(future.Get());
  EXPECT_EQ(service_.GetLastThirdPartyGenerationTime(), base::Time::Now());
}

TEST_F(ContextHubServiceTest, GenerateTabBasedTodos_VisibleTabNotEligible) {
  // Tab is visible and therefore not eligible.
  auto web_contents =
      CreateEligibleTab(GURL("https://example.com"), base::Hours(3),
                        /*is_visible=*/true);

  EXPECT_CALL(mock_page_content_extraction_service_,
              GetExtractedPageContentAndEligibilityForPageAsync(_, _, _))
      .Times(0);

  base::test::TestFuture<bool> future;
  service_.GenerateTabBasedTodos({web_contents->GetWeakPtr()},
                                 future.GetCallback());

  EXPECT_TRUE(future.Get());
}

TEST_F(ContextHubServiceTest, GenerateTabBasedTodos_PinnedTabNotEligible) {
  // Tab is pinned and therefore not eligible.
  tabs::MockTabInterface mock_tab;
  ui::UnownedUserDataHost user_data_host;
  ON_CALL(mock_tab, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(user_data_host));
  ON_CALL(mock_tab, IsPinned()).WillByDefault(testing::Return(true));

  auto web_contents = CreateEligibleTab(GURL("https://example.com"));
  ON_CALL(mock_tab, GetContents())
      .WillByDefault(testing::Return(web_contents.get()));

  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents.get(),
                                                       &mock_tab);

  EXPECT_CALL(mock_page_content_extraction_service_,
              GetExtractedPageContentAndEligibilityForPageAsync(_, _, _))
      .Times(0);

  base::test::TestFuture<bool> future;
  service_.GenerateTabBasedTodos({web_contents->GetWeakPtr()},
                                 future.GetCallback());

  EXPECT_TRUE(future.Get());
  EXPECT_EQ(service_.GetLastThirdPartyGenerationTime(), base::Time::Now());
}

TEST_F(ContextHubServiceTest, GenerateTabBasedTodos_ReentrancyBlocked) {
  auto web_contents = CreateEligibleTab();

  page_content_annotations::PageContentExtractionService::
      GetExtractedPageContentAndEligibilityCallback saved_callback;
  EXPECT_CALL(mock_page_content_extraction_service_,
              GetExtractedPageContentAndEligibilityForPageAsync(
                  testing::Ref(web_contents->GetPrimaryPage()), _, true))
      .WillOnce([&](content::Page&, auto cb, bool) {
        saved_callback = std::move(cb);
      });

  base::test::TestFuture<bool> future1;
  service_.GenerateTabBasedTodos({web_contents->GetWeakPtr()},
                                 future1.GetCallback());

  // Second call while first is in progress should immediately return false.
  base::test::TestFuture<bool> future2;
  service_.GenerateTabBasedTodos({web_contents->GetWeakPtr()},
                                 future2.GetCallback());
  EXPECT_FALSE(future2.Get());

  scoped_refptr<page_content_annotations::RefCountedAnnotatedPageContent> apc =
      base::MakeRefCounted<
          page_content_annotations::RefCountedAnnotatedPageContent>();
  page_content_annotations::ExtractedPageContentResult extracted_result(
      std::move(apc), base::Time::Now(),
      /*is_eligible_for_server_upload=*/true,
      /*screenshot_data=*/{});
  std::move(saved_callback).Run(std::move(extracted_result));
  EXPECT_TRUE(future1.Get());
}

TEST_F(ContextHubServiceTest,
       GenerateTabBasedTodos_NavigatedPageDuringExtraction) {
  auto web_contents = CreateEligibleTab();

  page_content_annotations::PageContentExtractionService::
      GetExtractedPageContentAndEligibilityCallback saved_callback;
  EXPECT_CALL(mock_page_content_extraction_service_,
              GetExtractedPageContentAndEligibilityForPageAsync(
                  testing::Ref(web_contents->GetPrimaryPage()), _, true))
      .WillOnce([&](content::Page&, auto cb, bool) {
        saved_callback = std::move(cb);
      });

  base::test::TestFuture<bool> future;
  service_.GenerateTabBasedTodos({web_contents->GetWeakPtr()},
                                 future.GetCallback());

  // Navigate to a new page, making the old page non-primary.
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(GURL("https://example.com/new_page"));

  scoped_refptr<page_content_annotations::RefCountedAnnotatedPageContent> apc =
      base::MakeRefCounted<
          page_content_annotations::RefCountedAnnotatedPageContent>();
  page_content_annotations::ExtractedPageContentResult extracted_result(
      std::move(apc), base::Time::Now(),
      /*is_eligible_for_server_upload=*/true,
      /*screenshot_data=*/{});

  // Old extraction callback returns with extracted page content for old page,
  // but page is no longer primary.
  std::move(saved_callback).Run(std::move(extracted_result));
  EXPECT_TRUE(future.Get());
}

TEST_F(ContextHubServiceTest, GenerateTabBasedTodos_NullWebContents) {
  // Tab is null weak pointer.
  base::test::TestFuture<bool> future;
  service_.GenerateTabBasedTodos({base::WeakPtr<content::WebContents>()},
                                 future.GetCallback());

  EXPECT_TRUE(future.Get());
}

TEST_F(ContextHubServiceTest,
       GenerateTabBasedTodos_SuccessfulGenerationSavesTodo) {
  EXPECT_TRUE(service_.GetLastThirdPartyGenerationTime().is_null());

  auto web_contents = CreateEligibleTabWithMockExtraction(
      GURL("https://example.com/item"), "Item Details");

  EXPECT_CALL(
      mock_remote_model_executor_,
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kContextHub, _,
                   _, _))
      .WillOnce(
          [this](
              optimization_guide::ModelBasedCapabilityKey feature,
              const google::protobuf::MessageLite& request_metadata,
              const optimization_guide::ModelExecutionOptions& options,
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            const auto& request = static_cast<
                const optimization_guide::proto::ContextHubRequest&>(
                request_metadata);
            EXPECT_GT(request.entry_items(0).tab().last_active_timestamp_ms(),
                      0);
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(
                    std::move(callback),
                    CreateContextHubResponseResult(
                        "Todo title",
                        optimization_guide::proto::BrowserBasedTodosResponse::
                            GROUP_TYPE_UNFINISHED),
                    nullptr));
          });

  MockServiceObserver observer;
  base::ScopedObservation<ContextHubService, ContextHubService::Observer>
      observation(&observer);
  observation.Observe(&service_);

  EXPECT_CALL(observer, OnThirdPartyAutoTodosGenerationStateChanged(true));
  // Verify that the observer is notified of the todo being saved to the store.
  EXPECT_CALL(observer, OnAutoTodosChanged(ElementsAre(AllOf(
                            Field(&AutoTodoEntry::id, "todo_1"),
                            Field(&AutoTodoEntry::title, "Todo title")))));
  EXPECT_CALL(observer, OnThirdPartyAutoTodosGenerationStateChanged(false));

  base::test::TestFuture<bool> future;
  service_.GenerateTabBasedTodos({web_contents->GetWeakPtr()},
                                 future.GetCallback());
  EXPECT_TRUE(future.Get());
  EXPECT_EQ(service_.GetLastThirdPartyGenerationTime(), base::Time::Now());
}

TEST_F(ContextHubServiceTest,
       GenerateTabBasedTodos_PopulatesLastForegroundDuration) {
  tabs::MockTabInterface mock_tab;
  ui::UnownedUserDataHost user_data_host;
  ON_CALL(mock_tab, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(user_data_host));

  auto web_contents = CreateEligibleTabWithMockExtraction(
      GURL("https://example.com/item"), "Item Details");
  ON_CALL(mock_tab, GetContents())
      .WillByDefault(testing::Return(web_contents.get()));

  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents.get(),
                                                       &mock_tab);
  contextual_tasks::ContextualTasksTabVisitTracker tracker(mock_tab);
  tracker.SetClockForTesting(task_environment_.GetMockTickClock());

  // Simulate a 5-second foreground visit before hiding.
  web_contents->WasShown();
  task_environment_.FastForwardBy(base::Seconds(5));
  web_contents->WasHidden();

  EXPECT_CALL(
      mock_remote_model_executor_,
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kContextHub, _,
                   _, _))
      .WillOnce(
          [this](
              optimization_guide::ModelBasedCapabilityKey feature,
              const google::protobuf::MessageLite& request_metadata,
              const optimization_guide::ModelExecutionOptions& options,
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            const auto& request = static_cast<
                const optimization_guide::proto::ContextHubRequest&>(
                request_metadata);
            EXPECT_EQ(
                request.entry_items(0).tab().last_foreground_duration_ms(),
                5000);
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(
                    std::move(callback),
                    CreateContextHubResponseResult(
                        "Todo title",
                        optimization_guide::proto::BrowserBasedTodosResponse::
                            GROUP_TYPE_UNFINISHED),
                    nullptr));
          });

  base::test::TestFuture<bool> future;
  service_.GenerateTabBasedTodos({web_contents->GetWeakPtr()},
                                 future.GetCallback());
  EXPECT_TRUE(future.Get());
}

TEST_F(ContextHubServiceTest, GenerateTabBasedTodos_MissingPageContentSkipped) {
  auto web_contents = CreateEligibleTab(GURL("https://example.com/page"));

  // Extraction returns result without page_content (null).
  page_content_annotations::ExtractedPageContentResult extracted_result(
      /*page_content=*/nullptr, base::Time::Now(),
      /*is_eligible_for_server_upload=*/true,
      /*screenshot_data=*/{});

  EXPECT_CALL(mock_page_content_extraction_service_,
              GetExtractedPageContentAndEligibilityForPageAsync(
                  testing::Ref(web_contents->GetPrimaryPage()), _, true))
      .WillOnce(RunOnceCallback<1>(std::move(extracted_result)));

  // Model execution should not be triggered since page content is missing.
  EXPECT_CALL(
      mock_remote_model_executor_,
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kContextHub, _,
                   _, _))
      .Times(0);

  base::test::TestFuture<bool> future;
  service_.GenerateTabBasedTodos({web_contents->GetWeakPtr()},
                                 future.GetCallback());
  EXPECT_TRUE(future.Get());
}

TEST_F(ContextHubServiceTest, GenerateTabBasedTodos_SkipsTabsAlreadyInCache) {
  auto web_contents1 = CreateEligibleTab(GURL("https://example.com/tab1"));
  auto web_contents2 = CreateEligibleTabWithMockExtraction(
      GURL("https://example.com/tab2"), "Tab 2");

  // Pre-populate store with a todo for tab1.
  SessionID session_id1 =
      sessions::SessionTabHelper::IdForTab(web_contents1.get());
  AutoTodoEntry existing_entry;
  existing_entry.title = "Existing Todo";
  ThirdPartyData third_party;
  third_party.tab_id = session_id1.id();
  third_party.group_type = ThirdPartyData::GroupType::kReadingList;
  existing_entry.data = std::move(third_party);

  base::test::TestFuture<bool> add_future;
  service_.UpdateAutoTodo(std::move(existing_entry), add_future.GetCallback());
  EXPECT_TRUE(add_future.Get());

  // Page content extraction service should only be called for tab2, not tab1.
  EXPECT_CALL(mock_page_content_extraction_service_,
              GetExtractedPageContentAndEligibilityForPageAsync(
                  testing::Ref(web_contents1->GetPrimaryPage()), _, _))
      .Times(0);

  EXPECT_CALL(
      mock_remote_model_executor_,
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kContextHub, _,
                   _, _))
      .WillOnce(
          [this](
              optimization_guide::ModelBasedCapabilityKey feature,
              const google::protobuf::MessageLite& request_metadata,
              const optimization_guide::ModelExecutionOptions& options,
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(
                    std::move(callback),
                    CreateContextHubResponseResult(
                        "Todo for tab 2",
                        optimization_guide::proto::BrowserBasedTodosResponse::
                            GROUP_TYPE_UNFINISHED),
                    nullptr));
          });

  base::test::TestFuture<bool> future;
  service_.GenerateTabBasedTodos(
      {web_contents1->GetWeakPtr(), web_contents2->GetWeakPtr()},
      future.GetCallback());
  EXPECT_TRUE(future.Get());
}

TEST_F(ContextHubServiceTest, GenerateTabBasedTodos_QueuesRequestsOverLimit) {
  // Set the number of tabs to exceed kMaxConcurrentMesRequests (10) to verify
  // request queuing and batch processing.
  constexpr size_t kNumTabs = 12;
  std::vector<std::unique_ptr<content::WebContents>> web_contents_list;
  std::vector<base::WeakPtr<content::WebContents>> tab_ptrs;

  for (size_t i = 0; i < kNumTabs; ++i) {
    auto web_contents = CreateEligibleTabWithMockExtraction(
        GURL("https://example.com/tab" + base::NumberToString(i)),
        "Tab " + base::NumberToString(i));
    tab_ptrs.push_back(web_contents->GetWeakPtr());
    web_contents_list.push_back(std::move(web_contents));
  }

  // Model execution is called for each tab. Tabs exceeding the concurrency
  // limit of 10 are queued and dispatched as in-flight requests complete.
  EXPECT_CALL(
      mock_remote_model_executor_,
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kContextHub, _,
                   _, _))
      .Times(kNumTabs)
      .WillRepeatedly(
          [this](
              optimization_guide::ModelBasedCapabilityKey feature,
              const google::protobuf::MessageLite& request_metadata,
              const optimization_guide::ModelExecutionOptions& options,
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            const auto& request = static_cast<
                const optimization_guide::proto::ContextHubRequest&>(
                request_metadata);
            int64_t tab_id = request.entry_items(0).tab().tab_id();

            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(
                    std::move(callback),
                    CreateContextHubResponseResult(
                        "Todo for tab " + base::NumberToString(tab_id),
                        optimization_guide::proto::BrowserBasedTodosResponse::
                            GROUP_TYPE_READING_LIST),
                    nullptr));
          });

  base::test::TestFuture<bool> future;
  service_.GenerateTabBasedTodos(std::move(tab_ptrs), future.GetCallback());
  EXPECT_TRUE(future.Get());

  // Verify that all generated todos across batches were stored.
  base::test::TestFuture<std::vector<AutoTodoEntry>> todos_future;
  service_.GetAutoTodos(todos_future.GetCallback());
  EXPECT_EQ(kNumTabs, todos_future.Get().size());
}

TEST_F(ContextHubServiceTest, GenerateTabBasedTodos_SkipsInvalidGroupType) {
  auto web_contents1 = CreateEligibleTabWithMockExtraction(
      GURL("https://example.com/tab1"), "Tab 1");

  EXPECT_CALL(
      mock_remote_model_executor_,
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kContextHub, _,
                   _, _))
      .WillOnce(
          [this](
              optimization_guide::ModelBasedCapabilityKey feature,
              const google::protobuf::MessageLite& request_metadata,
              const optimization_guide::ModelExecutionOptions& options,
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            // GROUP_TYPE_UNSPECIFIED -> kNoMatch, should NOT be saved.
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(
                    std::move(callback),
                    CreateContextHubResponseResult(
                        "Todo title",
                        optimization_guide::proto::BrowserBasedTodosResponse::
                            GROUP_TYPE_UNSPECIFIED),
                    nullptr));
          });

  base::test::TestFuture<bool> future;
  service_.GenerateTabBasedTodos({web_contents1->GetWeakPtr()},
                                 future.GetCallback());
  EXPECT_TRUE(future.Get());

  base::test::TestFuture<std::vector<AutoTodoEntry>> todos_future;
  service_.GetAutoTodos(todos_future.GetCallback());
  auto todos = todos_future.Get();
  EXPECT_TRUE(todos.empty());
}

TEST_F(ContextHubServiceTest, GenerateTabBasedTodos_ShoppingCartGroupType) {
  auto web_contents = CreateEligibleTabWithMockExtraction(
      GURL("https://example.com/cart"), "Shopping Cart");

  EXPECT_CALL(
      mock_remote_model_executor_,
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kContextHub, _,
                   _, _))
      .WillOnce(
          [this](
              optimization_guide::ModelBasedCapabilityKey feature,
              const google::protobuf::MessageLite& request_metadata,
              const optimization_guide::ModelExecutionOptions& options,
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(
                    std::move(callback),
                    CreateContextHubResponseResult(
                        "Cart items",
                        optimization_guide::proto::BrowserBasedTodosResponse::
                            GROUP_TYPE_SHOPPING_CART),
                    nullptr));
          });

  base::test::TestFuture<bool> future;
  service_.GenerateTabBasedTodos({web_contents->GetWeakPtr()},
                                 future.GetCallback());
  EXPECT_TRUE(future.Get());

  base::test::TestFuture<std::vector<AutoTodoEntry>> todos_future;
  service_.GetAutoTodos(todos_future.GetCallback());
  auto todos = todos_future.Get();
  ASSERT_EQ(todos.size(), 1u);
  EXPECT_EQ(todos[0].title, "Cart items");
  ASSERT_TRUE(todos[0].group_type().has_value());
  EXPECT_EQ(todos[0].group_type().value(),
            ThirdPartyData::GroupType::kShoppingCart);
}

TEST_F(ContextHubServiceTest, GenerateTabBasedTodos_EmptyTitleSkipped) {
  auto web_contents = CreateEligibleTabWithMockExtraction(
      GURL("https://example.com/tab"), "Tab");

  EXPECT_CALL(
      mock_remote_model_executor_,
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kContextHub, _,
                   _, _))
      .WillOnce(
          [this](
              optimization_guide::ModelBasedCapabilityKey feature,
              const google::protobuf::MessageLite& request_metadata,
              const optimization_guide::ModelExecutionOptions& options,
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(
                    std::move(callback),
                    CreateContextHubResponseResult(
                        "",
                        optimization_guide::proto::BrowserBasedTodosResponse::
                            GROUP_TYPE_NUDGE_TO_CLOSE),
                    nullptr));
          });

  base::test::TestFuture<bool> future;
  service_.GenerateTabBasedTodos({web_contents->GetWeakPtr()},
                                 future.GetCallback());
  EXPECT_TRUE(future.Get());

  base::test::TestFuture<std::vector<AutoTodoEntry>> todos_future;
  service_.GetAutoTodos(todos_future.GetCallback());
  EXPECT_TRUE(todos_future.Get().empty());
}

TEST_F(ContextHubServiceTest, SaveMemoryBankEntry_Tab) {
  base::test::TestFuture<bool> save_tab_future;
  service_.SaveMemoryBankEntry(
      MemoryBankEntry(MemoryBankType::kTab, GURL("https://example.com"),
                      "Title", "Page text"),
      save_tab_future.GetCallback());
  EXPECT_TRUE(save_tab_future.Get());

  base::test::TestFuture<std::vector<MemoryBankEntry>> get_entries_future;
  service_.GetAllEntries(get_entries_future.GetCallback());
  auto entries = get_entries_future.Get();
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ("Title", entries[0].tab_title);
  EXPECT_EQ(GURL("https://example.com"), entries[0].url);
  EXPECT_EQ(MemoryBankType::kTab, entries[0].type);
}

TEST_F(ContextHubServiceTest, SaveMemoryBankEntry_TextSelection) {
  base::test::TestFuture<bool> save_selection_future;
  service_.SaveMemoryBankEntry(
      MemoryBankEntry(MemoryBankType::kTextSelection,
                      GURL("https://example.com"), "Title", "Selection"),
      save_selection_future.GetCallback());
  EXPECT_TRUE(save_selection_future.Get());

  base::test::TestFuture<std::vector<MemoryBankEntry>> get_entries_future;
  service_.GetAllEntries(get_entries_future.GetCallback());
  auto entries = get_entries_future.Get();
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(MemoryBankType::kTextSelection, entries[0].type);
  EXPECT_EQ("Selection", entries[0].selected_text);
}

TEST_F(ContextHubServiceTest, DeleteEntries) {
  service_.SaveMemoryBankEntry(
      MemoryBankEntry(MemoryBankType::kTab, GURL("https://example1.com"),
                      "Title1", "Page text 1"),
      base::DoNothing());
  service_.SaveMemoryBankEntry(
      MemoryBankEntry(MemoryBankType::kTab, GURL("https://example2.com"),
                      "Title2", "Page text 2"),
      base::DoNothing());

  base::test::TestFuture<std::vector<MemoryBankEntry>> get_entries_future;
  service_.GetAllEntries(get_entries_future.GetCallback());
  auto entries = get_entries_future.Get();
  ASSERT_EQ(2u, entries.size());

  base::test::TestFuture<bool> delete_future;
  std::vector<int64_t> ids_to_delete = {entries[0].id, entries[1].id};
  service_.DeleteEntries(ids_to_delete, delete_future.GetCallback());
  EXPECT_TRUE(delete_future.Get());

  base::test::TestFuture<std::vector<MemoryBankEntry>> get_entries_future2;
  service_.GetAllEntries(get_entries_future2.GetCallback());
  EXPECT_TRUE(get_entries_future2.Get().empty());
}

TEST_F(ContextHubServiceTest, GetEntriesByIds) {
  service_.SaveMemoryBankEntry(
      MemoryBankEntry(MemoryBankType::kTab, GURL("https://example1.com"),
                      "Title1", "Page text 1"),
      base::DoNothing());
  service_.SaveMemoryBankEntry(
      MemoryBankEntry(MemoryBankType::kTab, GURL("https://example2.com"),
                      "Title2", "Page text 2"),
      base::DoNothing());

  base::test::TestFuture<std::vector<MemoryBankEntry>> get_entries_future;
  service_.GetAllEntries(get_entries_future.GetCallback());
  auto entries = get_entries_future.Get();
  ASSERT_EQ(2u, entries.size());

  base::test::TestFuture<std::vector<MemoryBankEntry>> by_ids_future;
  service_.GetEntriesByIds({entries[0].id}, by_ids_future.GetCallback());
  auto selected_entries = by_ids_future.Get();
  ASSERT_EQ(1u, selected_entries.size());
  EXPECT_EQ(entries[0].id, selected_entries[0].id);
  EXPECT_EQ(entries[0].tab_title, selected_entries[0].tab_title);
}

TEST_F(ContextHubServiceTest, UpdateMemoryBankEntryAnnotations) {
  service_.SaveMemoryBankEntry(
      MemoryBankEntry(MemoryBankType::kTab, GURL("https://example.com"),
                      "Original Title", "Page text"),
      base::DoNothing());

  base::test::TestFuture<std::vector<MemoryBankEntry>> get_entries_future;
  service_.GetAllEntries(get_entries_future.GetCallback());
  auto entries = get_entries_future.Get();
  ASSERT_EQ(1u, entries.size());
  int64_t id = entries[0].id;

  base::test::TestFuture<bool> update_future;
  service_.UpdateMemoryBankEntryAnnotations(
      id, {"tag1", "tag2"}, "Updated Note", "Updated Collection",
      update_future.GetCallback());
  EXPECT_TRUE(update_future.Get());

  base::test::TestFuture<std::vector<MemoryBankEntry>> get_updated_future;
  service_.GetAllEntries(get_updated_future.GetCallback());
  auto updated_entries = get_updated_future.Get();
  ASSERT_EQ(1u, updated_entries.size());
  EXPECT_EQ(id, updated_entries[0].id);
  EXPECT_EQ("Original Title", updated_entries[0].tab_title);
  EXPECT_EQ("Updated Note", updated_entries[0].note);
  EXPECT_EQ("Updated Collection", updated_entries[0].collection);
  EXPECT_THAT(updated_entries[0].tags, ElementsAre("tag1", "tag2"));
}

TEST_F(ContextHubServiceTest, UpdateMemoryBankEntryAnnotations_NotFound) {
  base::test::TestFuture<bool> update_future;
  service_.UpdateMemoryBankEntryAnnotations(
      999999, {"tag1"}, "Note", "Collection", update_future.GetCallback());
  EXPECT_FALSE(update_future.Get());
}

TEST_F(ContextHubServiceTest, GetAllMemoryBankTags) {
  MemoryBankEntry entry1(MemoryBankType::kTab, GURL("https://example1.com"),
                         "Title1", "Page text 1");
  entry1.tags = {"tag1", "tag2"};
  service_.SaveMemoryBankEntry(entry1, base::DoNothing());

  MemoryBankEntry entry2(MemoryBankType::kTab, GURL("https://example2.com"),
                         "Title2", "Page text 2");
  entry2.tags = {"tag2", "tag3"};
  service_.SaveMemoryBankEntry(entry2, base::DoNothing());

  base::test::TestFuture<const std::vector<std::string>&> tags_future;
  service_.GetAllMemoryBankTags(tags_future.GetCallback());
  EXPECT_THAT(tags_future.Get(),
              testing::UnorderedElementsAre("tag1", "tag2", "tag3"));
}

TEST_F(ContextHubServiceTest, GetAllMemoryBankCollections) {
  MemoryBankEntry entry1(MemoryBankType::kTab, GURL("https://example1.com"),
                         "Title1", "Page text 1");
  entry1.collection = "Research";
  service_.SaveMemoryBankEntry(entry1, base::DoNothing());

  MemoryBankEntry entry2(MemoryBankType::kTab, GURL("https://example2.com"),
                         "Title2", "Page text 2");
  entry2.collection = "Recipes";
  service_.SaveMemoryBankEntry(entry2, base::DoNothing());

  base::test::TestFuture<const std::vector<std::string>&> coll_future;
  service_.GetAllMemoryBankCollections(coll_future.GetCallback());
  EXPECT_THAT(coll_future.Get(), testing::ElementsAre("Recipes", "Research"));
}

TEST_F(ContextHubServiceTest, GroupTabs_NoTabs) {
  base::test::TestFuture<std::vector<TabGroupEntry>, std::vector<TabData>,
                         std::string>
      future;
  service_.GroupTabs(
      {}, "",
      future.GetCallback<std::vector<TabGroupEntry>, std::vector<TabData>,
                         std::string>());
  auto [groups, ungrouped_tabs, text_response] = future.Take();
  EXPECT_TRUE(groups.empty());
  EXPECT_TRUE(ungrouped_tabs.empty());
  EXPECT_TRUE(text_response.empty());
}

TEST_F(ContextHubServiceTest, GroupTabs_WithTabs) {
  std::vector<TabData> input_tabs = {
      {1, "Tab 1", GURL("https://example1.com")},
      {2, "Tab 2", GURL("https://example2.com")},
      {3, "Tab 3", GURL("https://example3.com")},
      {4, "Tab 4", GURL("https://example4.com")},
      {5, "Tab 5", GURL("https://example5.com")}};

  EXPECT_CALL(
      mock_remote_model_executor_,
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kContextHub, _,
                   _, _))
      .WillOnce([](optimization_guide::ModelBasedCapabilityKey feature,
                   const google::protobuf::MessageLite& request_metadata,
                   const optimization_guide::ModelExecutionOptions& options,
                   optimization_guide::
                       OptimizationGuideModelExecutionResultCallback callback) {
        optimization_guide::proto::ContextHubResponse response;
        optimization_guide::proto::GroupResponse* group_response =
            response.mutable_group_response();
        group_response->set_text_response("Grouped tabs into 2 groups.");
        optimization_guide::proto::TabGroupMinimal* group1 =
            group_response->add_minimal_tab_groups();
        group1->set_label("Group 1");
        group1->add_tab_ids(1);
        group1->add_tab_ids(2);

        optimization_guide::proto::TabGroupMinimal* group2 =
            group_response->add_minimal_tab_groups();
        group2->set_label("Group 2");
        group2->add_tab_ids(3);
        group2->add_tab_ids(4);

        optimization_guide::proto::Any any_response;
        any_response.set_type_url(
            "type.googleapis.com/optimization_guide.proto.ContextHubResponse");
        response.SerializeToString(any_response.mutable_value());

        std::move(callback).Run(
            optimization_guide::OptimizationGuideModelExecutionResult(
                base::ok(std::move(any_response)), nullptr),
            nullptr);
      });

  base::test::TestFuture<std::vector<TabGroupEntry>, std::vector<TabData>,
                         std::string>
      future;
  service_.GroupTabs(
      std::move(input_tabs), "",
      future.GetCallback<std::vector<TabGroupEntry>, std::vector<TabData>,
                         std::string>());
  auto [groups, ungrouped_tabs, text_response] = future.Take();

  ASSERT_EQ(groups.size(), 2u);
  EXPECT_EQ(groups[0].label, "Group 1");
  ASSERT_EQ(groups[0].tabs.size(), 2u);
  EXPECT_EQ(groups[0].tabs[0].id, 1);
  EXPECT_EQ(groups[0].tabs[1].id, 2);

  EXPECT_EQ(groups[1].label, "Group 2");
  ASSERT_EQ(groups[1].tabs.size(), 2u);
  EXPECT_EQ(groups[1].tabs[0].id, 3);
  EXPECT_EQ(groups[1].tabs[1].id, 4);

  ASSERT_EQ(ungrouped_tabs.size(), 1u);
  EXPECT_EQ(ungrouped_tabs[0].id, 5);

  EXPECT_EQ(text_response, "Grouped tabs into 2 groups.");

  auto history = service_.GetTabGroupChatHistory();
  ASSERT_EQ(history.size(), 1u);
  EXPECT_EQ(history[0].role(),
            optimization_guide::proto::ChatHistoryTurn::ROLE_ASSISTANT);
  EXPECT_EQ(history[0].message_content(), "Grouped tabs into 2 groups.");

  base::test::TestFuture<std::vector<TabGroupEntry>> stored_groups_future;
  service_.GetTabGroups(stored_groups_future.GetCallback());
  EXPECT_THAT(
      stored_groups_future.Get(),
      ElementsAre(
          FieldsAre(testing::Ne(""), "Group 1", ElementsAre(1, 2), _,
                    testing::Ne(base::Time()), testing::Ne(base::Time())),
          FieldsAre(testing::Ne(""), "Group 2", ElementsAre(3, 4), _,
                    testing::Ne(base::Time()), testing::Ne(base::Time()))));
}

TEST_F(ContextHubServiceTest, GroupTabs_WithConfirmedGroupsPayload) {
  tab_groups::SavedTabGroup confirmed_group(
      u"Confirmed Group", tab_groups::TabGroupColorId::kBlue, {},
      /*position=*/std::nullopt);
  confirmed_group.SetLocalGroupId(tab_groups::test::GenerateRandomTabGroupID());
  tab_groups::SavedTabGroupTab confirmed_tab1(
      GURL("https://example1.com"), u"Tab 1", confirmed_group.saved_guid(),
      /*position=*/0, /*saved_tab_guid=*/std::nullopt, /*local_tab_id=*/1);
  tab_groups::SavedTabGroupTab confirmed_tab2(
      GURL("https://example2.com"), u"Tab 2", confirmed_group.saved_guid(),
      /*position=*/1, /*saved_tab_guid=*/std::nullopt, /*local_tab_id=*/2);
  confirmed_group.AddTabLocally(confirmed_tab1);
  confirmed_group.AddTabLocally(confirmed_tab2);
  fake_tab_group_sync_service_.AddGroup(confirmed_group);

  std::vector<TabData> ungrouped_tabs = {
      {3, "Tab 3", GURL("https://example3.com")},
      {4, "Tab 4", GURL("https://example4.com")}};

  EXPECT_CALL(
      mock_remote_model_executor_,
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kContextHub, _,
                   _, _))
      .WillOnce([confirmed_guid =
                     confirmed_group.saved_guid().AsLowercaseString()](
                    optimization_guide::ModelBasedCapabilityKey feature,
                    const google::protobuf::MessageLite& request_metadata,
                    const optimization_guide::ModelExecutionOptions& options,
                    optimization_guide::
                        OptimizationGuideModelExecutionResultCallback
                            callback) {
        const auto& request =
            static_cast<const optimization_guide::proto::ContextHubRequest&>(
                request_metadata);
        EXPECT_EQ(request.user_command(), "regroup");
        EXPECT_EQ(request.entry_items_size(), 4);
        ASSERT_EQ(request.pre_existing_tab_groups_size(), 1);
        EXPECT_EQ(request.pre_existing_tab_groups(0).label(),
                  "Confirmed Group");
        EXPECT_EQ(request.pre_existing_tab_groups(0).group_id(),
                  confirmed_guid);
        ASSERT_EQ(request.pre_existing_tab_groups(0).tab_ids_size(), 2);
        EXPECT_EQ(request.pre_existing_tab_groups(0).tab_ids(0), 1);
        EXPECT_EQ(request.pre_existing_tab_groups(0).tab_ids(1), 2);

        optimization_guide::proto::ContextHubResponse response;
        optimization_guide::proto::GroupResponse* group_response =
            response.mutable_group_response();
        optimization_guide::proto::TabGroupMinimal* group1 =
            group_response->add_minimal_tab_groups();
        group1->set_label("Regrouped");
        group1->add_tab_ids(1);
        group1->add_tab_ids(3);

        optimization_guide::proto::Any any_response;
        any_response.set_type_url(
            "type.googleapis.com/optimization_guide.proto.ContextHubResponse");
        response.SerializeToString(any_response.mutable_value());

        std::move(callback).Run(
            optimization_guide::OptimizationGuideModelExecutionResult(
                base::ok(std::move(any_response)), nullptr),
            nullptr);
      });

  base::test::TestFuture<std::vector<TabGroupEntry>, std::vector<TabData>,
                         std::string>
      future;
  service_.GroupTabs(
      std::move(ungrouped_tabs), "regroup",
      future.GetCallback<std::vector<TabGroupEntry>, std::vector<TabData>,
                         std::string>());
  auto [groups, ungrouped, text_response] = future.Take();

  ASSERT_EQ(groups.size(), 1u);
  EXPECT_EQ(groups[0].label, "Regrouped");
  ASSERT_EQ(groups[0].tabs.size(), 2u);
  EXPECT_EQ(groups[0].tabs[0].id, 1);
  EXPECT_EQ(groups[0].tabs[1].id, 3);

  ASSERT_EQ(ungrouped.size(), 2u);
  EXPECT_EQ(ungrouped[0].id, 4);
  EXPECT_EQ(ungrouped[1].id, 2);
}

TEST_F(ContextHubServiceTest, GroupTabs_MESError) {
  std::vector<TabData> input_tabs = {
      {1, "Tab 1", GURL("https://example1.com")},
      {2, "Tab 2", GURL("https://example2.com")}};

  EXPECT_CALL(
      mock_remote_model_executor_,
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kContextHub, _,
                   _, _))
      .WillOnce(
          [](optimization_guide::ModelBasedCapabilityKey feature,
             const google::protobuf::MessageLite& request_metadata,
             const optimization_guide::ModelExecutionOptions& options,
             optimization_guide::OptimizationGuideModelExecutionResultCallback
                 callback) {
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(
                    std::move(callback),
                    optimization_guide::OptimizationGuideModelExecutionResult(),
                    nullptr));
          });

  base::test::TestFuture<std::vector<TabGroupEntry>, std::vector<TabData>,
                         std::string>
      future;
  service_.GroupTabs(
      std::move(input_tabs), "",
      future.GetCallback<std::vector<TabGroupEntry>, std::vector<TabData>,
                         std::string>());
  auto [groups, ungrouped_tabs, text_response] = future.Take();

  EXPECT_TRUE(groups.empty());
  ASSERT_EQ(ungrouped_tabs.size(), 2u);
  EXPECT_EQ(ungrouped_tabs[0].id, 1);
  EXPECT_EQ(ungrouped_tabs[1].id, 2);
  EXPECT_TRUE(text_response.empty());
}

TEST_F(ContextHubServiceTest, AddAndGetTabGroupChatHistory) {
  service_.AddTabGroupChatHistoryTurn(
      optimization_guide::proto::ChatHistoryTurn::ROLE_USER, "User message");
  task_environment_.FastForwardBy(base::Milliseconds(1));
  service_.AddTabGroupChatHistoryTurn(
      optimization_guide::proto::ChatHistoryTurn::ROLE_ASSISTANT,
      "Assistant reply");

  auto history = service_.GetTabGroupChatHistory();
  ASSERT_EQ(history.size(), 2u);
  EXPECT_EQ(history[0].role(),
            optimization_guide::proto::ChatHistoryTurn::ROLE_USER);
  EXPECT_EQ(history[0].message_content(), "User message");
  EXPECT_EQ(history[1].role(),
            optimization_guide::proto::ChatHistoryTurn::ROLE_ASSISTANT);
  EXPECT_EQ(history[1].message_content(), "Assistant reply");
}

TEST_F(ContextHubServiceTest, ChatHistory_LRUEviction) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      browser::context_hub::mojom::kAutoTabGroups,
      {{features::kMaxTabGroupChatHistoryTurns.name, "3"}});
  ContextHubService service(
      &profile_, identity_test_environment_.identity_manager(),
      &mock_personal_context_service_, &mock_remote_model_executor_,
      &fake_tab_group_sync_service_, &mock_page_content_extraction_service_,
      std::make_unique<InMemoryMemoryBank>(),
      std::make_unique<InMemoryTabGroupStore>(),
      /*context_hub_backend=*/nullptr,
      std::make_unique<InMemoryAutoTodosStore>());

  for (size_t i = 0; i < 4; ++i) {
    service.AddTabGroupChatHistoryTurn(
        optimization_guide::proto::ChatHistoryTurn::ROLE_USER,
        "Message " + base::NumberToString(i));
    task_environment_.FastForwardBy(base::Milliseconds(1));
  }

  auto history = service.GetTabGroupChatHistory();
  ASSERT_EQ(history.size(), 3u);
  // Oldest turn (Message 0) should be evicted.
  EXPECT_EQ(history.front().message_content(), "Message 1");
  EXPECT_EQ(history.back().message_content(), "Message 3");
}

TEST_F(ContextHubServiceTest, ChatHistory_Clear) {
  service_.AddTabGroupChatHistoryTurn(
      optimization_guide::proto::ChatHistoryTurn::ROLE_USER, "Message");
  EXPECT_EQ(service_.GetTabGroupChatHistory().size(), 1u);

  service_.ClearTabGroupChatHistory();
  EXPECT_TRUE(service_.GetTabGroupChatHistory().empty());
}

TEST_F(ContextHubServiceTest, DeleteAllTabGroups) {
  std::vector<TabData> input_tabs = {
      {1, "Tab 1", GURL("https://example1.com")},
      {2, "Tab 2", GURL("https://example2.com")}};

  EXPECT_CALL(
      mock_remote_model_executor_,
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kContextHub, _,
                   _, _))
      .WillOnce([](optimization_guide::ModelBasedCapabilityKey feature,
                   const google::protobuf::MessageLite& request_metadata,
                   const optimization_guide::ModelExecutionOptions& options,
                   optimization_guide::
                       OptimizationGuideModelExecutionResultCallback callback) {
        optimization_guide::proto::ContextHubResponse response;
        optimization_guide::proto::GroupResponse* group_response =
            response.mutable_group_response();
        optimization_guide::proto::TabGroupMinimal* group1 =
            group_response->add_minimal_tab_groups();
        group1->set_label("Group 1");
        group1->add_tab_ids(1);
        group1->add_tab_ids(2);

        optimization_guide::proto::Any any_response;
        any_response.set_type_url(
            "type.googleapis.com/optimization_guide.proto.ContextHubResponse");
        response.SerializeToString(any_response.mutable_value());

        std::move(callback).Run(
            optimization_guide::OptimizationGuideModelExecutionResult(
                base::ok(std::move(any_response)), nullptr),
            nullptr);
      });

  base::test::TestFuture<std::vector<TabGroupEntry>, std::vector<TabData>,
                         std::string>
      future;
  service_.GroupTabs(
      std::move(input_tabs), "",
      future.GetCallback<std::vector<TabGroupEntry>, std::vector<TabData>,
                         std::string>());
  EXPECT_TRUE(future.Wait());

  base::test::TestFuture<std::vector<TabGroupEntry>> stored_groups_future;
  service_.GetTabGroups(stored_groups_future.GetCallback());
  EXPECT_FALSE(stored_groups_future.Get().empty());

  base::test::TestFuture<void> delete_future;
  service_.DeleteAllTabGroups(delete_future.GetCallback());
  EXPECT_TRUE(delete_future.Wait());

  base::test::TestFuture<std::vector<TabGroupEntry>> stored_groups_future2;
  service_.GetTabGroups(stored_groups_future2.GetCallback());
  EXPECT_TRUE(stored_groups_future2.Get().empty());
}

TEST_F(ContextHubServiceTest, ClearTodoFeedbacks) {
  auto feedback = browser::context_hub::mojom::AutoTodoItemFeedback::New();
  feedback->todo_id = "todo_1";
  feedback->liked = true;

  // Add a feedback item.
  service_.SetTodoFeedback(std::move(feedback));
  EXPECT_EQ(1u, service_.GetTodoFeedbacks().size());

  // Clear all feedback items and verify that they are cleared.
  service_.ClearTodoFeedbacks();
  EXPECT_TRUE(service_.GetTodoFeedbacks().empty());
}

TEST_F(ContextHubServiceTest, DeleteTodoFeedback) {
  auto feedback1 = browser::context_hub::mojom::AutoTodoItemFeedback::New();
  feedback1->todo_id = "todo_1";
  feedback1->liked = true;

  auto feedback2 = browser::context_hub::mojom::AutoTodoItemFeedback::New();
  feedback2->todo_id = "todo_2";
  feedback2->liked = false;

  // Add two feedback items.
  service_.SetTodoFeedback(std::move(feedback1));
  service_.SetTodoFeedback(std::move(feedback2));
  EXPECT_EQ(2u, service_.GetTodoFeedbacks().size());

  // Clear one feedback item and verify that it is cleared.
  service_.DeleteTodoFeedback("todo_1");

  auto feedbacks = service_.GetTodoFeedbacks();
  ASSERT_EQ(1u, feedbacks.size());
  EXPECT_EQ("todo_2", feedbacks[0]->todo_id);
  EXPECT_FALSE(feedbacks[0]->liked);
}

TEST_F(ContextHubServiceTest, ExecuteMemoryBankChat_Success) {
  base::test::TestFuture<bool> save_tab_future1;
  service_.SaveMemoryBankEntry(
      MemoryBankEntry(MemoryBankType::kTab, GURL("https://example.com/1"),
                      "Title 1", "Page text 1"),
      save_tab_future1.GetCallback());
  EXPECT_TRUE(save_tab_future1.Get());

  base::test::TestFuture<bool> save_tab_future2;
  service_.SaveMemoryBankEntry(MemoryBankEntry(MemoryBankType::kTextSelection,
                                               GURL("https://example.com/2"),
                                               "Title 2", "Some selected text"),
                               save_tab_future2.GetCallback());
  EXPECT_TRUE(save_tab_future2.Get());

  base::test::TestFuture<std::vector<MemoryBankEntry>> entries_future;
  service_.GetAllEntries(entries_future.GetCallback());
  auto entries = entries_future.Take();
  ASSERT_EQ(entries.size(), 2u);

  std::vector<int64_t> ids = {entries[0].id, entries[1].id};

  EXPECT_CALL(
      mock_remote_model_executor_,
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kContextHub, _,
                   _, _))
      .WillOnce([](optimization_guide::ModelBasedCapabilityKey feature,
                   const google::protobuf::MessageLite& request_metadata,
                   const optimization_guide::ModelExecutionOptions& options,
                   optimization_guide::
                       OptimizationGuideModelExecutionResultCallback callback) {
        const auto& request =
            static_cast<const optimization_guide::proto::ContextHubRequest&>(
                request_metadata);
        EXPECT_EQ(request.request_type(),
                  optimization_guide::proto::
                      CONTEXT_HUB_REQUEST_TYPE_MEMORY_BANK_CHAT);
        EXPECT_EQ(request.user_command(), "summarize these");
        ASSERT_EQ(request.entry_items_size(), 2);

        EXPECT_TRUE(request.entry_items(0).has_memory_bank_entry());
        EXPECT_TRUE(request.entry_items(1).has_memory_bank_entry());

        optimization_guide::proto::ContextHubResponse response;
        response.mutable_memory_bank_chat_response()->set_text_response(
            "This is the LLM summary.");

        optimization_guide::proto::Any any_response;
        any_response.set_type_url(
            "type.googleapis.com/optimization_guide.proto.ContextHubResponse");
        response.SerializeToString(any_response.mutable_value());

        std::move(callback).Run(
            optimization_guide::OptimizationGuideModelExecutionResult(
                base::ok(std::move(any_response)), nullptr),
            nullptr);
      });

  base::test::TestFuture<std::optional<std::string>> future;
  service_.ExecuteMemoryBankChat(ids, "summarize these", future.GetCallback());
  auto result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), "This is the LLM summary.");
}

TEST_F(ContextHubServiceTest, ExecuteMemoryBankChat_Error) {
  EXPECT_CALL(
      mock_remote_model_executor_,
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kContextHub, _,
                   _, _))
      .WillOnce(
          [](optimization_guide::ModelBasedCapabilityKey feature,
             const google::protobuf::MessageLite& request_metadata,
             const optimization_guide::ModelExecutionOptions& options,
             optimization_guide::OptimizationGuideModelExecutionResultCallback
                 callback) {
            std::move(callback).Run(
                optimization_guide::OptimizationGuideModelExecutionResult(),
                nullptr);
          });

  base::test::TestFuture<std::optional<std::string>> future;
  std::vector<int64_t> ids = {100};
  service_.ExecuteMemoryBankChat(ids, "hello", future.GetCallback());
  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(ContextHubServiceTest, AddAndGetMemoryBankChatHistory) {
  service_.AddMemoryBankChatHistoryTurn(
      optimization_guide::proto::ChatHistoryTurn::ROLE_USER, "User query");
  task_environment_.FastForwardBy(base::Milliseconds(1));
  service_.AddMemoryBankChatHistoryTurn(
      optimization_guide::proto::ChatHistoryTurn::ROLE_ASSISTANT,
      "Assistant memory answer");

  auto history = service_.GetMemoryBankChatHistory();
  ASSERT_EQ(history.size(), 2u);
  EXPECT_EQ(history[0].role(),
            optimization_guide::proto::ChatHistoryTurn::ROLE_USER);
  EXPECT_EQ(history[0].message_content(), "User query");
  EXPECT_EQ(history[1].role(),
            optimization_guide::proto::ChatHistoryTurn::ROLE_ASSISTANT);
  EXPECT_EQ(history[1].message_content(), "Assistant memory answer");
}

TEST_F(ContextHubServiceTest, MemoryBankChatHistory_Clear) {
  service_.AddMemoryBankChatHistoryTurn(
      optimization_guide::proto::ChatHistoryTurn::ROLE_USER, "Query");
  EXPECT_EQ(service_.GetMemoryBankChatHistory().size(), 1u);

  service_.ClearMemoryBankChatHistory();
  EXPECT_TRUE(service_.GetMemoryBankChatHistory().empty());
}

TEST_F(ContextHubServiceTest, UpdateAutoTodo) {
  MockServiceObserver observer;
  base::ScopedObservation<ContextHubService, ContextHubService::Observer>
      observation(&observer);
  observation.Observe(&service_);

  AutoTodoEntry entry;
  entry.id = "todo_1";
  entry.title = "Initial Title";
  entry.status = AutoTodoEntry::Status::kActive;

  EXPECT_CALL(
      observer,
      OnAutoTodosChanged(ElementsAre(AllOf(
          Field(&AutoTodoEntry::id, "todo_1"),
          Field(&AutoTodoEntry::title, "Initial Title"),
          Field(&AutoTodoEntry::status, AutoTodoEntry::Status::kActive)))));

  base::test::TestFuture<bool> add_future;
  service_.UpdateAutoTodo(entry, add_future.GetCallback());
  EXPECT_TRUE(add_future.Get());

  AutoTodoEntry updated_entry;
  updated_entry.id = "todo_1";
  updated_entry.title = "Updated Title";
  updated_entry.status = AutoTodoEntry::Status::kCompleted;

  EXPECT_CALL(
      observer,
      OnAutoTodosChanged(ElementsAre(AllOf(
          Field(&AutoTodoEntry::id, "todo_1"),
          Field(&AutoTodoEntry::title, "Updated Title"),
          Field(&AutoTodoEntry::status, AutoTodoEntry::Status::kCompleted)))));

  base::test::TestFuture<bool> update_future;
  service_.UpdateAutoTodo(updated_entry, update_future.GetCallback());
  EXPECT_TRUE(update_future.Get());

  base::test::TestFuture<std::vector<AutoTodoEntry>> get_future;
  service_.GetAutoTodos(get_future.GetCallback());
  auto items = get_future.Get();
  ASSERT_EQ(items.size(), 1u);
  EXPECT_EQ(items[0].id, "todo_1");
  EXPECT_EQ(items[0].title, "Updated Title");
  EXPECT_EQ(items[0].status, AutoTodoEntry::Status::kCompleted);
}

TEST_F(ContextHubServiceTest, DeleteAutoTodoByTabId) {
  MockServiceObserver observer;
  base::ScopedObservation<ContextHubService, ContextHubService::Observer>
      observation(&observer);
  observation.Observe(&service_);

  AutoTodoEntry entry;
  entry.id = "tp_todo_1";
  entry.title = "Tab Todo";
  entry.status = AutoTodoEntry::Status::kActive;
  entry.data = ThirdPartyData{
      .tab_id = 123,
      .group_type = ThirdPartyData::GroupType::kNudgeToClose,
  };

  // Add the auto todo entry to cache.
  base::test::TestFuture<bool> add_future;
  service_.UpdateAutoTodo(entry, add_future.GetCallback());
  EXPECT_TRUE(add_future.Get());

  EXPECT_CALL(observer, OnAutoTodosChanged(IsEmpty()));

  // Delete the auto todo entry by tab id.
  base::test::TestFuture<bool> delete_future;
  service_.DeleteAutoTodoByTabId(123, delete_future.GetCallback());
  EXPECT_TRUE(delete_future.Get());

  // Verify that the auto todo entry is deleted.
  base::test::TestFuture<std::vector<AutoTodoEntry>> get_future;
  service_.GetAutoTodos(get_future.GetCallback());
  EXPECT_TRUE(get_future.Get().empty());
}

TEST_F(ContextHubServiceTest, ClearFirstPartyAutoTodos) {
  AutoTodoEntry fp_entry;
  fp_entry.id = "fp_todo_1";
  fp_entry.title = "Workspace Todo";
  fp_entry.status = AutoTodoEntry::Status::kActive;
  fp_entry.data = FirstPartyData{};

  AutoTodoEntry tp_entry;
  tp_entry.id = "tp_todo_1";
  tp_entry.title = "Browser Todo";
  tp_entry.status = AutoTodoEntry::Status::kActive;
  tp_entry.data = ThirdPartyData{
      .tab_id = 123,
      .group_type = ThirdPartyData::GroupType::kNudgeToClose,
  };

  base::test::TestFuture<bool> add_future1, add_future2;
  service_.UpdateAutoTodo(fp_entry, add_future1.GetCallback());
  EXPECT_TRUE(add_future1.Get());
  service_.UpdateAutoTodo(tp_entry, add_future2.GetCallback());
  EXPECT_TRUE(add_future2.Get());

  base::test::TestFuture<bool> clear_future;
  service_.ClearFirstPartyAutoTodos(clear_future.GetCallback());
  EXPECT_TRUE(clear_future.Get());
  EXPECT_TRUE(service_.GetLastFirstPartyGenerationTime().is_null());

  base::test::TestFuture<std::vector<AutoTodoEntry>> get_future;
  service_.GetAutoTodos(get_future.GetCallback());
  auto items = get_future.Get();
  ASSERT_EQ(items.size(), 1u);
  EXPECT_EQ(items[0].id, "tp_todo_1");
}

TEST_F(ContextHubServiceTest, ClearThirdPartyAutoTodos) {
  AutoTodoEntry fp_entry;
  fp_entry.id = "fp_todo_1";
  fp_entry.title = "Workspace Todo";
  fp_entry.status = AutoTodoEntry::Status::kActive;
  fp_entry.data = FirstPartyData{};

  AutoTodoEntry tp_entry;
  tp_entry.id = "tp_todo_1";
  tp_entry.title = "Browser Todo";
  tp_entry.status = AutoTodoEntry::Status::kActive;
  tp_entry.data = ThirdPartyData{
      .tab_id = 123,
      .group_type = ThirdPartyData::GroupType::kNudgeToClose,
  };

  base::test::TestFuture<bool> add_future1, add_future2;
  service_.UpdateAutoTodo(fp_entry, add_future1.GetCallback());
  EXPECT_TRUE(add_future1.Get());
  service_.UpdateAutoTodo(tp_entry, add_future2.GetCallback());
  EXPECT_TRUE(add_future2.Get());

  base::test::TestFuture<bool> clear_future;
  service_.ClearThirdPartyAutoTodos(clear_future.GetCallback());
  EXPECT_TRUE(clear_future.Get());
  EXPECT_TRUE(service_.GetLastThirdPartyGenerationTime().is_null());

  base::test::TestFuture<std::vector<AutoTodoEntry>> get_future;
  service_.GetAutoTodos(get_future.GetCallback());
  auto items = get_future.Get();
  ASSERT_EQ(items.size(), 1u);
  EXPECT_EQ(items[0].id, "fp_todo_1");
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(ContextHubServiceTest, OnTabStripModelChanged_DeletesTabTodoOnTabClose) {
  MockServiceObserver observer;
  base::ScopedObservation<ContextHubService, ContextHubService::Observer>
      observation(&observer);
  observation.Observe(&service_);

  auto web_contents = CreateEligibleTab();
  SessionID session_id =
      sessions::SessionTabHelper::IdForTab(web_contents.get());

  AutoTodoEntry entry;
  entry.id = "tp_todo_1";
  entry.title = "Tab Todo";
  entry.status = AutoTodoEntry::Status::kActive;
  entry.data = ThirdPartyData{
      .tab_id = session_id.id(),
      .group_type = ThirdPartyData::GroupType::kNudgeToClose,
  };

  base::test::TestFuture<bool> add_future;
  service_.UpdateAutoTodo(entry, add_future.GetCallback());
  EXPECT_TRUE(add_future.Get());

  EXPECT_CALL(observer, OnAutoTodosChanged(IsEmpty()));

  // Simulate tab removal via TabStripModelChange::kRemoved with
  // TabRemovedReason::kDeleted. Set session_id to a different historical
  // restore entry ID to verify the actual tab WebContents ID is used.
  TabStripModelChange::Remove remove;
  TabStripModelChange::RemovedTab removed_tab(
      /*tab=*/nullptr, /*index=*/0, TabRemovedReason::kDeleted,
      tabs::TabInterface::DetachReason::kDelete,
      SessionID::FromSerializedValue(99999));
  removed_tab.contents = web_contents.get();
  remove.contents.push_back(std::move(removed_tab));
  TabStripModelChange change(std::move(remove));
  TabStripSelectionChange selection;

  service_.OnTabStripModelChanged(/*tab_strip_model=*/nullptr, change,
                                  selection);

  base::test::TestFuture<std::vector<AutoTodoEntry>> get_future;
  service_.GetAutoTodos(get_future.GetCallback());
  EXPECT_TRUE(get_future.Get().empty());
}

TEST_F(ContextHubServiceTest,
       OnTabStripModelChanged_DeletesTabTodoUsingWebContents) {
  MockServiceObserver observer;
  base::ScopedObservation<ContextHubService, ContextHubService::Observer>
      observation(&observer);
  observation.Observe(&service_);

  auto web_contents = CreateEligibleTab();
  SessionID session_id =
      sessions::SessionTabHelper::IdForTab(web_contents.get());

  AutoTodoEntry entry;
  entry.id = "tp_todo_1";
  entry.title = "Tab Todo";
  entry.status = AutoTodoEntry::Status::kActive;
  entry.data = ThirdPartyData{
      .tab_id = session_id.id(),
      .group_type = ThirdPartyData::GroupType::kNudgeToClose,
  };

  base::test::TestFuture<bool> add_future;
  service_.UpdateAutoTodo(entry, add_future.GetCallback());
  EXPECT_TRUE(add_future.Get());

  EXPECT_CALL(observer, OnAutoTodosChanged(IsEmpty()));

  // Simulate tab removal with web_contents and no session_id provided directly.
  TabStripModelChange::Remove remove;
  TabStripModelChange::RemovedTab removed_tab(
      /*tab=*/nullptr, /*index=*/0, TabRemovedReason::kDeleted,
      tabs::TabInterface::DetachReason::kDelete, std::nullopt);
  removed_tab.contents = web_contents.get();
  remove.contents.push_back(std::move(removed_tab));
  TabStripModelChange change(std::move(remove));
  TabStripSelectionChange selection;

  service_.OnTabStripModelChanged(/*tab_strip_model=*/nullptr, change,
                                  selection);

  base::test::TestFuture<std::vector<AutoTodoEntry>> get_future;
  service_.GetAutoTodos(get_future.GetCallback());
  EXPECT_TRUE(get_future.Get().empty());
}

TEST_F(ContextHubServiceTest,
       OnTabStripModelChanged_IgnoresTabMovedToAnotherWindow) {
  AutoTodoEntry entry;
  entry.id = "tp_todo_1";
  entry.title = "Tab Todo";
  entry.status = AutoTodoEntry::Status::kActive;
  entry.data = ThirdPartyData{
      .tab_id = 123,
      .group_type = ThirdPartyData::GroupType::kNudgeToClose,
  };

  base::test::TestFuture<bool> add_future;
  service_.UpdateAutoTodo(entry, add_future.GetCallback());
  EXPECT_TRUE(add_future.Get());

  // Simulate tab moved to another tab strip (kInsertedIntoOtherTabStrip).
  TabStripModelChange::Remove remove;
  TabStripModelChange::RemovedTab removed_tab(
      /*tab=*/nullptr, /*index=*/0,
      TabRemovedReason::kInsertedIntoOtherTabStrip,
      tabs::TabInterface::DetachReason::kInsertIntoOtherWindow,
      SessionID::FromSerializedValue(123));
  remove.contents.push_back(std::move(removed_tab));
  TabStripModelChange change(std::move(remove));
  TabStripSelectionChange selection;

  service_.OnTabStripModelChanged(/*tab_strip_model=*/nullptr, change,
                                  selection);

  // Verify that the todo was NOT deleted.
  base::test::TestFuture<std::vector<AutoTodoEntry>> get_future;
  service_.GetAutoTodos(get_future.GetCallback());
  EXPECT_EQ(1u, get_future.Get().size());
}
#endif

TEST_F(ContextHubServiceTest, GetAutoTodos) {
  base::test::TestFuture<std::vector<AutoTodoEntry>> get_empty_future;
  service_.GetAutoTodos(get_empty_future.GetCallback());
  EXPECT_TRUE(get_empty_future.Get().empty());

  AutoTodoEntry entry1;
  entry1.id = "todo_1";
  entry1.title = "Todo 1";
  entry1.description = "Description 1";
  entry1.status = AutoTodoEntry::Status::kActive;
  entry1.importance_score = 0.8f;
  entry1.data = FirstPartyData{
      .source_references = {{.url = GURL("https://mail.google.com/1"),
                             .subject = "Subject 1"}},
      .actionable_url = GURL("https://example.com/1"),
  };

  AutoTodoEntry entry2;
  entry2.id = "todo_2";
  entry2.title = "Todo 2";
  entry2.description = "Description 2";
  entry2.status = AutoTodoEntry::Status::kCompleted;
  entry2.importance_score = 0.5f;

  base::test::TestFuture<bool> update_future1;
  service_.UpdateAutoTodo(entry1, update_future1.GetCallback());
  EXPECT_TRUE(update_future1.Get());

  base::test::TestFuture<bool> update_future2;
  service_.UpdateAutoTodo(entry2, update_future2.GetCallback());
  EXPECT_TRUE(update_future2.Get());

  base::test::TestFuture<std::vector<AutoTodoEntry>> get_future;
  service_.GetAutoTodos(get_future.GetCallback());
  auto items = get_future.Get();
  ASSERT_EQ(items.size(), 2u);

  EXPECT_EQ(items[0].id, "todo_2");
  EXPECT_EQ(items[0].title, "Todo 2");
  EXPECT_EQ(items[0].description, "Description 2");
  EXPECT_EQ(items[0].status, AutoTodoEntry::Status::kCompleted);
  EXPECT_FLOAT_EQ(items[0].importance_score, 0.5f);

  EXPECT_EQ(items[1].id, "todo_1");
  EXPECT_EQ(items[1].title, "Todo 1");
  EXPECT_EQ(items[1].description, "Description 1");
  EXPECT_EQ(items[1].status, AutoTodoEntry::Status::kActive);
  EXPECT_FLOAT_EQ(items[1].importance_score, 0.8f);
  EXPECT_TRUE(items[1].is_first_party());
}

TEST_F(ContextHubServiceTest, ConfirmAllTabGroups_EmptyGroups) {
  base::test::TestFuture<bool, std::vector<base::Uuid>> future;
  service_.ConfirmAllTabGroups(future.GetCallback());
  auto [success, added_group_guids] = future.Take();
  EXPECT_TRUE(success);
  EXPECT_TRUE(added_group_guids.empty());
}

TEST_F(ContextHubServiceTest, GetConfirmedTabGroups) {
  // 1. Open group with an open tab (should be included).
  tab_groups::SavedTabGroup open_group(u"Open Group",
                                       tab_groups::TabGroupColorId::kBlue, {},
                                       /*position=*/std::nullopt);
  open_group.SetLocalGroupId(tab_groups::test::GenerateRandomTabGroupID());
  tab_groups::SavedTabGroupTab open_tab(
      GURL("https://example.com"), u"Example", open_group.saved_guid(),
      /*position=*/0, /*saved_tab_guid=*/std::nullopt, /*local_tab_id=*/1);
  open_group.AddTabLocally(open_tab);
  fake_tab_group_sync_service_.AddGroup(open_group);

  // 2. Closed group without local_group_id (should be excluded).
  tab_groups::SavedTabGroup closed_group(u"Closed Group",
                                         tab_groups::TabGroupColorId::kRed, {},
                                         /*position=*/std::nullopt);
  tab_groups::SavedTabGroupTab closed_tab(
      GURL("https://example2.com"), u"Example 2", closed_group.saved_guid(),
      /*position=*/0, /*saved_tab_guid=*/std::nullopt, /*local_tab_id=*/2);
  closed_group.AddTabLocally(closed_tab);
  fake_tab_group_sync_service_.AddGroup(closed_group);

  // 3. Group with local_group_id but no open tabs (should be excluded).
  tab_groups::SavedTabGroup no_open_tabs_group(
      u"Group Without Open Tabs", tab_groups::TabGroupColorId::kGreen, {},
      /*position=*/std::nullopt);
  no_open_tabs_group.SetLocalGroupId(
      tab_groups::test::GenerateRandomTabGroupID());
  tab_groups::SavedTabGroupTab unopened_tab(
      GURL("https://example3.com"), u"Example 3",
      no_open_tabs_group.saved_guid(), /*position=*/0);
  no_open_tabs_group.AddTabLocally(unopened_tab);
  fake_tab_group_sync_service_.AddGroup(no_open_tabs_group);

  std::vector<TabGroupEntry> groups = service_.GetConfirmedTabGroups();
  ASSERT_EQ(groups.size(), 1u);
  EXPECT_EQ(groups[0].id, open_group.saved_guid().AsLowercaseString());
  EXPECT_EQ(groups[0].label, "Open Group");
  ASSERT_EQ(groups[0].tabs.size(), 1u);
  EXPECT_EQ(groups[0].tabs[0].id, 1);
  EXPECT_EQ(groups[0].tabs[0].title, "Example");
  EXPECT_EQ(groups[0].tabs[0].url, GURL("https://example.com"));

  std::optional<TabGroupEntry> fetched_group =
      service_.GetConfirmedTabGroup(open_group.saved_guid());
  ASSERT_TRUE(fetched_group.has_value());
  EXPECT_EQ(fetched_group->id, open_group.saved_guid().AsLowercaseString());
  EXPECT_EQ(fetched_group->tabs.size(), 1u);
}

TEST_F(ContextHubServiceTest, ConfirmAllTabGroups_Success) {
  std::vector<TabData> input_tabs = {
      {1, "Tab 1", GURL("https://example1.com")},
      {2, "Tab 2", GURL("https://example2.com")}};

  EXPECT_CALL(
      mock_remote_model_executor_,
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kContextHub, _,
                   _, _))
      .WillOnce([](optimization_guide::ModelBasedCapabilityKey feature,
                   const google::protobuf::MessageLite& request_metadata,
                   const optimization_guide::ModelExecutionOptions& options,
                   optimization_guide::
                       OptimizationGuideModelExecutionResultCallback callback) {
        optimization_guide::proto::ContextHubResponse response;
        optimization_guide::proto::GroupResponse* group_response =
            response.mutable_group_response();
        optimization_guide::proto::TabGroupMinimal* group1 =
            group_response->add_minimal_tab_groups();
        group1->set_label("Unconfirmed Group");
        group1->add_tab_ids(1);
        group1->add_tab_ids(2);

        optimization_guide::proto::Any any_response;
        any_response.set_type_url(
            "type.googleapis.com/optimization_guide.proto.ContextHubResponse");
        response.SerializeToString(any_response.mutable_value());

        std::move(callback).Run(
            optimization_guide::OptimizationGuideModelExecutionResult(
                base::ok(std::move(any_response)), nullptr),
            nullptr);
      });

  base::test::TestFuture<std::vector<TabGroupEntry>, std::vector<TabData>,
                         std::string>
      group_future;
  service_.GroupTabs(
      std::move(input_tabs), "",
      group_future.GetCallback<std::vector<TabGroupEntry>, std::vector<TabData>,
                               std::string>());
  EXPECT_TRUE(group_future.Wait());

  base::test::TestFuture<bool, std::vector<base::Uuid>> confirm_future;
  service_.ConfirmAllTabGroups(confirm_future.GetCallback());
  auto [success, added_guids] = confirm_future.Take();

  EXPECT_TRUE(success);
  ASSERT_EQ(added_guids.size(), 1u);

  // Verify group is now in sync service
  auto confirmed_groups = fake_tab_group_sync_service_.GetAllGroups();
  ASSERT_EQ(confirmed_groups.size(), 1u);
  EXPECT_EQ(confirmed_groups[0].saved_guid(), added_guids[0]);

  // Verify store is cleared
  base::test::TestFuture<std::vector<TabGroupEntry>> stored_groups_future;
  service_.GetTabGroups(stored_groups_future.GetCallback());
  EXPECT_TRUE(stored_groups_future.Get().empty());
}

TEST_F(ContextHubServiceTest, TabGroupStore_Null) {
  ContextHubService service_null_store(
      &profile_, identity_test_environment_.identity_manager(),
      &mock_personal_context_service_, &mock_remote_model_executor_,
      &fake_tab_group_sync_service_, &mock_page_content_extraction_service_,
      std::make_unique<InMemoryMemoryBank>(),
      /*tab_group_store=*/nullptr,
      /*context_hub_backend=*/nullptr,
      std::make_unique<InMemoryAutoTodosStore>());

  base::test::TestFuture<std::vector<TabGroupEntry>> get_future;
  service_null_store.GetTabGroups(get_future.GetCallback());
  EXPECT_TRUE(get_future.Get().empty());

  base::test::TestFuture<void> delete_future;
  service_null_store.DeleteAllTabGroups(delete_future.GetCallback());
  EXPECT_TRUE(delete_future.Wait());

  base::test::TestFuture<bool, std::vector<base::Uuid>> confirm_future;
  service_null_store.ConfirmAllTabGroups(confirm_future.GetCallback());
  auto [success, added_guids] = confirm_future.Take();
  EXPECT_FALSE(success);
  EXPECT_TRUE(added_guids.empty());
}

TEST_F(ContextHubServiceTest, GetConfirmedTabGroup_NotFound) {
  EXPECT_FALSE(service_.GetConfirmedTabGroup(base::Uuid::GenerateRandomV4())
                   .has_value());
}

TEST_F(ContextHubServiceTest, GetLocalGroupIdForConfirmedGroup) {
  tab_groups::SavedTabGroup group(u"Test Group",
                                  tab_groups::TabGroupColorId::kBlue, {},
                                  /*position=*/std::nullopt);
  tab_groups::LocalTabGroupID local_id =
      tab_groups::test::GenerateRandomTabGroupID();
  group.SetLocalGroupId(local_id);
  fake_tab_group_sync_service_.AddGroup(group);

  auto result = service_.GetLocalGroupIdForConfirmedGroup(group.saved_guid());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), local_id);

  EXPECT_FALSE(
      service_.GetLocalGroupIdForConfirmedGroup(base::Uuid::GenerateRandomV4())
          .has_value());
}

TEST_F(ContextHubServiceTest, RemoveConfirmedTabGroup) {
  tab_groups::SavedTabGroup group(u"Test Group",
                                  tab_groups::TabGroupColorId::kBlue, {},
                                  /*position=*/std::nullopt);
  fake_tab_group_sync_service_.AddGroup(group);

  EXPECT_TRUE(service_.RemoveConfirmedTabGroup(group.saved_guid()));
  EXPECT_TRUE(fake_tab_group_sync_service_.GetAllGroups().empty());

  EXPECT_FALSE(
      service_.RemoveConfirmedTabGroup(base::Uuid::GenerateRandomV4()));
}

TEST_F(ContextHubServiceTest, RemoveAllConfirmedTabGroups) {
  tab_groups::SavedTabGroup group1(u"Group 1",
                                   tab_groups::TabGroupColorId::kBlue, {},
                                   /*position=*/std::nullopt);
  tab_groups::SavedTabGroup group2(u"Group 2",
                                   tab_groups::TabGroupColorId::kRed, {},
                                   /*position=*/std::nullopt);
  fake_tab_group_sync_service_.AddGroup(group1);
  fake_tab_group_sync_service_.AddGroup(group2);
  ASSERT_EQ(fake_tab_group_sync_service_.GetAllGroups().size(), 2u);

  EXPECT_TRUE(service_.RemoveAllConfirmedTabGroups());
  EXPECT_TRUE(fake_tab_group_sync_service_.GetAllGroups().empty());
}

TEST_F(ContextHubServiceTest, ConnectLocalTabGroup) {
  tab_groups::SavedTabGroup group(u"Test Group",
                                  tab_groups::TabGroupColorId::kBlue, {},
                                  /*position=*/std::nullopt);
  fake_tab_group_sync_service_.AddGroup(group);

  tab_groups::LocalTabGroupID local_id =
      tab_groups::test::GenerateRandomTabGroupID();
  service_.ConnectLocalTabGroup(group.saved_guid(), local_id);

  auto updated_group = service_.GetConfirmedTabGroup(group.saved_guid());
  ASSERT_TRUE(updated_group.has_value());
  EXPECT_EQ(service_.GetLocalGroupIdForConfirmedGroup(group.saved_guid()),
            local_id);
}

TEST_F(ContextHubServiceTest, AutoTodos_TriggersOnStartupWhenTokensLoaded) {
  identity_test_environment_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);

  personal_context::MockPersonalContextService mock_personal_context_service;
  EXPECT_CALL(
      mock_personal_context_service,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _));

  ContextHubService service(
      &profile_, identity_test_environment_.identity_manager(),
      &mock_personal_context_service, &mock_remote_model_executor_,
      &fake_tab_group_sync_service_, &mock_page_content_extraction_service_,
      std::make_unique<InMemoryMemoryBank>(),
      std::make_unique<InMemoryTabGroupStore>(),
      /*context_hub_backend=*/nullptr,
      std::make_unique<InMemoryAutoTodosStore>());
}

TEST_F(ContextHubServiceTest,
       AutoTodos_DoesNotTriggerOnStartupWhenNotSignedIn) {
  personal_context::MockPersonalContextService mock_personal_context_service;
  EXPECT_CALL(
      mock_personal_context_service,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .Times(0);

  ContextHubService service(
      &profile_, identity_test_environment_.identity_manager(),
      &mock_personal_context_service, &mock_remote_model_executor_,
      &fake_tab_group_sync_service_, &mock_page_content_extraction_service_,
      std::make_unique<InMemoryMemoryBank>(),
      std::make_unique<InMemoryTabGroupStore>(),
      /*context_hub_backend=*/nullptr,
      std::make_unique<InMemoryAutoTodosStore>());
}

TEST_F(ContextHubServiceTest, AutoTodos_TriggersWhenPrimaryAccountSignedIn) {
  personal_context::MockPersonalContextService mock_personal_context_service;
  EXPECT_CALL(
      mock_personal_context_service,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .Times(0);

  ContextHubService service(
      &profile_, identity_test_environment_.identity_manager(),
      &mock_personal_context_service, &mock_remote_model_executor_,
      &fake_tab_group_sync_service_, &mock_page_content_extraction_service_,
      std::make_unique<InMemoryMemoryBank>(),
      std::make_unique<InMemoryTabGroupStore>(),
      /*context_hub_backend=*/nullptr,
      std::make_unique<InMemoryAutoTodosStore>());

  // When the user signs in, generation is triggered.
  EXPECT_CALL(
      mock_personal_context_service,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _));
  identity_test_environment_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
}

TEST_F(ContextHubServiceTest, AutoTodos_TriggersWhenRefreshTokensLoaded) {
  AccountInfo account_info =
      identity_test_environment_.MakeAccountAvailable("test@example.com");
  identity_test_environment_.SetPrimaryAccount(account_info.email,
                                               signin::ConsentLevel::kSignin);
  identity_test_environment_.ResetToAccountsNotYetLoadedFromDiskState();

  personal_context::MockPersonalContextService mock_personal_context_service;
  EXPECT_CALL(
      mock_personal_context_service,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .Times(0);

  ContextHubService service(
      &profile_, identity_test_environment_.identity_manager(),
      &mock_personal_context_service, &mock_remote_model_executor_,
      &fake_tab_group_sync_service_, &mock_page_content_extraction_service_,
      std::make_unique<InMemoryMemoryBank>(),
      std::make_unique<InMemoryTabGroupStore>(),
      /*context_hub_backend=*/nullptr,
      std::make_unique<InMemoryAutoTodosStore>());

  // When refresh tokens finish loading from disk, generation is triggered.
  EXPECT_CALL(
      mock_personal_context_service,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _));
  identity_test_environment_.ReloadAccountsFromDisk();
}

TEST_F(ContextHubServiceTest, AutoTodos_DoesNotRunWhenFeatureDisabled) {
  identity_test_environment_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);

  personal_context::MockPersonalContextService mock_personal_context_service;
  EXPECT_CALL(
      mock_personal_context_service,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .Times(0);

  ContextHubService service(
      &profile_, identity_test_environment_.identity_manager(),
      &mock_personal_context_service, &mock_remote_model_executor_,
      &fake_tab_group_sync_service_, &mock_page_content_extraction_service_,
      std::make_unique<InMemoryMemoryBank>(),
      std::make_unique<InMemoryTabGroupStore>(),
      /*context_hub_backend=*/nullptr,
      /*auto_todos_store=*/nullptr);
}

TEST_F(ContextHubServiceTest,
       AutoTodos_PeriodicTimer_TriggersAfterIntervalElapsed) {
  identity_test_environment_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);

  personal_context::proto::AutoTodosResponse expected_response;
  personal_context::proto::Any any_response;
  expected_response.SerializeToString(any_response.mutable_value());

  personal_context::MockPersonalContextService mock_personal_context_service;
  // Triggers once on startup and completes.
  EXPECT_CALL(
      mock_personal_context_service,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .WillOnce(RunOnceCallback<3>(
          personal_context::FetchContextResult(base::ok(any_response))));

  ContextHubService service(
      &profile_, identity_test_environment_.identity_manager(),
      &mock_personal_context_service, &mock_remote_model_executor_,
      &fake_tab_group_sync_service_, &mock_page_content_extraction_service_,
      std::make_unique<InMemoryMemoryBank>(),
      std::make_unique<InMemoryTabGroupStore>(),
      /*context_hub_backend=*/nullptr,
      std::make_unique<InMemoryAutoTodosStore>());

  // 12 hours later: should not trigger yet.
  task_environment_.FastForwardBy(base::Hours(12));

  // Next 12 hours (total 24h interval): should trigger again.
  EXPECT_CALL(
      mock_personal_context_service,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _));

  task_environment_.FastForwardBy(base::Hours(12));
}

TEST_F(ContextHubServiceTest, AutoTodos_ManualGenerationResetsPeriodicTimer) {
  identity_test_environment_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);

  personal_context::proto::AutoTodosResponse expected_response;
  personal_context::proto::Any any_response;
  expected_response.SerializeToString(any_response.mutable_value());

  personal_context::MockPersonalContextService mock_personal_context_service;
  // Triggers once on startup and completes.
  EXPECT_CALL(
      mock_personal_context_service,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .WillOnce(RunOnceCallback<3>(
          personal_context::FetchContextResult(base::ok(any_response))));

  ContextHubService service(
      &profile_, identity_test_environment_.identity_manager(),
      &mock_personal_context_service, &mock_remote_model_executor_,
      &fake_tab_group_sync_service_, &mock_page_content_extraction_service_,
      std::make_unique<InMemoryMemoryBank>(),
      std::make_unique<InMemoryTabGroupStore>(),
      /*context_hub_backend=*/nullptr,
      std::make_unique<InMemoryAutoTodosStore>());

  // Advance to 2 hours before the 24-hour periodic timer would trigger (22
  // hours after startup).
  task_environment_.FastForwardBy(base::Hours(22));

  // Trigger manual generation.
  EXPECT_CALL(
      mock_personal_context_service,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .WillOnce(RunOnceCallback<3>(
          personal_context::FetchContextResult(base::ok(any_response))));

  base::test::TestFuture<bool> future;
  service.GenerateFirstPartyAutoTodos(future.GetCallback());
  EXPECT_TRUE(future.Get());

  // Fast-forward 2 hours (24 hours after startup). Because manual generation
  // reset the periodic timer, the timer should NOT trigger here.
  EXPECT_CALL(
      mock_personal_context_service,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .Times(0);
  task_environment_.FastForwardBy(base::Hours(2));

  // Fast-forward another 22 hours (24 hours after manual generation). The
  // periodic timer should now trigger.
  EXPECT_CALL(
      mock_personal_context_service,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _));

  task_environment_.FastForwardBy(base::Hours(22));
}

TEST_F(ContextHubServiceTest,
       AutoTodos_PowerResume_TriggersWhenIntervalElapsed) {
  base::test::ScopedPowerMonitorTestSource power_monitor_source;
  identity_test_environment_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);

  personal_context::proto::AutoTodosResponse expected_response;
  personal_context::proto::Any any_response;
  expected_response.SerializeToString(any_response.mutable_value());

  personal_context::MockPersonalContextService mock_personal_context_service;
  // Triggers once on startup and completes.
  EXPECT_CALL(
      mock_personal_context_service,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .WillOnce(RunOnceCallback<3>(
          personal_context::FetchContextResult(base::ok(any_response))));

  ContextHubService service(
      &profile_, identity_test_environment_.identity_manager(),
      &mock_personal_context_service, &mock_remote_model_executor_,
      &fake_tab_group_sync_service_, &mock_page_content_extraction_service_,
      std::make_unique<InMemoryMemoryBank>(),
      std::make_unique<InMemoryTabGroupStore>(),
      /*context_hub_backend=*/nullptr,
      std::make_unique<InMemoryAutoTodosStore>());

  // 10 hours of uptime.
  task_environment_.FastForwardBy(base::Hours(10));

  // Machine suspends.
  power_monitor_source.GenerateSuspendEvent();

  // 15 hours pass while suspended (total 25 hours wall-clock time since last
  // generation).
  task_environment_.AdvanceClock(base::Hours(15));

  // Machine resumes. Since >= 24 hours have elapsed, generation should trigger.
  EXPECT_CALL(
      mock_personal_context_service,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .WillOnce(RunOnceCallback<3>(
          personal_context::FetchContextResult(base::ok(any_response))));

  power_monitor_source.GenerateResumeEvent();
}

TEST_F(ContextHubServiceTest,
       AutoTodos_PowerResume_AdjustsTimerWhenIntervalNotElapsed) {
  base::test::ScopedPowerMonitorTestSource power_monitor_source;

  identity_test_environment_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);

  personal_context::proto::AutoTodosResponse expected_response;
  personal_context::proto::Any any_response;
  expected_response.SerializeToString(any_response.mutable_value());

  personal_context::MockPersonalContextService mock_personal_context_service;
  // Triggers once on startup and completes.
  EXPECT_CALL(
      mock_personal_context_service,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .WillOnce(RunOnceCallback<3>(
          personal_context::FetchContextResult(base::ok(any_response))));

  ContextHubService service(
      &profile_, identity_test_environment_.identity_manager(),
      &mock_personal_context_service, &mock_remote_model_executor_,
      &fake_tab_group_sync_service_, &mock_page_content_extraction_service_,
      std::make_unique<InMemoryMemoryBank>(),
      std::make_unique<InMemoryTabGroupStore>(),
      /*context_hub_backend=*/nullptr,
      std::make_unique<InMemoryAutoTodosStore>());

  // 6 hours of uptime.
  task_environment_.FastForwardBy(base::Hours(6));

  // Machine suspends and sleeps for 10 hours.
  power_monitor_source.GenerateSuspendEvent();
  task_environment_.AdvanceClock(base::Hours(10));

  // Machine resumes. 16 hours elapsed in total (< 24 hours).
  // It should NOT trigger generation immediately, but adjust the timer to fire
  // in 8 hours (24 - 16).
  EXPECT_CALL(
      mock_personal_context_service,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .Times(0);

  power_monitor_source.GenerateResumeEvent();

  // Fast forward 7 hours (23 hours total wall-clock time). Still shouldn't
  // trigger.
  task_environment_.FastForwardBy(base::Hours(7));

  // Fast forward 1 more hour (24 hours total wall-clock time). Timer fires.
  EXPECT_CALL(
      mock_personal_context_service,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .WillOnce(RunOnceCallback<3>(
          personal_context::FetchContextResult(base::ok(any_response))));

  task_environment_.FastForwardBy(base::Hours(1));

  // Verify that after successful generation, the timer is restored to the full
  // 24-hour interval rather than repeating the shortened 8-hour delay.
  EXPECT_CALL(
      mock_personal_context_service,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .Times(0);
  task_environment_.FastForwardBy(base::Hours(8));

  // At +24 hours after generation (8h + 16h), the timer fires again.
  EXPECT_CALL(
      mock_personal_context_service,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _));
  task_environment_.FastForwardBy(base::Hours(16));
}

TEST_F(ContextHubServiceTest,
       AutoTodos_DoesNotTriggerRepeatedlyInSameSessionWhenRecent) {
  identity_test_environment_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);

  personal_context::proto::AutoTodosResponse expected_response;
  personal_context::proto::Any any_response;
  expected_response.SerializeToString(any_response.mutable_value());

  personal_context::MockPersonalContextService mock_personal_context_service;
  // Triggers once on startup and finishes.
  EXPECT_CALL(
      mock_personal_context_service,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .WillOnce(RunOnceCallback<3>(
          personal_context::FetchContextResult(base::ok(any_response))));

  ContextHubService service(
      &profile_, identity_test_environment_.identity_manager(),
      &mock_personal_context_service, &mock_remote_model_executor_,
      &fake_tab_group_sync_service_, &mock_page_content_extraction_service_,
      std::make_unique<InMemoryMemoryBank>(),
      std::make_unique<InMemoryTabGroupStore>(),
      /*context_hub_backend=*/nullptr,
      std::make_unique<InMemoryAutoTodosStore>());

  // Advance time by only 10 minutes.
  task_environment_.FastForwardBy(base::Minutes(10));

  // Simulating token reloads or auth error updates within the same session
  // should not trigger redundant generation.
  EXPECT_CALL(
      mock_personal_context_service,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .Times(0);

  identity_test_environment_.ReloadAccountsFromDisk();
}

TEST_F(ContextHubServiceTest, PendingMemoryBankEntryLifecycle) {
  ContextHubService service(
      &profile_, identity_test_environment_.identity_manager(),
      &mock_personal_context_service_, &mock_remote_model_executor_,
      &fake_tab_group_sync_service_, &mock_page_content_extraction_service_,
      std::make_unique<InMemoryMemoryBank>(),
      std::make_unique<InMemoryTabGroupStore>(),
      /*context_hub_backend=*/nullptr,
      std::make_unique<InMemoryAutoTodosStore>());

  // When no pending entry is set, GetPendingMemoryBankEntry returns nullopt.
  EXPECT_FALSE(service.GetPendingMemoryBankEntry().has_value());

  MemoryBankEntry pending(MemoryBankType::kTextSelection,
                          GURL("https://example.com/pending"), "Pending Title",
                          "Pending selected text");

  service.SetPendingMemoryBankEntry(std::move(pending));

  auto fetched = service.GetPendingMemoryBankEntry();
  ASSERT_TRUE(fetched.has_value());
  EXPECT_EQ(fetched->type, MemoryBankType::kTextSelection);
  EXPECT_EQ(fetched->url, GURL("https://example.com/pending"));
  EXPECT_EQ(fetched->tab_title, "Pending Title");
  EXPECT_EQ(fetched->selected_text, "Pending selected text");

  // Save pending entry with tags, note, and collection.
  bool saved = service.SavePendingMemoryBankEntry(
      {"tag1", "tag2"}, "Pending Note", "Pending Collection");
  EXPECT_TRUE(saved);

  // After saving, the pending entry should no longer exist.
  EXPECT_FALSE(service.GetPendingMemoryBankEntry().has_value());

  // Verify the entry was committed to the memory bank.
  base::test::TestFuture<std::vector<MemoryBankEntry>> entries_future;
  service.GetAllEntries(entries_future.GetCallback());
  auto entries = entries_future.Take();
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0].url, GURL("https://example.com/pending"));
  EXPECT_EQ(entries[0].tab_title, "Pending Title");
  EXPECT_EQ(entries[0].selected_text, "Pending selected text");
  EXPECT_THAT(entries[0].tags, UnorderedElementsAre("tag1", "tag2"));
  EXPECT_EQ(entries[0].note, "Pending Note");
  EXPECT_EQ(entries[0].collection, "Pending Collection");
}

TEST_F(ContextHubServiceTest,
       PendingMemoryBankEntryLifecycleDefaultParameters) {
  ContextHubService service(
      &profile_, identity_test_environment_.identity_manager(),
      &mock_personal_context_service_, &mock_remote_model_executor_,
      &fake_tab_group_sync_service_, &mock_page_content_extraction_service_,
      std::make_unique<InMemoryMemoryBank>(),
      std::make_unique<InMemoryTabGroupStore>(),
      /*context_hub_backend=*/nullptr,
      std::make_unique<InMemoryAutoTodosStore>());

  MemoryBankEntry pending(MemoryBankType::kTextSelection,
                          GURL("https://example.com/pending"), "Pending Title",
                          "Pending selected text");
  service.SetPendingMemoryBankEntry(std::move(pending));

  // Save pending entry using default parameters (no tags, note, or collection).
  bool saved = service.SavePendingMemoryBankEntry();
  EXPECT_TRUE(saved);
  EXPECT_FALSE(service.GetPendingMemoryBankEntry().has_value());

  base::test::TestFuture<std::vector<MemoryBankEntry>> entries_future;
  service.GetAllEntries(entries_future.GetCallback());
  auto entries = entries_future.Take();
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0].url, GURL("https://example.com/pending"));
  EXPECT_TRUE(entries[0].tags.empty());
  EXPECT_FALSE(entries[0].note.has_value());
  EXPECT_FALSE(entries[0].collection.has_value());
}

}  // namespace
}  // namespace context_hub
