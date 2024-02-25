// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/projector_app/untrusted_projector_page_handler_impl.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/projector/projector_new_screencast_precondition.h"
#include "ash/public/cpp/test/mock_projector_controller.h"
#include "ash/webui/projector_app/mojom/untrusted_projector.mojom.h"
#include "ash/webui/projector_app/public/mojom/projector_types.mojom.h"
#include "ash/webui/projector_app/test/mock_app_client.h"
#include "base/files/safe_base_name.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-death-test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr char kTestUserEmail[] = "testuser1@gmail.com";
const char kVideoFileId[] = "video_file_id";
const char kResourceKey[] = "resource_key";

constexpr char kTestXhrUrl[] =
    "https://www.googleapis.com/drive/v3/files/fileID";
constexpr char kTestXhrUnsupportedUrl[] = "https://www.example.com";
constexpr ash::projector::mojom::RequestType kTestXhrMethod =
    ash::projector::mojom::RequestType::kPost;
constexpr char kTestXhrRequestBody[] = "{}";
constexpr char kTestXhrHeaderKey[] = "X-Goog-Drive-Resource-Keys";
constexpr char kTestXhrHeaderValue[] = "resource-key";
constexpr char kTestResponseBody[] = "{}";

// MOCK the Projector page instance in the WebUI renderer.
class MockUntrustedProjectorPageJs
    : public projector::mojom::UntrustedProjectorPage {
 public:
  MockUntrustedProjectorPageJs() = default;
  MockUntrustedProjectorPageJs(const MockUntrustedProjectorPageJs&) = delete;
  MockUntrustedProjectorPageJs& operator=(const MockUntrustedProjectorPageJs&) =
      delete;
  ~MockUntrustedProjectorPageJs() override = default;

  MOCK_METHOD1(OnNewScreencastPreconditionChanged,
               void(const NewScreencastPrecondition& precondition));
  MOCK_METHOD1(OnSodaInstallProgressUpdated, void(int32_t));
  MOCK_METHOD0(OnSodaInstalled, void());
  MOCK_METHOD0(OnSodaInstallError, void());
  MOCK_METHOD1(OnScreencastsStateChange,
               void(std::vector<projector::mojom::PendingScreencastPtr>
                        pending_screencasts));

  void FlushReceiverForTesting() { receiver_.FlushForTesting(); }

  void FlushRemoteForTesting() { page_handler_.FlushForTesting(); }

  mojo::Receiver<projector::mojom::UntrustedProjectorPage>& receiver() {
    return receiver_;
  }
  mojo::Remote<projector::mojom::UntrustedProjectorPageHandler>&
  page_handler() {
    return page_handler_;
  }

 private:
  mojo::Receiver<projector::mojom::UntrustedProjectorPage> receiver_{this};
  mojo::Remote<projector::mojom::UntrustedProjectorPageHandler> page_handler_;
};

}  // namespace

class UntrustedProjectorPageHandlerImplUnitTest : public testing::Test {
 public:
  UntrustedProjectorPageHandlerImplUnitTest() = default;
  UntrustedProjectorPageHandlerImplUnitTest(
      const UntrustedProjectorPageHandlerImplUnitTest&) = delete;
  UntrustedProjectorPageHandlerImplUnitTest& operator=(
      const UntrustedProjectorPageHandlerImplUnitTest&) = delete;
  ~UntrustedProjectorPageHandlerImplUnitTest() override = default;

  void SetUp() override {
    auto* registry = pref_service_.registry();
    registry->RegisterBooleanPref(ash::prefs::kProjectorCreationFlowEnabled,
                                  false);
    registry->RegisterBooleanPref(
        ash::prefs::kProjectorExcludeTranscriptDialogShown, false);
    registry->RegisterIntegerPref(
        ash::prefs::kProjectorGalleryOnboardingShowCount, 0);
    registry->RegisterIntegerPref(
        ash::prefs::kProjectorViewerOnboardingShowCount, 0);

    page_ = std::make_unique<MockUntrustedProjectorPageJs>();
    handler_impl_ = std::make_unique<UntrustedProjectorPageHandlerImpl>(
        page().page_handler().BindNewPipeAndPassReceiver(),
        page().receiver().BindNewPipeAndPassRemote(), &pref_service_);
  }

  void TearDown() override {
    handler_impl_.reset();
    page_.reset();
  }

  MockProjectorController& controller() { return mock_controller_; }
  MockUntrustedProjectorPageJs& page() { return *page_; }
  UntrustedProjectorPageHandlerImpl& handler() { return *handler_impl_; }
  MockAppClient& mock_app_client() { return mock_app_client_; }

 protected:
  void TestUserPref(projector::mojom::PrefsThatProjectorCanAskFor pref,
                    base::Value value);

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  MockProjectorController mock_controller_;
  MockAppClient mock_app_client_;
  std::unique_ptr<MockUntrustedProjectorPageJs> page_;
  std::unique_ptr<UntrustedProjectorPageHandlerImpl> handler_impl_;
  TestingPrefServiceSimple pref_service_;
};

void UntrustedProjectorPageHandlerImplUnitTest::TestUserPref(
    projector::mojom::PrefsThatProjectorCanAskFor pref,
    base::Value value) {
  base::test::TestFuture<void> set_pref_future;
  page().page_handler()->SetUserPref(pref, value.Clone(),
                                     set_pref_future.GetCallback());
  set_pref_future.Get();

  base::test::TestFuture<base::Value> get_pref_future;
  page().page_handler()->GetUserPref(pref, get_pref_future.GetCallback());

  EXPECT_EQ(get_pref_future.Get(), value);
}

TEST_F(UntrustedProjectorPageHandlerImplUnitTest, CanStartProjectorSession) {
  NewScreencastPrecondition precondition = NewScreencastPrecondition(
      NewScreencastPreconditionState::kEnabled,
      {NewScreencastPreconditionReason::kEnabledBySoda});

  ON_CALL(controller(), GetNewScreencastPrecondition)
      .WillByDefault(testing::Return(precondition));

  base::test::TestFuture<const NewScreencastPrecondition&>
      new_screencast_precondition_future;

  page().page_handler()->GetNewScreencastPrecondition(
      new_screencast_precondition_future.GetCallback());

  const auto& result = new_screencast_precondition_future.Get();
  EXPECT_EQ(result.state, ash::NewScreencastPreconditionState::kEnabled);
  EXPECT_EQ(result.reasons.size(), 1u);
  EXPECT_EQ(result.reasons[0],
            ash::NewScreencastPreconditionReason::kEnabledBySoda);
}

TEST_F(UntrustedProjectorPageHandlerImplUnitTest,
       NewScreencastPreconditionChanged) {
  EXPECT_CALL(page(), OnNewScreencastPreconditionChanged(testing::_)).Times(1);
  NewScreencastPrecondition precondition = NewScreencastPrecondition(
      NewScreencastPreconditionState::kEnabled,
      {NewScreencastPreconditionReason::kEnabledBySoda});
  handler().OnNewScreencastPreconditionChanged(precondition);
  page().FlushReceiverForTesting();
}

TEST_F(UntrustedProjectorPageHandlerImplUnitTest, OnSodaProgress) {
  EXPECT_CALL(page(), OnSodaInstallProgressUpdated(50)).Times(1);
  handler().OnSodaProgress(50);
  page().FlushReceiverForTesting();
}

TEST_F(UntrustedProjectorPageHandlerImplUnitTest, OnSodaInstalled) {
  EXPECT_CALL(page(), OnSodaInstalled()).Times(1);
  handler().OnSodaInstalled();
  page().FlushReceiverForTesting();
}

TEST_F(UntrustedProjectorPageHandlerImplUnitTest, OnSodaError) {
  EXPECT_CALL(page(), OnSodaInstallError()).Times(1);
  handler().OnSodaError();
  page().FlushReceiverForTesting();
}

TEST_F(UntrustedProjectorPageHandlerImplUnitTest, ShouldDownloadSoda) {
  ON_CALL(mock_app_client(), ShouldDownloadSoda())
      .WillByDefault(testing::Return(true));
  base::test::TestFuture<bool> should_download_soda_future;
  page().page_handler()->ShouldDownloadSoda(
      should_download_soda_future.GetCallback());
  EXPECT_TRUE(should_download_soda_future.Get());
}

TEST_F(UntrustedProjectorPageHandlerImplUnitTest, InstallSoda) {
  ON_CALL(mock_app_client(), InstallSoda()).WillByDefault(testing::Return());
  base::test::TestFuture<bool> install_triggered_future;
  page().page_handler()->InstallSoda(install_triggered_future.GetCallback());
  EXPECT_TRUE(install_triggered_future.Get());
}

TEST_F(UntrustedProjectorPageHandlerImplUnitTest, GetPendingScreencasts) {
  const std::string name = "test_pending_screencast";
  const std::string path = "/root/projector_data/test_pending_screencast";
  const PendingScreencastContainerSet expected_screencasts{
      ash::PendingScreencastContainer(
          /*container_dir=*/base::FilePath(path), /*name=*/name,
          /*total_size_in_bytes=*/1, /*bytes_transferred=*/0)};

  ON_CALL(mock_app_client(), GetPendingScreencasts())
      .WillByDefault(testing::ReturnRef(expected_screencasts));

  base::test::TestFuture<
      std::vector<ash::projector::mojom::PendingScreencastPtr>>
      install_triggered_future;

  page().page_handler()->GetPendingScreencasts(
      install_triggered_future.GetCallback());
  const auto& pending_screencasts = install_triggered_future.Get();
  EXPECT_EQ(pending_screencasts.size(), 1u);
  const auto& pending_screencast = pending_screencasts[0];
  EXPECT_EQ(pending_screencast->name, name);
  EXPECT_EQ(pending_screencast->upload_failed, false);
  EXPECT_EQ(pending_screencast->created_time, 0.0);
}

TEST_F(UntrustedProjectorPageHandlerImplUnitTest, OnScreencastsStateChange) {
  EXPECT_CALL(page(), OnScreencastsStateChange(testing::_)).Times(1);
  handler().OnScreencastsPendingStatusChanged(PendingScreencastContainerSet());
  page().FlushReceiverForTesting();
}

TEST_F(UntrustedProjectorPageHandlerImplUnitTest, TestPrefs) {
  TestUserPref(projector::mojom::PrefsThatProjectorCanAskFor::
                   kProjectorCreationFlowEnabled,
               /*value=*/base::Value(true));

  TestUserPref(projector::mojom::PrefsThatProjectorCanAskFor::
                   kProjectorExcludeTranscriptDialogShown,
               /*value=*/base::Value(true));

  TestUserPref(projector::mojom::PrefsThatProjectorCanAskFor::
                   kProjectorViewerOnboardingShowCount,
               /*value=*/base::Value(3));
  TestUserPref(projector::mojom::PrefsThatProjectorCanAskFor::
                   kProjectorGalleryOnboardingShowCount,
               /*value=*/base::Value(4));
}

TEST_F(UntrustedProjectorPageHandlerImplUnitTest, OpenFeedbackDialog) {
  EXPECT_CALL(mock_app_client(), OpenFeedbackDialog()).Times(1);
  base::test::TestFuture<void> open_feedback_future;
  page().page_handler()->OpenFeedbackDialog(open_feedback_future.GetCallback());
  EXPECT_TRUE(open_feedback_future.Wait());
}

class ProjectorSessionStartUnitTest
    : public ::testing::WithParamInterface<NewScreencastPrecondition>,
      public UntrustedProjectorPageHandlerImplUnitTest {
 public:
  ProjectorSessionStartUnitTest() = default;
  ProjectorSessionStartUnitTest(const ProjectorSessionStartUnitTest&) = delete;
  ProjectorSessionStartUnitTest& operator=(
      const ProjectorSessionStartUnitTest&) = delete;
  ~ProjectorSessionStartUnitTest() override = default;
};

TEST_P(ProjectorSessionStartUnitTest, ProjectorSessionTest) {
  const auto& precondition = GetParam();
  EXPECT_CALL(controller(), GetNewScreencastPrecondition());
  ON_CALL(controller(), GetNewScreencastPrecondition)
      .WillByDefault(testing::Return(precondition));

  bool expected_success =
      precondition.state == NewScreencastPreconditionState::kEnabled;

  const auto kFolderId = base::SafeBaseName::Create("folderId").value();

  EXPECT_CALL(controller(), StartProjectorSession(kFolderId))
      .Times(expected_success ? 1 : 0);

  base::test::TestFuture<bool> start_projector_session_future;
  page().page_handler()->StartProjectorSession(
      kFolderId, start_projector_session_future.GetCallback());
  EXPECT_EQ(start_projector_session_future.Get(), expected_success);
}

INSTANTIATE_TEST_SUITE_P(
    SessionStartSuccessFailTest,
    ProjectorSessionStartUnitTest,
    ::testing::Values(
        NewScreencastPrecondition(NewScreencastPreconditionState::kEnabled, {}),
        NewScreencastPrecondition(
            NewScreencastPreconditionState::kDisabled,
            {NewScreencastPreconditionReason::kInProjectorSession})));

TEST_F(UntrustedProjectorPageHandlerImplUnitTest, SafeBaseNameTest) {
  const auto valid_path = base::FilePath("folderName");
  const auto failing_path_1 = base::FilePath("parent1/folderName");
  const auto failing_path_2 = base::FilePath("../folderId");
  const auto failing_path_3 = base::FilePath("../");
  const auto failing_path_4 = base::FilePath("parent1/../../folderName");

  EXPECT_EQ(base::SafeBaseName::Create(valid_path)->path(), valid_path);
  EXPECT_NE(base::SafeBaseName::Create(failing_path_1)->path(), failing_path_1);
  EXPECT_NE(base::SafeBaseName::Create(failing_path_2)->path(), failing_path_2);
  EXPECT_NE(base::SafeBaseName::Create(failing_path_4)->path(), failing_path_4);

  // The safe base name would not even be created in this instance.
  EXPECT_FALSE(base::SafeBaseName::Create(failing_path_3));
}

TEST_F(UntrustedProjectorPageHandlerImplUnitTest, SendXhr) {
  mock_app_client().test_url_loader_factory().AddResponse(kTestXhrUrl,
                                                          kTestResponseBody);
  const base::flat_map<std::string, std::string> headers{
      {std::string(kTestXhrHeaderKey), std::string(kTestXhrHeaderValue)}};

  base::test::TestFuture<projector::mojom::XhrResponsePtr>
      send_xhr_request_future;
  page().page_handler()->SendXhr(
      GURL(kTestXhrUrl), kTestXhrMethod, kTestXhrRequestBody,
      /*use_credentials=*/true,
      /*use_api_key=*/false, headers,
      /*email=*/std::nullopt, send_xhr_request_future.GetCallback());
  mock_app_client().WaitForAccessRequest(kTestUserEmail);

  const auto& response = send_xhr_request_future.Get();
  EXPECT_EQ(response->response, kTestResponseBody);
  EXPECT_EQ(response->response_code,
            projector::mojom::XhrResponseCode::kSuccess);
}

TEST_F(UntrustedProjectorPageHandlerImplUnitTest, SendXhrEmptyEmail) {
  mock_app_client().test_url_loader_factory().AddResponse(kTestXhrUrl,
                                                          kTestResponseBody);
  const base::flat_map<std::string, std::string> headers{
      {std::string(kTestXhrHeaderKey), std::string(kTestXhrHeaderValue)}};

  base::test::TestFuture<projector::mojom::XhrResponsePtr>
      send_xhr_request_future;
  page().page_handler()->SendXhr(
      GURL(kTestXhrUrl), kTestXhrMethod, kTestXhrRequestBody,
      /*use_credentials=*/true,
      /*use_api_key=*/false, headers,
      /*email=*/"", send_xhr_request_future.GetCallback());
  mock_app_client().WaitForAccessRequest(kTestUserEmail);

  const auto& response = send_xhr_request_future.Get();
  EXPECT_EQ(response->response, kTestResponseBody);
  EXPECT_EQ(response->response_code,
            projector::mojom::XhrResponseCode::kSuccess);
}

TEST_F(UntrustedProjectorPageHandlerImplUnitTest, SendXhrWithEmail) {
  mock_app_client().test_url_loader_factory().AddResponse(kTestXhrUrl,
                                                          kTestResponseBody);
  const base::flat_map<std::string, std::string> headers{
      {std::string(kTestXhrHeaderKey), std::string(kTestXhrHeaderValue)}};
  base::test::TestFuture<projector::mojom::XhrResponsePtr>
      send_xhr_request_future;
  page().page_handler()->SendXhr(GURL(kTestXhrUrl), kTestXhrMethod,
                                 kTestXhrRequestBody,
                                 /*use_credentials=*/true,
                                 /*use_api_key=*/false, headers, kTestUserEmail,
                                 send_xhr_request_future.GetCallback());
  mock_app_client().WaitForAccessRequest(kTestUserEmail);

  const auto& response = send_xhr_request_future.Get();
  EXPECT_EQ(response->response, kTestResponseBody);
  EXPECT_EQ(response->response_code,
            projector::mojom::XhrResponseCode::kSuccess);
}

TEST_F(UntrustedProjectorPageHandlerImplUnitTest, SendXhrFailed) {
  constexpr char kTestErrorResponseBody[] = "error";
  mock_app_client().test_url_loader_factory().AddResponse(
      /*url=*/kTestXhrUrl,
      /*content=*/kTestErrorResponseBody,
      /*status=*/net::HttpStatusCode::HTTP_NOT_FOUND);
  const base::flat_map<std::string, std::string> headers{
      {std::string(kTestXhrHeaderKey), std::string(kTestXhrHeaderValue)}};
  base::test::TestFuture<projector::mojom::XhrResponsePtr>
      send_xhr_request_future;
  page().page_handler()->SendXhr(GURL(kTestXhrUrl), kTestXhrMethod,
                                 kTestXhrRequestBody,
                                 /*use_credentials=*/true,
                                 /*use_api_key=*/false, headers, kTestUserEmail,
                                 send_xhr_request_future.GetCallback());

  mock_app_client().WaitForAccessRequest(kTestUserEmail);
  const auto& response = send_xhr_request_future.Get();
  EXPECT_EQ(response->response, kTestErrorResponseBody);
  EXPECT_EQ(response->response_code,
            projector::mojom::XhrResponseCode::kXhrFetchFailure);
}

TEST_F(UntrustedProjectorPageHandlerImplUnitTest, SendXhrWithUnSupportedUrl) {
  auto crashing_lambda_test = [&]() {
    const base::flat_map<std::string, std::string> headers{
        {std::string(kTestXhrHeaderKey), std::string(kTestXhrHeaderValue)}};

    base::test::TestFuture<projector::mojom::XhrResponsePtr>
        send_xhr_request_future;

    page().page_handler()->SendXhr(
        GURL(kTestXhrUnsupportedUrl), kTestXhrMethod, kTestXhrRequestBody,
        /*use_credentials=*/true,
        /*use_api_key=*/false, headers, kTestUserEmail,
        send_xhr_request_future.GetCallback());

    const auto& response = send_xhr_request_future.Get();
    EXPECT_EQ(response->response_code,
              projector::mojom::XhrResponseCode::kUnsupportedURL);
  };

  EXPECT_DEATH_IF_SUPPORTED(crashing_lambda_test(), "");
}

TEST_F(UntrustedProjectorPageHandlerImplUnitTest, GetAccounts) {
  base::test::TestFuture<std::vector<projector::mojom::AccountPtr>>
      get_accounts_future;
  page().page_handler()->GetAccounts(get_accounts_future.GetCallback());
  const auto& accounts = get_accounts_future.Get();
  EXPECT_EQ(accounts.size(), 1u);
  EXPECT_EQ(accounts[0]->email, kTestUserEmail);
}

TEST_F(UntrustedProjectorPageHandlerImplUnitTest, GetVideo) {
  auto expected_video = projector::mojom::VideoInfo::New();
  expected_video->file_id = kVideoFileId;

  EXPECT_CALL(mock_app_client(),
              GetVideo(kVideoFileId, std::optional<std::string>(kResourceKey),
                       testing::_))
      .WillOnce([&expected_video](
                    const std::string& video_file_id,
                    const std::optional<std::string>& resource_key,
                    ProjectorAppClient::OnGetVideoCallback callback) {
        std::move(callback).Run(
            projector::mojom::GetVideoResult::NewVideo(expected_video.Clone()));
      });

  base::test::TestFuture<projector::mojom::GetVideoResultPtr> get_video_future;
  page().page_handler()->GetVideo(kVideoFileId, kResourceKey,
                                  get_video_future.GetCallback());

  const auto& result = get_video_future.Get<0>();
  EXPECT_FALSE(result->is_error_message());
  EXPECT_TRUE(result->is_video());
  EXPECT_EQ(result->get_video()->file_id, expected_video->file_id);
}

TEST_F(UntrustedProjectorPageHandlerImplUnitTest, GetVideoFail) {
  EXPECT_CALL(mock_app_client(), GetVideo(kVideoFileId, testing::_, testing::_))
      .WillOnce([](const std::string& video_file_id,
                   const std::optional<std::string>& resource_key,
                   ProjectorAppClient::OnGetVideoCallback callback) {
        EXPECT_FALSE(resource_key);
        std::move(callback).Run(
            projector::mojom::GetVideoResult::NewErrorMessage("error1"));
      });

  base::test::TestFuture<projector::mojom::GetVideoResultPtr> get_video_future;
  page().page_handler()->GetVideo(kVideoFileId, std::nullopt,
                                  get_video_future.GetCallback());

  const auto& result = get_video_future.Get<0>();
  EXPECT_TRUE(result->is_error_message());
  EXPECT_EQ(result->get_error_message(), "error1");
}

}  // namespace ash
