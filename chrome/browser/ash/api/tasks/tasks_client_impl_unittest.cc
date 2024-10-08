// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/api/tasks/tasks_client_impl.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/api/tasks/tasks_types.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/web_app_id_constants.h"
#include "ash/glanceables/glanceables_metrics.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/services/app_service/public/cpp/app.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/dummy_auth_service.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/common/time_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/gaia_urls_overrider_for_testing.h"
#include "google_apis/tasks/tasks_api_requests.h"
#include "google_apis/tasks/tasks_api_task_status.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/list_model.h"
#include "ui/base/models/list_model_observer.h"

namespace ash::api {
namespace {

using ::base::test::TestFuture;
using ::google_apis::ApiErrorCode;
using ::google_apis::util::FormatTimeAsString;
using ::net::test_server::BasicHttpResponse;
using ::net::test_server::HttpMethod;
using ::net::test_server::HttpRequest;
using ::net::test_server::HttpResponse;
using ::testing::_;
using ::testing::ByMove;
using ::testing::Eq;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::Not;
using ::testing::Return;

constexpr char kDefaultTaskListsResponseContent[] = R"(
    {
      "kind": "tasks#taskLists",
      "items": [
        {
          "id": "qwerty",
          "title": "My Tasks 1",
          "updated": "2023-01-30T22:19:22.812Z"
        },
        {
          "id": "asdfgh",
          "title": "My Tasks 2",
          "updated": "2022-12-21T23:38:22.590Z"
        }
      ]
    }
  )";

constexpr char kDefaultTasksResponseContent[] = R"(
    {
      "kind": "tasks#tasks",
      "items": [
        {
          "id": "asd",
          "title": "Parent task, level 1",
          "status": "needsAction",
          "due": "2023-04-19T00:00:00.000Z",
          "updated": "2023-01-30T22:19:22.812Z",
          "webViewLink": "https://tasks.google.com/task/id1"
        },
        {
          "id": "qwe",
          "title": "Child task, level 2",
          "parent": "asd",
          "status": "needsAction",
          "updated": "2022-12-21T23:38:22.590Z",
          "webViewLink": "https://tasks.google.com/task/id2"
        },
        {
          "id": "zxc",
          "title": "Parent task 2, level 1",
          "status": "needsAction",
          "links": [{"type": "email"}],
          "notes": "Lorem ipsum dolor sit amet",
          "updated": "2022-12-21T23:38:22.590Z",
          "webViewLink": "https://tasks.google.com/task/id3"
        }
      ]
    }
  )";

using TaskListsFuture = TestFuture<bool,
                                   std::optional<ApiErrorCode>,
                                   const ui::ListModel<TaskList>*>;

using TasksFuture =
    TestFuture<bool, std::optional<ApiErrorCode>, const ui::ListModel<Task>*>;

// Helper class to simplify mocking `net::EmbeddedTestServer` responses,
// especially useful for subsequent responses when testing pagination logic.
class TestRequestHandler {
 public:
  static std::unique_ptr<HttpResponse> CreateSuccessfulResponse(
      const std::string& content) {
    auto response = std::make_unique<BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content(content);
    response->set_content_type("application/json");
    return response;
  }

  static std::unique_ptr<HttpResponse> CreateFailedResponse() {
    auto response = std::make_unique<BasicHttpResponse>();
    response->set_code(net::HTTP_INTERNAL_SERVER_ERROR);
    return response;
  }

  MOCK_METHOD(std::unique_ptr<HttpResponse>,
              HandleRequest,
              (const HttpRequest&));
};

// Observer for `ui::ListModel` changes.
class TestListModelObserver : public ui::ListModelObserver {
 public:
  MOCK_METHOD(void, ListItemsAdded, (size_t start, size_t count), (override));
  MOCK_METHOD(void, ListItemsRemoved, (size_t start, size_t count), (override));
  MOCK_METHOD(void,
              ListItemMoved,
              (size_t index, size_t target_index),
              (override));
  MOCK_METHOD(void, ListItemsChanged, (size_t start, size_t count), (override));
};

}  // namespace

class TasksClientImplIsDisabledByAdminTest : public testing::Test {
 public:
  TasksClientImplIsDisabledByAdminTest()
      : profile_manager_(
            TestingProfileManager(TestingBrowserProcess::GetGlobal())) {}

  void SetUp() override { ASSERT_TRUE(profile_manager_.SetUp()); }

  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
  GetDefaultPrefs() const {
    auto prefs =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterUserProfilePrefs(prefs->registry());
    return prefs;
  }

  TestingProfile* CreateTestingProfile(
      std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> prefs) {
    return profile_manager_.CreateTestingProfile(
        "profile@example.com", std::move(prefs), u"User Name", /*avatar_id=*/0,
        TestingProfile::TestingFactories());
  }

  TasksClientImpl CreateClientForProfile(Profile* profile) const {
    return TasksClientImpl(
        profile,
        base::BindLambdaForTesting(
            [&](const std::vector<std::string>& scopes,
                const net::NetworkTrafficAnnotationTag& traffic_annotation_tag)
                -> std::unique_ptr<google_apis::RequestSender> {
              return nullptr;
            }),
        TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  const base::HistogramTester* histogram_tester() const {
    return &histogram_tester_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  const base::HistogramTester histogram_tester_;
  TestingProfileManager profile_manager_;
};

TEST_F(TasksClientImplIsDisabledByAdminTest, Default) {
  auto* const profile = CreateTestingProfile(GetDefaultPrefs());
  EXPECT_FALSE(CreateClientForProfile(profile).IsDisabledByAdmin());
  histogram_tester()->ExpectUniqueSample(
      "Ash.ContextualGoogleIntegrations.GoogleTasks.Status",
      ContextualGoogleIntegrationStatus::kEnabled,
      /*expected_bucket_count=*/1);
}

TEST_F(TasksClientImplIsDisabledByAdminTest,
       NoTasksInContextualGoogleIntegrationsPref) {
  auto prefs = GetDefaultPrefs();
  base::Value::List enabled_integrations;
  enabled_integrations.Append(prefs::kGoogleCalendarIntegrationName);
  enabled_integrations.Append(prefs::kGoogleClassroomIntegrationName);
  prefs->SetList(prefs::kContextualGoogleIntegrationsConfiguration,
                 std::move(enabled_integrations));

  auto* const profile = CreateTestingProfile(std::move(prefs));
  EXPECT_TRUE(CreateClientForProfile(profile).IsDisabledByAdmin());
  histogram_tester()->ExpectUniqueSample(
      "Ash.ContextualGoogleIntegrations.GoogleTasks.Status",
      ContextualGoogleIntegrationStatus::kDisabledByPolicy,
      /*expected_bucket_count=*/1);
}

TEST_F(TasksClientImplIsDisabledByAdminTest, DisabledCalendarApp) {
  auto* const profile = CreateTestingProfile(GetDefaultPrefs());

  std::vector<apps::AppPtr> app_deltas;
  app_deltas.push_back(apps::AppPublisher::MakeApp(
      apps::AppType::kWeb, web_app::kGoogleCalendarAppId,
      apps::Readiness::kDisabledByPolicy, "Calendar",
      apps::InstallReason::kUser, apps::InstallSource::kBrowser));

  apps::AppServiceProxyFactory::GetForProfile(profile)->OnApps(
      std::move(app_deltas), apps::AppType::kWeb,
      /*should_notify_initialized=*/true);

  EXPECT_TRUE(CreateClientForProfile(profile).IsDisabledByAdmin());
  histogram_tester()->ExpectUniqueSample(
      "Ash.ContextualGoogleIntegrations.GoogleTasks.Status",
      ContextualGoogleIntegrationStatus::kDisabledByAppBlock,
      /*expected_bucket_count=*/1);
}

TEST_F(TasksClientImplIsDisabledByAdminTest, BlockedTasksUrl) {
  auto prefs = GetDefaultPrefs();
  base::Value::List blocklist;
  blocklist.Append("tasks.google.com");
  prefs->SetManagedPref(policy::policy_prefs::kUrlBlocklist,
                        std::move(blocklist));

  auto* const profile = CreateTestingProfile(std::move(prefs));
  EXPECT_TRUE(CreateClientForProfile(profile).IsDisabledByAdmin());
  histogram_tester()->ExpectUniqueSample(
      "Ash.ContextualGoogleIntegrations.GoogleTasks.Status",
      ContextualGoogleIntegrationStatus::kDisabledByUrlBlock,
      /*expected_bucket_count=*/1);
}

class TasksClientImplTest : public testing::Test {
 public:
  TasksClientImplTest()
      : profile_manager_(
            TestingProfileManager(TestingBrowserProcess::GetGlobal())) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());

    auto create_request_sender_callback = base::BindLambdaForTesting(
        [&](const std::vector<std::string>& scopes,
            const net::NetworkTrafficAnnotationTag& traffic_annotation_tag) {
          return std::make_unique<google_apis::RequestSender>(
              std::make_unique<google_apis::DummyAuthService>(),
              url_loader_factory_, task_environment_.GetMainThreadTaskRunner(),
              "test-user-agent", traffic_annotation_tag);
        });
    client_ = std::make_unique<TasksClientImpl>(
        profile_manager_.CreateTestingProfile("profile@example.com",
                                              /*is_main_profile=*/true,
                                              url_loader_factory_),
        create_request_sender_callback, TRAFFIC_ANNOTATION_FOR_TESTS);

    test_server_.RegisterRequestHandler(
        base::BindRepeating(&TestRequestHandler::HandleRequest,
                            base::Unretained(&request_handler_)));
    ASSERT_TRUE(test_server_.Start());

    gaia_urls_overrider_ = std::make_unique<GaiaUrlsOverriderForTesting>(
        base::CommandLine::ForCurrentProcess(), "tasks_api_origin_url",
        test_server_.base_url().spec());
    ASSERT_EQ(GaiaUrls::GetInstance()->tasks_api_origin_url(),
              test_server_.base_url().spec());
  }

  TasksClientImpl* client() { return client_.get(); }
  base::HistogramTester* histogram_tester() { return &histogram_tester_; }
  TestRequestHandler& request_handler() { return request_handler_; }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  TestingProfileManager profile_manager_;
  net::EmbeddedTestServer test_server_;
  scoped_refptr<network::TestSharedURLLoaderFactory> url_loader_factory_ =
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>(
          /*network_service=*/nullptr,
          /*is_trusted=*/true);
  std::unique_ptr<GaiaUrlsOverriderForTesting> gaia_urls_overrider_;
  testing::StrictMock<TestRequestHandler> request_handler_;
  std::unique_ptr<TasksClientImpl> client_;
  base::HistogramTester histogram_tester_;
};

// ----------------------------------------------------------------------------
// Get task lists:

TEST_F(TasksClientImplTest, GetTaskLists) {
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          kDefaultTaskListsResponseContent))));

  TaskListsFuture future;
  client()->GetTaskLists(/*force_fetch=*/false, future.GetCallback());
  ASSERT_TRUE(future.Wait());

  const auto [success, http_error, task_lists] = future.Take();
  EXPECT_TRUE(success);
  EXPECT_EQ(task_lists->item_count(), 2u);

  EXPECT_EQ(task_lists->GetItemAt(0)->id, "qwerty");
  EXPECT_EQ(task_lists->GetItemAt(0)->title, "My Tasks 1");
  EXPECT_EQ(FormatTimeAsString(task_lists->GetItemAt(0)->updated),
            "2023-01-30T22:19:22.812Z");

  EXPECT_EQ(task_lists->GetItemAt(1)->id, "asdfgh");
  EXPECT_EQ(task_lists->GetItemAt(1)->title, "My Tasks 2");
  EXPECT_EQ(FormatTimeAsString(task_lists->GetItemAt(1)->updated),
            "2022-12-21T23:38:22.590Z");

  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Tasks.GetTaskLists.Latency", /*expected_count=*/1);
  histogram_tester()->ExpectUniqueSample(
      "Ash.Glanceables.Api.Tasks.GetTaskLists.Status",
      ApiErrorCode::HTTP_SUCCESS,
      /*expected_bucket_count=*/1);
  histogram_tester()->ExpectUniqueSample(
      "Ash.Glanceables.Api.Tasks.GetTaskLists.PagesCount",
      /*sample=*/1,
      /*expected_bucket_count=*/1);
  histogram_tester()->ExpectUniqueSample(
      "Ash.Glanceables.Api.Tasks.TaskListsCount",
      /*sample=*/2,
      /*expected_bucket_count=*/1);
}

TEST_F(TasksClientImplTest, GetTaskListsOnSubsequentCalls) {
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          kDefaultTaskListsResponseContent))));

  TaskListsFuture future;
  client()->GetTaskLists(/*force_fetch=*/false, future.GetRepeatingCallback());
  ASSERT_TRUE(future.Wait());

  const auto [status, http_error, task_lists] = future.Take();
  EXPECT_TRUE(status);

  // Subsequent request doesn't trigger another network call and returns a
  // pointer to the same `ui::ListModel`.
  client()->GetTaskLists(/*force_fetch=*/false, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(std::get<2>(future.Take()), task_lists);
}

TEST_F(TasksClientImplTest, GetCachedTaskLists) {
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          kDefaultTaskListsResponseContent))));

  EXPECT_EQ(client()->GetCachedTaskLists(), nullptr);

  TaskListsFuture future;
  client()->GetTaskLists(/*force_fetch=*/false, future.GetRepeatingCallback());
  ASSERT_TRUE(future.Wait());

  const auto [status, http_error, task_lists] = future.Take();
  EXPECT_TRUE(status);
  EXPECT_EQ(client()->GetCachedTaskLists(), task_lists);
}

TEST_F(TasksClientImplTest, ConcurrentGetTaskListsCalls) {
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          kDefaultTaskListsResponseContent))));

  TaskListsFuture first_future;
  client()->GetTaskLists(/*force_fetch=*/false, first_future.GetCallback());

  TaskListsFuture second_future;
  client()->GetTaskLists(/*force_fetch=*/false, second_future.GetCallback());

  ASSERT_TRUE(first_future.Wait());
  ASSERT_TRUE(second_future.Wait());

  const auto [first_success, first_error, task_lists] = first_future.Take();
  EXPECT_TRUE(first_success);

  const auto [second_success, second_error, second_task_lists] =
      second_future.Take();
  EXPECT_TRUE(second_success);

  EXPECT_EQ(task_lists, second_task_lists);
  EXPECT_EQ(task_lists->item_count(), 2u);

  EXPECT_EQ(task_lists->GetItemAt(0)->id, "qwerty");
  EXPECT_EQ(task_lists->GetItemAt(1)->id, "asdfgh");

  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Tasks.GetTaskLists.Latency", /*expected_count=*/1);
  histogram_tester()->ExpectUniqueSample(
      "Ash.Glanceables.Api.Tasks.GetTaskLists.Status",
      ApiErrorCode::HTTP_SUCCESS,
      /*expected_bucket_count=*/1);
}

TEST_F(TasksClientImplTest,
       GetTaskListsOnSubsequentCallsAfterClosingGlanceablesBubble) {
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#taskLists",
            "items": [{
              "id": "qwerty",
              "title": "My Tasks 1",
              "updated": "2023-01-30T22:19:22.812Z"
            }, {
              "id": "asdfgh",
              "title": "My Tasks 2",
              "updated": "2022-12-21T23:38:22.590Z"
            }]
          })"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#taskLists",
            "items": [{
              "id": "qwerty",
              "title": "My Tasks 1",
              "updated": "2023-01-30T22:19:22.812Z"
            }, {
              "id": "zxcvbn",
              "title": "My Tasks 3",
              "updated": "2022-12-21T23:38:22.590Z"
            }]
          })"))));

  TaskListsFuture future;
  client()->GetTaskLists(/*force_fetch=*/false, future.GetRepeatingCallback());
  ASSERT_TRUE(future.Wait());

  const auto [success, http_error, task_lists] = future.Take();
  EXPECT_TRUE(success);

  EXPECT_EQ(task_lists->item_count(), 2u);
  EXPECT_EQ(task_lists->GetItemAt(0)->id, "qwerty");
  EXPECT_EQ(task_lists->GetItemAt(1)->id, "asdfgh");

  client()->OnGlanceablesBubbleClosed(base::DoNothing());

  // Request to get tasks after glanceables bubble was closed should trigger
  // another fetch.
  TaskListsFuture refresh_future;
  client()->GetTaskLists(/*force_fetch=*/false, refresh_future.GetCallback());
  ASSERT_TRUE(refresh_future.Wait());

  const auto [refresh_success, refresh_error, refreshed_task_lists] =
      refresh_future.Take();
  EXPECT_TRUE(refresh_success);

  EXPECT_EQ(refreshed_task_lists->item_count(), 2u);
  EXPECT_EQ(refreshed_task_lists->GetItemAt(0)->id, "qwerty");
  EXPECT_EQ(refreshed_task_lists->GetItemAt(1)->id, "zxcvbn");

  TaskListsFuture repeated_refresh_future;
  client()->GetTaskLists(/*force_fetch=*/false,
                         repeated_refresh_future.GetCallback());

  const auto [repeated_refresh_success, repeated_refresh_error,
              repeated_refreshed_task_lists] = repeated_refresh_future.Take();
  EXPECT_TRUE(repeated_refresh_success);

  EXPECT_EQ(repeated_refreshed_task_lists->item_count(), 2u);
  EXPECT_EQ(repeated_refreshed_task_lists->GetItemAt(0)->id, "qwerty");
  EXPECT_EQ(repeated_refreshed_task_lists->GetItemAt(1)->id, "zxcvbn");
}

TEST_F(TasksClientImplTest, GlanceablesBubbleClosedWhileFetchingTaskLists) {
  base::RunLoop first_request_waiter;
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(Invoke([&first_request_waiter](const HttpRequest&) {
        first_request_waiter.Quit();

        return TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#taskLists",
            "items": [{
              "id": "qwerty",
              "title": "My Tasks 1",
              "updated": "2023-01-30T22:19:22.812Z"
            }, {
              "id": "asdfgh",
              "title": "My Tasks 2",
              "updated": "2022-12-21T23:38:22.590Z"
            }]
          })");
      }))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#taskLists",
            "items": [{
              "id": "qwerty",
              "title": "My Tasks 1",
              "updated": "2023-01-30T22:19:22.812Z"
            }, {
              "id": "zxcvbn",
              "title": "My Tasks 3",
              "updated": "2022-12-21T23:38:22.590Z"
            }]
          })"))));

  TaskListsFuture future;
  client()->GetTaskLists(/*force_fetch=*/false, future.GetRepeatingCallback());

  // Simulate bubble closure before first request response arrives.
  client()->OnGlanceablesBubbleClosed(base::DoNothing());

  ASSERT_TRUE(future.Wait());

  const auto [success, http_error, task_lists] = future.Take();
  EXPECT_FALSE(success);

  EXPECT_EQ(task_lists->item_count(), 0u);

  // Wait for the first reqeust response to be generated before making the
  // second request, to guard agains the case where second request gets handled
  // by the test server before the first one.
  first_request_waiter.Run();

  // Request to get tasks after glanceables bubble was closed should trigger
  // another fetch.
  TaskListsFuture refresh_future;
  client()->GetTaskLists(/*force_fetch=*/false, refresh_future.GetCallback());
  ASSERT_TRUE(refresh_future.Wait());

  const auto [refresh_success, refresh_error, refreshed_task_lists] =
      refresh_future.Take();
  EXPECT_TRUE(refresh_success);

  EXPECT_EQ(refreshed_task_lists->item_count(), 2u);
  EXPECT_EQ(refreshed_task_lists->GetItemAt(0)->id, "qwerty");
  EXPECT_EQ(refreshed_task_lists->GetItemAt(1)->id, "zxcvbn");

  TaskListsFuture repeated_refresh_future;
  client()->GetTaskLists(/*force_fetch=*/false,
                         repeated_refresh_future.GetCallback());

  const auto [repeated_refresh_success, repeated_error,
              repeated_refreshed_task_lists] = repeated_refresh_future.Take();
  EXPECT_TRUE(repeated_refresh_success);

  EXPECT_EQ(repeated_refreshed_task_lists->item_count(), 2u);
  EXPECT_EQ(repeated_refreshed_task_lists->GetItemAt(0)->id, "qwerty");
  EXPECT_EQ(repeated_refreshed_task_lists->GetItemAt(1)->id, "zxcvbn");
}

TEST_F(TasksClientImplTest, GetTaskListsReturnsEmptyVectorOnHttpError) {
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateFailedResponse())));

  TaskListsFuture future;
  client()->GetTaskLists(/*force_fetch=*/false, future.GetCallback());
  ASSERT_TRUE(future.Wait());

  const auto [success, http_error, task_lists] = future.Take();
  EXPECT_FALSE(success);
  EXPECT_EQ(task_lists->item_count(), 0u);

  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Tasks.GetTaskLists.Latency", /*expected_count=*/1);
  histogram_tester()->ExpectUniqueSample(
      "Ash.Glanceables.Api.Tasks.GetTaskLists.Status",
      ApiErrorCode::HTTP_INTERNAL_SERVER_ERROR,
      /*expected_bucket_count=*/1);
}

TEST_F(TasksClientImplTest, GetTaskListsReturnsCachedResultsOnHttpError) {
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#taskLists",
            "items": [{
              "id": "qwerty",
              "title": "My Tasks 1",
              "updated": "2023-01-30T22:19:22.812Z"
            }, {
              "id": "asdfgh",
              "title": "My Tasks 2",
              "updated": "2022-12-21T23:38:22.590Z"
            }]
          })"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateFailedResponse())))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#taskLists",
            "items": [{
              "id": "zxcvb",
              "title": "My Tasks 3",
              "updated": "2023-01-30T22:19:22.812Z"
            }]
          })"))));

  TaskListsFuture future;
  client()->GetTaskLists(/*force_fetch=*/false, future.GetCallback());
  ASSERT_TRUE(future.Wait());

  const auto [success, http_error, task_lists] = future.Take();
  EXPECT_TRUE(success);
  EXPECT_EQ(task_lists->item_count(), 2u);
  EXPECT_EQ(task_lists->GetItemAt(0)->id, "qwerty");
  EXPECT_EQ(task_lists->GetItemAt(1)->id, "asdfgh");

  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Tasks.GetTaskLists.Latency", /*expected_count=*/1);
  histogram_tester()->ExpectUniqueSample(
      "Ash.Glanceables.Api.Tasks.GetTaskLists.Status",
      ApiErrorCode::HTTP_SUCCESS,
      /*expected_bucket_count=*/1);

  client()->OnGlanceablesBubbleClosed(base::DoNothing());

  TaskListsFuture failure_future;
  client()->GetTaskLists(/*force_fetch=*/true, failure_future.GetCallback());
  ASSERT_TRUE(failure_future.Wait());

  const auto [failure_status, failure_error, failed_task_lists] =
      failure_future.Take();
  EXPECT_FALSE(failure_status);

  EXPECT_EQ(failed_task_lists->item_count(), 2u);
  EXPECT_EQ(failed_task_lists->GetItemAt(0)->id, "qwerty");
  EXPECT_EQ(failed_task_lists->GetItemAt(1)->id, "asdfgh");

  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Tasks.GetTaskLists.Latency", /*expected_count=*/2);
  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Tasks.GetTaskLists.Status", 2);
  histogram_tester()->ExpectBucketCount(
      "Ash.Glanceables.Api.Tasks.GetTaskLists.Status",
      ApiErrorCode::HTTP_INTERNAL_SERVER_ERROR,
      /*expected_bucket_count=*/1);

  TaskListsFuture retry_future;
  client()->GetTaskLists(/*force_fetch=*/false, retry_future.GetCallback());
  ASSERT_TRUE(retry_future.Wait());

  const auto [retry_success, retry_error, retry_task_lists] =
      retry_future.Take();
  EXPECT_TRUE(retry_success);

  EXPECT_EQ(retry_task_lists->item_count(), 1u);
  EXPECT_EQ(retry_task_lists->GetItemAt(0)->id, "zxcvb");

  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Tasks.GetTaskLists.Latency", /*expected_count=*/3);
  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Tasks.GetTaskLists.Status", 3);
  histogram_tester()->ExpectBucketCount(
      "Ash.Glanceables.Api.Tasks.GetTaskLists.Status",
      ApiErrorCode::HTTP_SUCCESS,
      /*expected_bucket_count=*/2);
}

TEST_F(TasksClientImplTest,
       GetTaskListsReturnsCachedResultsOnPartialHttpError) {
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  Not(HasSubstr("pageToken")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#taskLists",
            "items": [{
              "id": "qwerty",
              "title": "My Tasks 1",
              "updated": "2023-01-30T22:19:22.812Z"
            }, {
              "id": "asdfgh",
              "title": "My Tasks 2",
              "updated": "2022-12-21T23:38:22.590Z"
            }]
          })"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#taskLists",
            "items": [{
              "id": "zxcvb",
              "title": "My Tasks 3",
              "updated": "2023-01-30T22:19:22.812Z"
            }],
            "nextPageToken": "tt"
          })"))));
  EXPECT_CALL(request_handler(),
              HandleRequest(
                  Field(&HttpRequest::relative_url, HasSubstr("pageToken=tt"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateFailedResponse())));

  TaskListsFuture future;
  client()->GetTaskLists(/*force_fetch=*/false, future.GetCallback());
  ASSERT_TRUE(future.Wait());

  const auto [success, http_error, task_lists] = future.Take();
  EXPECT_TRUE(success);

  EXPECT_EQ(task_lists->item_count(), 2u);
  EXPECT_EQ(task_lists->GetItemAt(0)->id, "qwerty");
  EXPECT_EQ(task_lists->GetItemAt(1)->id, "asdfgh");

  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Tasks.GetTaskLists.Latency", /*expected_count=*/1);
  histogram_tester()->ExpectUniqueSample(
      "Ash.Glanceables.Api.Tasks.GetTaskLists.Status",
      ApiErrorCode::HTTP_SUCCESS,
      /*expected_bucket_count=*/1);

  client()->OnGlanceablesBubbleClosed(base::DoNothing());

  TaskListsFuture failure_future;
  client()->GetTaskLists(/*force_fetch=*/true, failure_future.GetCallback());
  ASSERT_TRUE(failure_future.Wait());

  const auto [failed_status, failed_error, failed_task_lists] =
      failure_future.Take();
  EXPECT_FALSE(failed_status);

  EXPECT_EQ(failed_task_lists->item_count(), 2u);
  EXPECT_EQ(failed_task_lists->GetItemAt(0)->id, "qwerty");
  EXPECT_EQ(failed_task_lists->GetItemAt(1)->id, "asdfgh");

  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Tasks.GetTaskLists.Latency", /*expected_count=*/3);
  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Tasks.GetTaskLists.Status", 3);
  histogram_tester()->ExpectBucketCount(
      "Ash.Glanceables.Api.Tasks.GetTaskLists.Status",
      ApiErrorCode::HTTP_SUCCESS,
      /*expected_bucket_count=*/2);
  histogram_tester()->ExpectBucketCount(
      "Ash.Glanceables.Api.Tasks.GetTaskLists.Status",
      ApiErrorCode::HTTP_INTERNAL_SERVER_ERROR,
      /*expected_bucket_count=*/1);
}

TEST_F(TasksClientImplTest, GetTaskListsFetchesAllPages) {
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  Not(HasSubstr("pageToken")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#taskLists",
            "items": [{"id": "task-list-from-page-1"}],
            "nextPageToken": "qwe"
          }
        )"))));
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  HasSubstr("pageToken=qwe"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#taskLists",
            "items": [{"id": "task-list-from-page-2"}],
            "nextPageToken": "asd"
          }
        )"))));
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  HasSubstr("pageToken=asd"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#taskLists",
            "items": [{"id": "task-list-from-page-3"}]
          }
        )"))));

  TaskListsFuture future;
  client()->GetTaskLists(/*force_fetch=*/false, future.GetCallback());
  ASSERT_TRUE(future.Wait());

  const auto [status, http_error, task_lists] = future.Take();
  EXPECT_TRUE(status);

  EXPECT_EQ(task_lists->item_count(), 3u);
  EXPECT_EQ(task_lists->GetItemAt(0)->id, "task-list-from-page-1");
  EXPECT_EQ(task_lists->GetItemAt(1)->id, "task-list-from-page-2");
  EXPECT_EQ(task_lists->GetItemAt(2)->id, "task-list-from-page-3");

  histogram_tester()->ExpectUniqueSample(
      "Ash.Glanceables.Api.Tasks.GetTaskLists.PagesCount",
      /*sample=*/3,
      /*expected_bucket_count=*/1);
  histogram_tester()->ExpectUniqueSample(
      "Ash.Glanceables.Api.Tasks.TaskListsCount",
      /*sample=*/3,
      /*expected_bucket_count=*/1);
}

TEST_F(TasksClientImplTest, GlanceablesBubbleClosedWhileFetchingTaskListsPage) {
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  Not(HasSubstr("pageToken")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#taskLists",
            "items": [{"id": "task-list-from-page-1"}],
            "nextPageToken": "qwe"
          }
        )"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#taskLists",
            "items": [{"id": "task-list-from-page-1-2"}],
            "nextPageToken": "qwe"
          }
        )"))));
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  HasSubstr("pageToken=qwe"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#taskLists",
            "items": [{"id": "task-list-from-page-2"}],
            "nextPageToken": "asd"
          })"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#taskLists",
            "items": [{"id": "task-list-from-page-2-2"}],
            "nextPageToken": "asd"
          })"))));
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  HasSubstr("pageToken=asd"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#taskLists",
            "items": [{"id": "task-list-from-page-3-2"}]
          }
        )"))));

  // Set a test callback that simulates glanceables bubble closing just after
  // requesting the second task lists page.
  client()->set_task_lists_request_callback_for_testing(
      base::BindLambdaForTesting([&](const std::string& page_token) {
        if (page_token == "qwe") {
          client()->OnGlanceablesBubbleClosed(base::DoNothing());
        }
      }));
  TaskListsFuture future;
  client()->GetTaskLists(/*force_fetch=*/false, future.GetRepeatingCallback());

  // Note that injected tasks lists request test callback simulates bubble
  // closure before returning the second task lists page. The
  // `GetTaskLists(/*force_fetch=*/false, )` call should return, but it will
  // contain empty tasks list, as closing the bubble cancels the fetch.
  ASSERT_TRUE(future.Wait());

  const auto [success, http_error, task_lists] = future.Take();
  EXPECT_FALSE(success);

  EXPECT_EQ(task_lists->item_count(), 0u);

  client()->set_task_lists_request_callback_for_testing(
      TasksClientImpl::TaskListsRequestCallback());

  // Request to get tasks after glanceables bubble was closed should trigger
  // another fetch.
  TaskListsFuture refresh_future;
  client()->GetTaskLists(/*force_fetch=*/false, refresh_future.GetCallback());
  ASSERT_TRUE(refresh_future.Wait());

  const auto [refresh_success, refresh_error, refreshed_task_lists] =
      refresh_future.Take();
  EXPECT_TRUE(refresh_success);

  EXPECT_EQ(refreshed_task_lists->item_count(), 3u);
  EXPECT_EQ(refreshed_task_lists->GetItemAt(0)->id, "task-list-from-page-1-2");
  EXPECT_EQ(refreshed_task_lists->GetItemAt(1)->id, "task-list-from-page-2-2");
  EXPECT_EQ(refreshed_task_lists->GetItemAt(2)->id, "task-list-from-page-3-2");

  TaskListsFuture repeated_refresh_future;
  client()->GetTaskLists(/*force_fetch=*/false,
                         repeated_refresh_future.GetCallback());

  const auto [repeated_refresh_success, repreated_refresh_error,
              repeated_refreshed_task_lists] = repeated_refresh_future.Take();
  EXPECT_TRUE(repeated_refresh_success);

  EXPECT_EQ(repeated_refreshed_task_lists->item_count(), 3u);
  EXPECT_EQ(repeated_refreshed_task_lists->GetItemAt(0)->id,
            "task-list-from-page-1-2");
  EXPECT_EQ(repeated_refreshed_task_lists->GetItemAt(1)->id,
            "task-list-from-page-2-2");
  EXPECT_EQ(repeated_refreshed_task_lists->GetItemAt(2)->id,
            "task-list-from-page-3-2");
}

TEST_F(TasksClientImplTest, AbandonedTaskListsRemovedFromCache) {
  EXPECT_CALL(request_handler(), HandleRequest(Field(&HttpRequest::relative_url,
                                                     HasSubstr("lists?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#taskLists",
            "items": [{
              "id": "qwerty",
              "title": "My Tasks 1",
              "updated": "2023-01-30T22:19:22.812Z"
            }, {
              "id": "asdfgh",
              "title": "My Tasks 2",
              "updated": "2022-12-21T23:38:22.590Z"
            }]
          })"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#taskLists",
            "items": [{
              "id": "qwerty",
              "title": "My Tasks 1",
              "updated": "2023-01-30T22:19:22.812Z"
            }]
          })"))));
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  HasSubstr("/qwerty/tasks?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#tasks",
            "items": [{
              "id": "asd",
              "title": "Parent task, level 1",
              "status": "needsAction",
              "due": "2023-04-19T00:00:00.000Z"
            }, {
              "id": "zxc",
              "title": "Parent task 3, level 1",
              "status": "needsAction",
              "links": [{"type": "email"}]
            }]
          })"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateFailedResponse())));
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  HasSubstr("/asdfgh/tasks?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#tasks",
            "items": [{
              "id": "fgh",
              "title": "Parent task, level 1",
              "status": "needsAction",
              "due": "2023-04-19T00:00:00.000Z"
            }]
          })"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateFailedResponse())));

  TaskListsFuture future;
  client()->GetTaskLists(/*force_fetch=*/false, future.GetCallback());
  ASSERT_TRUE(future.Wait());

  const auto [status, http_error, task_lists] = future.Take();
  EXPECT_TRUE(status);

  ASSERT_EQ(task_lists->item_count(), 2u);
  EXPECT_EQ(task_lists->GetItemAt(0)->id, "qwerty");
  EXPECT_EQ(task_lists->GetItemAt(1)->id, "asdfgh");

  TasksFuture abandoned_tasks_future;
  client()->GetTasks("asdfgh", /*force_fetch=*/true,
                     abandoned_tasks_future.GetCallback());
  ASSERT_TRUE(abandoned_tasks_future.Wait());

  const auto [abandoned_tasks_success, abandoned_error, abandoned_tasks] =
      abandoned_tasks_future.Take();
  EXPECT_TRUE(abandoned_tasks_success);

  EXPECT_EQ(abandoned_tasks->item_count(), 1u);
  EXPECT_EQ(abandoned_tasks->GetItemAt(0)->id, "fgh");

  TasksFuture tasks_future;
  client()->GetTasks("qwerty", /*force_fetch=*/true,
                     tasks_future.GetCallback());
  ASSERT_TRUE(tasks_future.Wait());

  const auto [tasks_success, tasks_error, tasks] = tasks_future.Take();
  EXPECT_TRUE(tasks_success);

  EXPECT_EQ(tasks->item_count(), 2u);
  EXPECT_EQ(tasks->GetItemAt(0)->id, "asd");
  EXPECT_EQ(tasks->GetItemAt(1)->id, "zxc");

  client()->OnGlanceablesBubbleClosed(base::DoNothing());

  TaskListsFuture refreshed_task_list_future;
  client()->GetTaskLists(/*force_fetch=*/true,
                         refreshed_task_list_future.GetCallback());
  ASSERT_TRUE(refreshed_task_list_future.Wait());

  const auto [refresh_status, refresh_error, refreshed_task_lists] =
      refreshed_task_list_future.Take();
  EXPECT_TRUE(refresh_status);

  EXPECT_EQ(refreshed_task_lists->item_count(), 1u);
  EXPECT_EQ(refreshed_task_lists->GetItemAt(0)->id, "qwerty");

  TasksFuture refreshed_abandoned_tasks_future;
  client()->GetTasks("asdfgh", /*force_fetch=*/true,
                     refreshed_abandoned_tasks_future.GetCallback());
  ASSERT_TRUE(refreshed_abandoned_tasks_future.Wait());

  const auto [refresh_abandoned_tasks_success, refresh_abandoned_tasks_error,
              refreshed_abandoned_tasks] =
      refreshed_abandoned_tasks_future.Take();
  EXPECT_FALSE(refresh_abandoned_tasks_success);

  EXPECT_EQ(refreshed_abandoned_tasks->item_count(), 0u);

  TasksFuture refreshed_tasks_future;
  client()->GetTasks("qwerty", /*force_fetch=*/true,
                     refreshed_tasks_future.GetCallback());
  ASSERT_TRUE(refreshed_tasks_future.Wait());

  const auto [refresh_success, refresh_success_error, refreshed_tasks] =
      refreshed_tasks_future.Take();
  EXPECT_FALSE(refresh_success);

  EXPECT_EQ(refreshed_tasks->item_count(), 2u);
  EXPECT_EQ(refreshed_tasks->GetItemAt(0)->id, "asd");
  EXPECT_EQ(refreshed_tasks->GetItemAt(1)->id, "zxc");
}

// ----------------------------------------------------------------------------
// Get tasks:

TEST_F(TasksClientImplTest, GetTasks) {
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          kDefaultTasksResponseContent))));

  TasksFuture future;
  client()->GetTasks("test-task-list-id", /*force_fetch=*/false,
                     future.GetCallback());
  ASSERT_TRUE(future.Wait());

  const auto [success, http_error, root_tasks] = future.Take();
  EXPECT_TRUE(success);

  ASSERT_EQ(root_tasks->item_count(), 2u);

  EXPECT_EQ(root_tasks->GetItemAt(0)->id, "asd");
  EXPECT_EQ(root_tasks->GetItemAt(0)->title, "Parent task, level 1");
  EXPECT_EQ(root_tasks->GetItemAt(0)->completed, false);
  EXPECT_EQ(FormatTimeAsString(root_tasks->GetItemAt(0)->due.value()),
            "2023-04-19T00:00:00.000Z");
  EXPECT_TRUE(root_tasks->GetItemAt(0)->has_subtasks);
  EXPECT_FALSE(root_tasks->GetItemAt(0)->has_email_link);
  EXPECT_FALSE(root_tasks->GetItemAt(0)->has_notes);
  EXPECT_EQ(FormatTimeAsString(root_tasks->GetItemAt(0)->updated),
            "2023-01-30T22:19:22.812Z");
  EXPECT_EQ(root_tasks->GetItemAt(0)->web_view_link,
            "https://tasks.google.com/task/id1");
  EXPECT_EQ(root_tasks->GetItemAt(0)->origin_surface_type,
            api::Task::OriginSurfaceType::kRegular);

  EXPECT_EQ(root_tasks->GetItemAt(1)->id, "zxc");
  EXPECT_EQ(root_tasks->GetItemAt(1)->title, "Parent task 2, level 1");
  EXPECT_EQ(root_tasks->GetItemAt(1)->completed, false);
  EXPECT_FALSE(root_tasks->GetItemAt(1)->due);
  EXPECT_FALSE(root_tasks->GetItemAt(1)->has_subtasks);
  EXPECT_TRUE(root_tasks->GetItemAt(1)->has_email_link);
  EXPECT_TRUE(root_tasks->GetItemAt(1)->has_notes);
  EXPECT_EQ(FormatTimeAsString(root_tasks->GetItemAt(1)->updated),
            "2022-12-21T23:38:22.590Z");
  EXPECT_EQ(root_tasks->GetItemAt(1)->web_view_link,
            "https://tasks.google.com/task/id3");
  EXPECT_EQ(root_tasks->GetItemAt(1)->origin_surface_type,
            api::Task::OriginSurfaceType::kRegular);

  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Tasks.GetTasks.Latency", /*expected_count=*/1);
  histogram_tester()->ExpectUniqueSample(
      "Ash.Glanceables.Api.Tasks.GetTasks.Status", ApiErrorCode::HTTP_SUCCESS,
      /*expected_bucket_count=*/1);
  histogram_tester()->ExpectUniqueSample(
      "Ash.Glanceables.Api.Tasks.GetTasks.PagesCount",
      /*sample=*/1,
      /*expected_bucket_count=*/1);
  histogram_tester()->ExpectUniqueSample(
      "Ash.Glanceables.Api.Tasks.RawTasksCount",
      /*sample=*/3,
      /*expected_bucket_count=*/1);
  histogram_tester()->ExpectUniqueSample(
      "Ash.Glanceables.Api.Tasks.ProcessedTasksCount",
      /*sample=*/2,
      /*expected_bucket_count=*/1);
}

TEST_F(TasksClientImplTest, ConcurrentGetTasksCalls) {
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          kDefaultTasksResponseContent))));

  TasksFuture first_future;
  client()->GetTasks("test-task-list-id", /*force_fetch=*/false,
                     first_future.GetCallback());

  TasksFuture second_future;
  client()->GetTasks("test-task-list-id", /*force_fetch=*/false,
                     second_future.GetCallback());

  ASSERT_TRUE(first_future.Wait());
  ASSERT_TRUE(second_future.Wait());

  const auto [first_success, first_error, root_tasks] = first_future.Take();
  EXPECT_TRUE(first_success);

  const auto [second_success, second_error, second_root_tasks] =
      second_future.Take();
  EXPECT_TRUE(second_success);

  EXPECT_EQ(root_tasks, second_root_tasks);

  ASSERT_EQ(root_tasks->item_count(), 2u);

  EXPECT_EQ(root_tasks->GetItemAt(0)->id, "asd");
  EXPECT_EQ(root_tasks->GetItemAt(0)->title, "Parent task, level 1");
  EXPECT_EQ(root_tasks->GetItemAt(0)->completed, false);
  EXPECT_EQ(FormatTimeAsString(root_tasks->GetItemAt(0)->due.value()),
            "2023-04-19T00:00:00.000Z");
  EXPECT_TRUE(root_tasks->GetItemAt(0)->has_subtasks);
  EXPECT_FALSE(root_tasks->GetItemAt(0)->has_email_link);

  EXPECT_EQ(root_tasks->GetItemAt(1)->id, "zxc");
  EXPECT_EQ(root_tasks->GetItemAt(1)->title, "Parent task 2, level 1");
  EXPECT_EQ(root_tasks->GetItemAt(1)->completed, false);
  EXPECT_FALSE(root_tasks->GetItemAt(1)->due);
  EXPECT_FALSE(root_tasks->GetItemAt(1)->has_subtasks);
  EXPECT_TRUE(root_tasks->GetItemAt(1)->has_email_link);

  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Tasks.GetTasks.Latency", /*expected_count=*/1);
  histogram_tester()->ExpectUniqueSample(
      "Ash.Glanceables.Api.Tasks.GetTasks.Status", ApiErrorCode::HTTP_SUCCESS,
      /*expected_bucket_count=*/1);
}

TEST_F(TasksClientImplTest, ConcurrentGetTasksCallsForDifferentLists) {
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  HasSubstr("test-task-list-1/tasks?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"({
          "kind": "tasks#tasks",
          "items": [{
            "id": "task-1-1",
            "title": "Parent task, level 1",
            "status": "needsAction",
            "due": "2023-04-19T00:00:00.000Z"
          }, {
            "id": "task-1-2",
            "title": "Parent task 2, level 1",
            "status": "needsAction"
          }]
      })"))));
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  HasSubstr("test-task-list-2/tasks?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"({
          "kind": "tasks#tasks",
          "items": [{
            "id": "task-2-1",
            "title": "Parent task, level 1",
            "status": "needsAction",
            "due": "2023-04-19T00:00:00.000Z"
          }, {
            "id": "task-2-2",
            "title": "Parent task 2, level 1",
            "status": "needsAction"
          }]
      })"))));

  TasksFuture first_future;
  client()->GetTasks("test-task-list-1", /*force_fetch=*/false,
                     first_future.GetCallback());

  TasksFuture second_future;
  client()->GetTasks("test-task-list-2", /*force_fetch=*/false,
                     second_future.GetCallback());

  ASSERT_TRUE(first_future.Wait());
  ASSERT_TRUE(second_future.Wait());

  const auto [first_success, first_error, first_root_tasks] =
      first_future.Take();
  EXPECT_TRUE(first_success);

  ASSERT_EQ(first_root_tasks->item_count(), 2u);

  EXPECT_EQ(first_root_tasks->GetItemAt(0)->id, "task-1-1");
  EXPECT_EQ(first_root_tasks->GetItemAt(1)->id, "task-1-2");

  const auto [second_success, second_error, second_root_tasks] =
      second_future.Take();
  EXPECT_TRUE(second_success);

  ASSERT_EQ(second_root_tasks->item_count(), 2u);

  EXPECT_EQ(second_root_tasks->GetItemAt(0)->id, "task-2-1");
  EXPECT_EQ(second_root_tasks->GetItemAt(1)->id, "task-2-2");

  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Tasks.GetTasks.Latency", /*expected_count=*/2);
  histogram_tester()->ExpectUniqueSample(
      "Ash.Glanceables.Api.Tasks.GetTasks.Status", ApiErrorCode::HTTP_SUCCESS,
      /*expected_bucket_count=*/2);
}

TEST_F(TasksClientImplTest, GetCachedTasks) {
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          kDefaultTasksResponseContent))));

  EXPECT_EQ(client()->GetCachedTasksInTaskList("test-task-list-id"), nullptr);

  TasksFuture future;
  client()->GetTasks("test-task-list-id", /*force_fetch=*/false,
                     future.GetRepeatingCallback());
  ASSERT_TRUE(future.Wait());

  const auto [success, http_error, root_tasks] = future.Take();
  EXPECT_TRUE(success);
  EXPECT_EQ(client()->GetCachedTasksInTaskList("test-task-list-id"),
            root_tasks);
}

TEST_F(TasksClientImplTest, GetTasksOnSubsequentCalls) {
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          kDefaultTasksResponseContent))));

  TasksFuture future;
  client()->GetTasks("test-task-list-id", /*force_fetch=*/false,
                     future.GetRepeatingCallback());
  ASSERT_TRUE(future.Wait());

  const auto [success, http_error, root_tasks] = future.Take();
  EXPECT_TRUE(success);

  // Subsequent request doesn't trigger another network call and returns a
  // pointer to the same `ui::ListModel`.
  client()->GetTasks("test-task-list-id", /*force_fetch=*/false,
                     future.GetCallback());
  ASSERT_TRUE(future.Wait());

  const auto [retry_success, retry_error, retry_root_tasks] = future.Take();
  EXPECT_TRUE(retry_success);
  EXPECT_EQ(retry_root_tasks, root_tasks);
}

TEST_F(TasksClientImplTest, GetTasksOnSubsequentCallsWhenForcingFetch) {
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          kDefaultTasksResponseContent))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          kDefaultTasksResponseContent))));

  TasksFuture future;
  client()->GetTasks("test-task-list-id", /*force_fetch=*/true,
                     future.GetRepeatingCallback());
  ASSERT_TRUE(future.Wait());

  const auto [success, http_error, root_tasks] = future.Take();
  EXPECT_TRUE(success);

  EXPECT_EQ(root_tasks->GetItemAt(0)->id, "asd");
  EXPECT_EQ(root_tasks->GetItemAt(0)->title, "Parent task, level 1");

  // When `force_fetch` is true, we get the updated `ListModel`.
  client()->GetTasks("test-task-list-id", /*force_fetch=*/true,
                     future.GetCallback());
  ASSERT_TRUE(future.Wait());

  const auto [retry_success, retry_error, retry_root_tasks] = future.Take();
  EXPECT_TRUE(retry_success);
}

TEST_F(TasksClientImplTest,
       GetTasksOnSubsequentCallsAfterClosingGlanceablesBubble) {
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"({
          "kind": "tasks#tasks",
          "items": [{
            "id": "asd",
            "title": "Parent task, level 1",
            "status": "needsAction",
            "due": "2023-04-19T00:00:00.000Z"
          }, {
            "id": "qwe",
            "title": "Parent task 2, level 1",
            "status": "needsAction"
          }]
      })"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"({
          "kind": "tasks#tasks",
          "items": [{
            "id": "asd",
            "title": "Parent task, level 1",
            "status": "needsAction",
            "due": "2023-04-19T00:00:00.000Z"
          }, {
            "id": "zxc",
            "title": "Parent task 3, level 1",
            "status": "needsAction",
            "links": [{"type": "email"}]
          }]
      })"))));

  TasksFuture future;
  client()->GetTasks("test-task-list-id", /*force_fetch=*/false,
                     future.GetRepeatingCallback());
  ASSERT_TRUE(future.Wait());

  const auto [success, http_error, root_tasks] = future.Take();
  EXPECT_TRUE(success);

  ASSERT_EQ(root_tasks->item_count(), 2u);

  EXPECT_EQ(root_tasks->GetItemAt(0)->id, "asd");
  EXPECT_EQ(root_tasks->GetItemAt(1)->id, "qwe");

  // Simulate glanceables bubble closure, which should cause the next tasks call
  // to fetch fresh list of tasks.
  client()->OnGlanceablesBubbleClosed(base::DoNothing());

  TasksFuture refresh_future;
  client()->GetTasks("test-task-list-id", /*force_fetch=*/false,
                     refresh_future.GetCallback());
  ASSERT_TRUE(refresh_future.Wait());

  const auto [refresh_success, refresh_error, refreshed_root_tasks] =
      refresh_future.Take();
  EXPECT_TRUE(refresh_success);

  ASSERT_EQ(refreshed_root_tasks->item_count(), 2u);
  EXPECT_EQ(refreshed_root_tasks->GetItemAt(0)->id, "asd");
  EXPECT_EQ(refreshed_root_tasks->GetItemAt(1)->id, "zxc");

  TasksFuture repeated_refresh_future;
  client()->GetTasks("test-task-list-id", /*force_fetch=*/false,
                     repeated_refresh_future.GetCallback());
  ASSERT_TRUE(repeated_refresh_future.Wait());

  const auto [repeated_refresh_success, repeated_refresh_error,
              repeated_refreshed_root_tasks] = repeated_refresh_future.Take();
  EXPECT_TRUE(repeated_refresh_success);

  ASSERT_EQ(repeated_refreshed_root_tasks->item_count(), 2u);
  EXPECT_EQ(repeated_refreshed_root_tasks->GetItemAt(0)->id, "asd");
  EXPECT_EQ(repeated_refreshed_root_tasks->GetItemAt(1)->id, "zxc");
}

TEST_F(TasksClientImplTest, GlanceablesBubbleClosedWhileFetchingTasks) {
  base::RunLoop first_request_waiter;
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(Invoke([&first_request_waiter](const HttpRequest&) {
        first_request_waiter.Quit();
        return TestRequestHandler::CreateSuccessfulResponse(R"({
          "kind": "tasks#tasks",
          "items": [{
            "id": "asd",
            "title": "Parent task, level 1",
            "status": "needsAction",
            "due": "2023-04-19T00:00:00.000Z"
          }, {
            "id": "qwe",
            "title": "Parent task 2, level 1",
            "status": "needsAction"
          }]
        })");
      }))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"({
          "kind": "tasks#tasks",
          "items": [{
            "id": "asd",
            "title": "Parent task, level 1",
            "status": "needsAction",
            "due": "2023-04-19T00:00:00.000Z"
          }, {
            "id": "zxc",
            "title": "Parent task 3, level 1",
            "status": "needsAction",
            "links": [{"type": "email"}]
          }]
      })"))));

  TasksFuture future;
  client()->GetTasks("test-task-list-id", /*force_fetch=*/false,
                     future.GetRepeatingCallback());

  // Simulate glanceables bubble closure, which should cause the next tasks call
  // to fetch fresh list of tasks.
  client()->OnGlanceablesBubbleClosed(base::DoNothing());
  ASSERT_TRUE(future.Wait());

  const auto [success, http_error, root_tasks] = future.Take();
  EXPECT_FALSE(success);

  // Glanceables bubble was closed before receiving tasks response, so
  // `GetTasks()` should have returned an empty list.
  ASSERT_EQ(root_tasks->item_count(), 0u);

  // Wait for the first reqeust response to be generated before making the
  // second request, to guard agains the case where second request gets handled
  // by the test server before the first one.
  first_request_waiter.Run();

  TasksFuture refresh_future;
  client()->GetTasks("test-task-list-id", /*force_fetch=*/false,
                     refresh_future.GetCallback());
  ASSERT_TRUE(refresh_future.Wait());

  const auto [refresh_success, refreshed_error, refreshed_root_tasks] =
      refresh_future.Take();
  EXPECT_TRUE(refresh_success);

  ASSERT_EQ(refreshed_root_tasks->item_count(), 2u);
  EXPECT_EQ(refreshed_root_tasks->GetItemAt(0)->id, "asd");
  EXPECT_EQ(refreshed_root_tasks->GetItemAt(1)->id, "zxc");

  TasksFuture repeated_refresh_future;
  client()->GetTasks("test-task-list-id", /*force_fetch=*/false,
                     repeated_refresh_future.GetCallback());
  ASSERT_TRUE(repeated_refresh_future.Wait());

  const auto [repeated_refresh_success, repeated_refresh_error,
              repeated_refreshed_root_tasks] = repeated_refresh_future.Take();
  EXPECT_TRUE(repeated_refresh_success);

  ASSERT_EQ(repeated_refreshed_root_tasks->item_count(), 2u);
  EXPECT_EQ(repeated_refreshed_root_tasks->GetItemAt(0)->id, "asd");
  EXPECT_EQ(repeated_refreshed_root_tasks->GetItemAt(1)->id, "zxc");
}

TEST_F(TasksClientImplTest, GetTasksReturnsEmptyVectorOnHttpError) {
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateFailedResponse())));

  TasksFuture future;
  client()->GetTasks("test-task-list-id", /*force_fetch=*/false,
                     future.GetCallback());
  ASSERT_TRUE(future.Wait());

  const auto [success, http_error, root_tasks] = future.Take();
  EXPECT_FALSE(success);

  EXPECT_EQ(root_tasks->item_count(), 0u);

  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Tasks.GetTasks.Latency", /*expected_count=*/1);
  histogram_tester()->ExpectUniqueSample(
      "Ash.Glanceables.Api.Tasks.GetTasks.Status",
      ApiErrorCode::HTTP_INTERNAL_SERVER_ERROR,
      /*expected_bucket_count=*/1);
}

TEST_F(TasksClientImplTest, GetTasksReturnsCachedResultsOnHttpError) {
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"({
          "kind": "tasks#tasks",
          "items": [{
            "id": "asd",
            "title": "Parent task, level 1",
            "status": "needsAction",
            "due": "2023-04-19T00:00:00.000Z"
          }, {
            "id": "zxc",
            "title": "Parent task 3, level 1",
            "status": "needsAction",
            "links": [{"type": "email"}]
          }]
      })"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateFailedResponse())))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"({
          "kind": "tasks#tasks",
          "items": [{
            "id": "qwe",
            "title": "Parent task, level 1",
            "status": "needsAction",
            "due": "2023-04-19T00:00:00.000Z"
          }]
      })"))));

  TasksFuture future;
  client()->GetTasks("test-task-list-id", /*force_fetch=*/false,
                     future.GetCallback());
  ASSERT_TRUE(future.Wait());

  const auto [success, http_error, root_tasks] = future.Take();
  EXPECT_TRUE(success);

  EXPECT_EQ(root_tasks->item_count(), 2u);
  EXPECT_EQ(root_tasks->GetItemAt(0)->id, "asd");
  EXPECT_EQ(root_tasks->GetItemAt(1)->id, "zxc");

  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Tasks.GetTasks.Latency", /*expected_count=*/1);
  histogram_tester()->ExpectUniqueSample(
      "Ash.Glanceables.Api.Tasks.GetTasks.Status", ApiErrorCode::HTTP_SUCCESS,
      /*expected_bucket_count=*/1);

  client()->OnGlanceablesBubbleClosed(base::DoNothing());

  TasksFuture failed_future;
  client()->GetTasks("test-task-list-id", /*force_fetch=*/true,
                     failed_future.GetCallback());
  ASSERT_TRUE(failed_future.Wait());

  const auto [failed_status, failed_error, failed_root_tasks] =
      failed_future.Take();
  EXPECT_FALSE(failed_status);

  EXPECT_EQ(failed_root_tasks->item_count(), 2u);
  EXPECT_EQ(failed_root_tasks->GetItemAt(0)->id, "asd");
  EXPECT_EQ(failed_root_tasks->GetItemAt(1)->id, "zxc");

  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Tasks.GetTasks.Latency", /*expected_count=*/2);
  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Tasks.GetTasks.Status", /*expected_count=*/2);
  histogram_tester()->ExpectBucketCount(
      "Ash.Glanceables.Api.Tasks.GetTasks.Status",
      ApiErrorCode::HTTP_INTERNAL_SERVER_ERROR,
      /*expected_bucket_count=*/1);

  TasksFuture retry_future;
  client()->GetTasks("test-task-list-id", /*force_fetch=*/true,
                     retry_future.GetCallback());
  ASSERT_TRUE(retry_future.Wait());

  const auto [retry_success, retry_error, retry_root_tasks] =
      retry_future.Take();
  EXPECT_TRUE(retry_success);

  EXPECT_EQ(retry_root_tasks->item_count(), 1u);
  EXPECT_EQ(retry_root_tasks->GetItemAt(0)->id, "qwe");

  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Tasks.GetTasks.Latency", /*expected_count=*/3);
  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Tasks.GetTasks.Status", /*expected_count=*/3);
  histogram_tester()->ExpectBucketCount(
      "Ash.Glanceables.Api.Tasks.GetTasks.Status", ApiErrorCode::HTTP_SUCCESS,
      /*expected_bucket_count=*/2);
}

TEST_F(TasksClientImplTest, GetTasksReturnsCachedResultsOnPartialHttpError) {
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  Not(HasSubstr("pageToken")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
          "kind": "tasks#tasks",
          "items": [{
            "id": "asd",
            "title": "Parent task, level 1",
            "status": "needsAction",
            "due": "2023-04-19T00:00:00.000Z"
          }, {
            "id": "zxc",
            "title": "Parent task 3, level 1",
            "status": "needsAction",
            "links": [{"type": "email"}]
          }]
      })"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"({
          "kind": "tasks#tasks",
          "items": [{
            "id": "qwe",
            "title": "Parent task, level 1",
            "status": "needsAction",
            "due": "2023-04-19T00:00:00.000Z"
          }],
          "nextPageToken": "tt"
      })"))));
  EXPECT_CALL(request_handler(),
              HandleRequest(
                  Field(&HttpRequest::relative_url, HasSubstr("pageToken=tt"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateFailedResponse())));

  TasksFuture future;
  client()->GetTasks("task-list-1", /*force_fetch=*/false,
                     future.GetCallback());
  ASSERT_TRUE(future.Wait());

  const auto [success, http_error, tasks] = future.Take();
  EXPECT_TRUE(success);

  EXPECT_EQ(tasks->item_count(), 2u);
  EXPECT_EQ(tasks->GetItemAt(0)->id, "asd");
  EXPECT_EQ(tasks->GetItemAt(1)->id, "zxc");

  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Tasks.GetTasks.Latency", /*expected_count=*/1);
  histogram_tester()->ExpectUniqueSample(
      "Ash.Glanceables.Api.Tasks.GetTasks.Status", ApiErrorCode::HTTP_SUCCESS,
      /*expected_bucket_count=*/1);

  client()->OnGlanceablesBubbleClosed(base::DoNothing());

  TasksFuture failure_future;
  client()->GetTasks("task-list-1", /*force_fetch=*/true,
                     failure_future.GetCallback());
  ASSERT_TRUE(failure_future.Wait());

  const auto [failure_status, failure_error, failed_tasks] =
      failure_future.Take();
  EXPECT_FALSE(failure_status);

  EXPECT_EQ(failed_tasks->item_count(), 2u);
  EXPECT_EQ(failed_tasks->GetItemAt(0)->id, "asd");
  EXPECT_EQ(failed_tasks->GetItemAt(1)->id, "zxc");

  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Tasks.GetTasks.Latency", /*expected_count=*/3);
  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Tasks.GetTasks.Status", 3);
  histogram_tester()->ExpectBucketCount(
      "Ash.Glanceables.Api.Tasks.GetTasks.Status", ApiErrorCode::HTTP_SUCCESS,
      /*expected_bucket_count=*/2);
  histogram_tester()->ExpectBucketCount(
      "Ash.Glanceables.Api.Tasks.GetTasks.Status",
      ApiErrorCode::HTTP_INTERNAL_SERVER_ERROR,
      /*expected_bucket_count=*/1);
}

TEST_F(TasksClientImplTest, GetTasksFetchesAllPages) {
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  Not(HasSubstr("pageToken")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#tasks",
            "items": [
              {
                "id": "child-task-from-page-1",
                "parent": "parent-task-from-page-2"
              }
            ],
            "nextPageToken": "qwe"
          }
        )"))));
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  HasSubstr("pageToken=qwe"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#tasks",
            "items": [{"id": "parent-task-from-page-2"}],
            "nextPageToken": "asd"
          }
        )"))));
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  HasSubstr("pageToken=asd"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#tasks",
            "items": [{"id": "parent-task-from-page-3"}]
          }
        )"))));

  TasksFuture future;
  client()->GetTasks("test-task-list-id", /*force_fetch=*/false,
                     future.GetCallback());
  ASSERT_TRUE(future.Wait());

  const auto [success, http_error, root_tasks] = future.Take();
  EXPECT_TRUE(success);

  ASSERT_EQ(root_tasks->item_count(), 2u);

  EXPECT_EQ(root_tasks->GetItemAt(0)->id, "parent-task-from-page-2");
  EXPECT_TRUE(root_tasks->GetItemAt(0)->has_subtasks);

  EXPECT_EQ(root_tasks->GetItemAt(1)->id, "parent-task-from-page-3");
  EXPECT_FALSE(root_tasks->GetItemAt(1)->has_subtasks);

  histogram_tester()->ExpectUniqueSample(
      "Ash.Glanceables.Api.Tasks.GetTasks.PagesCount",
      /*sample=*/3,
      /*expected_bucket_count=*/1);
  histogram_tester()->ExpectUniqueSample(
      "Ash.Glanceables.Api.Tasks.RawTasksCount",
      /*sample=*/3,
      /*expected_bucket_count=*/1);
  histogram_tester()->ExpectUniqueSample(
      "Ash.Glanceables.Api.Tasks.ProcessedTasksCount",
      /*sample=*/2,
      /*expected_bucket_count=*/1);
}

TEST_F(TasksClientImplTest, GlanceablesBubbleClosedWhileFetchingTasksPage) {
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  Not(HasSubstr("pageToken")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#tasks",
            "items": [{"id": "task-from-page-1"}],
            "nextPageToken": "qwe"
          }
        )"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#tasks",
            "items": [{"id": "task-from-page-1-2"}],
            "nextPageToken": "qwe"
          }
        )"))));

  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  HasSubstr("pageToken=qwe"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#tasks",
            "items": [{"id": "task-from-page-2"}],
            "nextPageToken": "asd"
          }
        )"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#tasks",
            "items": [{"id": "task-from-page-2-2"}],
            "nextPageToken": "asd"
          }
        )"))));
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  HasSubstr("pageToken=asd"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#tasks",
            "items": [{"id": "task-from-page-3-2"}]
          }
        )"))));

  // Inject a test callback that will simulate glanceables bubble closure after
  // requesting second page of tasks.
  client()->set_tasks_request_callback_for_testing(base::BindLambdaForTesting(
      [&](const std::string& task_list_id, const std::string& page_token) {
        ASSERT_EQ("test-task-list-id", task_list_id);
        if (page_token == "qwe") {
          client()->OnGlanceablesBubbleClosed(base::DoNothing());
        }
      }));

  TasksFuture future;
  client()->GetTasks("test-task-list-id", /*force_fetch=*/false,
                     future.GetCallback());
  ASSERT_TRUE(future.Wait());

  // Expect an empty list, given that the glanceables bubble got closed before
  // all tasks were fetched, effectively cancelling the fetch.
  const auto [success, http_error, root_tasks] = future.Take();
  EXPECT_FALSE(success);

  ASSERT_EQ(root_tasks->item_count(), 0u);

  client()->set_tasks_request_callback_for_testing(
      TasksClientImpl::TasksRequestCallback());

  TasksFuture refresh_future;
  client()->GetTasks("test-task-list-id", /*force_fetch=*/false,
                     refresh_future.GetCallback());
  ASSERT_TRUE(refresh_future.Wait());

  const auto [refresh_success, refresh_error, refreshed_root_tasks] =
      refresh_future.Take();
  EXPECT_TRUE(refresh_success);

  ASSERT_EQ(refreshed_root_tasks->item_count(), 3u);
  EXPECT_EQ(refreshed_root_tasks->GetItemAt(0)->id, "task-from-page-1-2");
  EXPECT_EQ(refreshed_root_tasks->GetItemAt(1)->id, "task-from-page-2-2");
  EXPECT_EQ(refreshed_root_tasks->GetItemAt(2)->id, "task-from-page-3-2");

  TasksFuture repeated_refresh_future;
  client()->GetTasks("test-task-list-id", /*force_fetch=*/false,
                     repeated_refresh_future.GetCallback());
  ASSERT_TRUE(repeated_refresh_future.Wait());

  const auto [repeated_refresh_success, repeated_refresh_error,
              repeated_refreshed_root_tasks] = repeated_refresh_future.Take();
  EXPECT_TRUE(repeated_refresh_success);

  ASSERT_EQ(repeated_refreshed_root_tasks->item_count(), 3u);
  EXPECT_EQ(repeated_refreshed_root_tasks->GetItemAt(0)->id,
            "task-from-page-1-2");
  EXPECT_EQ(repeated_refreshed_root_tasks->GetItemAt(1)->id,
            "task-from-page-2-2");
  EXPECT_EQ(repeated_refreshed_root_tasks->GetItemAt(2)->id,
            "task-from-page-3-2");
}

TEST_F(TasksClientImplTest, GetTasksSortsByPosition) {
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#tasks",
            "items": [
              {"title": "2nd", "position": "00000000000000000001"},
              {"title": "3rd", "position": "00000000000000000002"},
              {"title": "1st", "position": "00000000000000000000"}
            ]
          }
        )"))));

  TasksFuture future;
  client()->GetTasks("test-task-list-id", /*force_fetch=*/false,
                     future.GetCallback());
  ASSERT_TRUE(future.Wait());

  const auto [success, http_error, root_tasks] = future.Take();
  EXPECT_TRUE(success);

  ASSERT_EQ(root_tasks->item_count(), 3u);

  EXPECT_EQ(root_tasks->GetItemAt(0)->title, "1st");
  EXPECT_EQ(root_tasks->GetItemAt(1)->title, "2nd");
  EXPECT_EQ(root_tasks->GetItemAt(2)->title, "3rd");
}

TEST_F(TasksClientImplTest, GetTasksHandlesOriginSurfaceType) {
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#tasks",
            "items": [
              {"id": "1"},
              {"id": "2", "assignmentInfo": {"surfaceType": "DOCUMENT"}},
              {"id": "3", "assignmentInfo": {"surfaceType": "SPACE"}},
              {"id": "4", "assignmentInfo": {"surfaceType": "UNKNOWN"}}
            ]
          }
        )"))));

  TasksFuture future;
  client()->GetTasks("test-task-list-id", /*force_fetch=*/false,
                     future.GetCallback());
  ASSERT_TRUE(future.Wait());

  const auto [success, http_error, root_tasks] = future.Take();
  EXPECT_TRUE(success);

  ASSERT_EQ(root_tasks->item_count(), 4u);

  EXPECT_EQ(root_tasks->GetItemAt(0)->origin_surface_type,
            api::Task::OriginSurfaceType::kRegular);
  EXPECT_EQ(root_tasks->GetItemAt(1)->origin_surface_type,
            api::Task::OriginSurfaceType::kDocument);
  EXPECT_EQ(root_tasks->GetItemAt(2)->origin_surface_type,
            api::Task::OriginSurfaceType::kSpace);
  EXPECT_EQ(root_tasks->GetItemAt(3)->origin_surface_type,
            api::Task::OriginSurfaceType::kUnknown);
}

// ----------------------------------------------------------------------------
// Mark as completed:

TEST_F(TasksClientImplTest, MarkAsCompleted) {
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(&HttpRequest::method, Eq(HttpMethod::METHOD_GET))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#tasks",
            "items": [
              {
                "id": "task-1",
                "status": "needsAction"
              },
              {
                "id": "task-2",
                "status": "needsAction"
              }
            ]
          }
        )"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(&HttpRequest::method, Eq(HttpMethod::METHOD_PATCH))))
      .Times(2)
      .WillRepeatedly(Invoke([](const HttpRequest&) {
        return TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#task",
            "id": "task-id",
            "title": "Updated title",
            "status": "completed"
          }
        )");
      }));

  TasksFuture get_tasks_future;
  client()->GetTasks("test-task-list-id", /*force_fetch=*/false,
                     get_tasks_future.GetCallback());
  ASSERT_TRUE(get_tasks_future.Wait());

  const auto [success, http_error, tasks] = get_tasks_future.Take();
  EXPECT_TRUE(success);

  EXPECT_EQ(tasks->item_count(), 2u);

  TestFuture<bool> mark_as_completed_future;
  client()->MarkAsCompleted("test-task-list-id", "task-1", true);
  client()->MarkAsCompleted("test-task-list-id", "task-2", true);

  TestFuture<void> glanceables_bubble_closed_future;
  client()->OnGlanceablesBubbleClosed(
      glanceables_bubble_closed_future.GetCallback());
  ASSERT_TRUE(glanceables_bubble_closed_future.Wait());

  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Tasks.PatchTask.Latency", /*expected_count=*/2);
  histogram_tester()->ExpectUniqueSample(
      "Ash.Glanceables.Api.Tasks.PatchTask.Status", ApiErrorCode::HTTP_SUCCESS,
      /*expected_bucket_count=*/2);
  histogram_tester()->ExpectUniqueSample(
      "Ash.Glanceables.Api.Tasks.SimultaneousMarkAsCompletedRequestsCount",
      /*sample=*/2,
      /*expected_bucket_count=*/1);
}

TEST_F(TasksClientImplTest, MarkAsCompletedOnHttpError) {
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(&HttpRequest::method, Eq(HttpMethod::METHOD_GET))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#tasks",
            "items": [
              {
                "id": "task-1",
                "status": "needsAction"
              },
              {
                "id": "task-2",
                "status": "needsAction"
              }
            ]
          }
        )"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(&HttpRequest::method, Eq(HttpMethod::METHOD_PATCH))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateFailedResponse())));

  TasksFuture get_tasks_future;
  client()->GetTasks("test-task-list-id", /*force_fetch=*/false,
                     get_tasks_future.GetCallback());
  ASSERT_TRUE(get_tasks_future.Wait());

  const auto [success, http_error, tasks] = get_tasks_future.Take();
  EXPECT_TRUE(success);

  EXPECT_EQ(tasks->item_count(), 2u);

  client()->MarkAsCompleted("test-task-list-id", "task-2", true);
  EXPECT_EQ(tasks->item_count(), 2u);

  TestFuture<void> glanceables_bubble_closed_future;
  client()->OnGlanceablesBubbleClosed(
      glanceables_bubble_closed_future.GetCallback());
  ASSERT_TRUE(glanceables_bubble_closed_future.Wait());

  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Tasks.PatchTask.Latency", /*expected_count=*/1);
  histogram_tester()->ExpectUniqueSample(
      "Ash.Glanceables.Api.Tasks.PatchTask.Status",
      ApiErrorCode::HTTP_INTERNAL_SERVER_ERROR, /*expected_bucket_count=*/1);
}

// ----------------------------------------------------------------------------
// Add a new task:

TEST_F(TasksClientImplTest, AddsNewTask) {
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(&HttpRequest::method, Eq(HttpMethod::METHOD_GET))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#tasks",
            "items": [
              {
                "id": "task-id",
                "title": "Task 1",
                "status": "needsAction"
              }
            ]
          }
        )"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(&HttpRequest::method, Eq(HttpMethod::METHOD_POST))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#task",
            "id": "new-task-id",
            "title": "New task"
          }
        )"))));

  TasksFuture get_tasks_future;
  client()->GetTasks("test-task-list-id", /*force_fetch=*/false,
                     get_tasks_future.GetCallback());
  ASSERT_TRUE(get_tasks_future.Wait());

  const auto [success, http_error, tasks] = get_tasks_future.Take();
  EXPECT_TRUE(success);

  EXPECT_EQ(tasks->item_count(), 1u);
  EXPECT_EQ(tasks->GetItemAt(0)->id, "task-id");
  EXPECT_EQ(tasks->GetItemAt(0)->title, "Task 1");

  testing::StrictMock<TestListModelObserver> observer;
  tasks->AddObserver(&observer);
  EXPECT_CALL(observer, ListItemsAdded(/*start=*/0, /*count=*/1));

  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Tasks.InsertTask.Latency", /*expected_count=*/0);
  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Tasks.InsertTask.Status", /*expected_count=*/0);

  TestFuture<ApiErrorCode, const Task*> add_task_future;
  client()->AddTask("test-task-list-id", "New task",
                    add_task_future.GetCallback());

  ASSERT_TRUE(add_task_future.Wait());
  const auto [new_error, new_task] = add_task_future.Take();
  EXPECT_EQ(new_task->id, "new-task-id");
  EXPECT_EQ(new_task->title, "New task");

  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Tasks.InsertTask.Latency", /*expected_count=*/1);
  histogram_tester()->ExpectUniqueSample(
      "Ash.Glanceables.Api.Tasks.InsertTask.Status", ApiErrorCode::HTTP_SUCCESS,
      /*expected_bucket_count=*/1);
}

// ----------------------------------------------------------------------------
// Update a task:

TEST_F(TasksClientImplTest, UpdatesTask) {
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(&HttpRequest::method, Eq(HttpMethod::METHOD_GET))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#tasks",
            "items": [
              {
                "id": "task-id",
                "title": "Task 1",
                "status": "needsAction"
              }
            ]
          }
        )"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(&HttpRequest::method, Eq(HttpMethod::METHOD_PATCH))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#task",
            "id": "task-id",
            "title": "Updated title",
            "status": "completed"
          }
        )"))));

  // Get tasks first.
  TasksFuture get_tasks_future;
  client()->GetTasks("task-list-id", /*force_fetch=*/false,
                     get_tasks_future.GetCallback());
  ASSERT_TRUE(get_tasks_future.Wait());
  const auto [success, http_error, tasks] = get_tasks_future.Take();
  EXPECT_TRUE(success);

  ASSERT_EQ(tasks->item_count(), 1u);
  EXPECT_EQ(tasks->GetItemAt(0)->title, "Task 1");

  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Tasks.PatchTask.Latency", /*expected_count=*/0);
  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Tasks.PatchTask.Status", /*expected_count=*/0);

  // Update the task.
  TestFuture<ApiErrorCode, const Task*> update_task_future;
  client()->UpdateTask("task-list-id", "task-id", "Updated title",
                       /*completed=*/true, update_task_future.GetCallback());
  ASSERT_TRUE(update_task_future.Wait());

  // Make sure `tasks` contains the update.
  EXPECT_EQ(tasks->GetItemAt(0), std::get<1>(update_task_future.Take()));
  EXPECT_EQ(tasks->GetItemAt(0)->title, "Updated title");
  EXPECT_EQ(tasks->GetItemAt(0)->completed, true);

  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Tasks.PatchTask.Latency", /*expected_count=*/1);
  histogram_tester()->ExpectUniqueSample(
      "Ash.Glanceables.Api.Tasks.PatchTask.Status", ApiErrorCode::HTTP_SUCCESS,
      /*expected_bucket_count=*/1);
}

TEST_F(TasksClientImplTest, UpdatesTaskOnHttpError) {
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(&HttpRequest::method, Eq(HttpMethod::METHOD_PATCH))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateFailedResponse())));

  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Tasks.PatchTask.Latency", /*expected_count=*/0);
  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Tasks.PatchTask.Status", /*expected_count=*/0);

  TestFuture<ApiErrorCode, const Task*> update_task_future;
  client()->UpdateTask("task-list-id", "task-id", "Updated title",
                       /*completed=*/false, update_task_future.GetCallback());

  ASSERT_TRUE(update_task_future.Wait());
  EXPECT_FALSE(std::get<1>(update_task_future.Take()));

  histogram_tester()->ExpectTotalCount(
      "Ash.Glanceables.Api.Tasks.PatchTask.Latency", /*expected_count=*/1);
  histogram_tester()->ExpectUniqueSample(
      "Ash.Glanceables.Api.Tasks.PatchTask.Status",
      ApiErrorCode::HTTP_INTERNAL_SERVER_ERROR,
      /*expected_bucket_count=*/1);
}

}  // namespace ash::api
