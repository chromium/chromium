// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/projector_app/projector_message_handler.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/projector/projector_new_screencast_precondition.h"
#include "ash/public/cpp/test/mock_projector_controller.h"
#include "ash/webui/projector_app/projector_screencast.h"
#include "ash/webui/projector_app/projector_xhr_sender.h"
#include "ash/webui/projector_app/test/mock_app_client.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/test_web_ui.h"
#include "net/http/http_status_code.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::_;

const char kTestUserEmail[] = "testuser1@gmail.com";
const char kVideoFileId[] = "video_file_id";
const char kResourceKey[] = "resource_key";

const char kTestXhrUrl[] = "https://www.googleapis.com/drive/v3/files/fileID";
const char kTestXhrUnsupportedUrl[] = "https://www.example.com";
const char kTestXhrMethod[] = "POST";
const char kTestXhrRequestBody[] = "{}";
const char kTestXhrHeaderKey[] = "X-Goog-Drive-Resource-Keys";
const char kTestXhrHeaderValue[] = "resource-key";

const char kXhrResponseSuccessPath[] = "success";
const char kXhrResponseErrorPath[] = "error";
const char kXhrResponseStringPath[] = "response";

const char kWebUIListenerCall[] = "cr.webUIListenerCallback";
const char kWebUIResponse[] = "cr.webUIResponse";
const char kGetAccountsCallback[] = "getAccountsCallback";
const char kStartProjectorSessionCallback[] = "startProjectorSessionCallback";
const char kGetOAuthTokenCallback[] = "getOAuthTokenCallback";
const char kSendXhrCallback[] = "sendXhrCallback";
const char kGetVideoCallback[] = "getVideoCallback";

const char kGetPendingScreencastsCallback[] = "getPendingScreencastsCallback";

const char kOpenFeedbackDialogCallback[] = "openFeedbackDialog";

const char kSetUserPrefCallback[] = "setUserPrefCallback";
const char kGetUserPrefCallback[] = "getUserPrefCallback";

constexpr char kRejectedRequestMessage[] = "Request Rejected";
constexpr char kRejectedRequestMessageKey[] = "message";
constexpr char kRejectedRequestArgsKey[] = "requestArgs";
}  // namespace

namespace ash {

class ProjectorMessageHandlerForTest : public ProjectorMessageHandler {
 public:
  explicit ProjectorMessageHandlerForTest(PrefService* pref_service)
      : ProjectorMessageHandler(pref_service) {}
  ProjectorMessageHandlerForTest(const ProjectorMessageHandlerForTest&) =
      delete;
  ProjectorMessageHandlerForTest& operator=(
      const ProjectorMessageHandlerForTest&) = delete;
  ~ProjectorMessageHandlerForTest() override = default;

  // ProjectorMessageHandler:
  void OnXhrRequestCompleted(const std::string& js_callback_id,
                             bool success,
                             const std::string& response_body,
                             const std::string& error) override {
    ProjectorMessageHandler::OnXhrRequestCompleted(js_callback_id, success,
                                                   response_body, error);
    std::move(quit_closure_).Run();
  }

  void SetXhrRequestRunLoopQuitClosure(base::RepeatingClosure closure) {
    quit_closure_ = base::BindOnce(closure);
  }

 private:
  base::OnceClosure quit_closure_;
};

class ProjectorMessageHandlerUnitTest : public testing::Test {
 public:
  ProjectorMessageHandlerUnitTest() = default;
  ProjectorMessageHandlerUnitTest(const ProjectorMessageHandlerUnitTest&) =
      delete;
  ProjectorMessageHandlerUnitTest& operator=(
      const ProjectorMessageHandlerUnitTest&) = delete;
  ~ProjectorMessageHandlerUnitTest() override = default;

  // testing::Test
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

    message_handler_ =
        std::make_unique<ProjectorMessageHandlerForTest>(&pref_service_);
    message_handler_->set_web_ui_for_test(&web_ui());
    message_handler_->RegisterMessages();
  }

  void TearDown() override { message_handler_.reset(); }

  void ExpectCallToWebUI(const std::string& type,
                         const std::string& func_name,
                         size_t count) {
    EXPECT_EQ(web_ui().call_data().size(), count);
    const content::TestWebUI::CallData& call_data = FetchCallData(0);
    EXPECT_EQ(call_data.function_name(), type);
    EXPECT_EQ(call_data.arg1()->GetString(), func_name);
  }

  const content::TestWebUI::CallData& FetchCallData(int sequence_number) {
    return *(web_ui().call_data()[sequence_number]);
  }

  ProjectorMessageHandlerForTest* message_handler() {
    return message_handler_.get();
  }
  content::TestWebUI& web_ui() { return web_ui_; }
  MockProjectorController& controller() { return mock_controller_; }
  MockAppClient& mock_app_client() { return mock_app_client_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  std::unique_ptr<ProjectorMessageHandlerForTest> message_handler_;
  MockProjectorController mock_controller_;
  MockAppClient mock_app_client_;
  content::TestWebUI web_ui_;
  TestingPrefServiceSimple pref_service_;
};

TEST_F(ProjectorMessageHandlerUnitTest, GetAccounts) {
  base::Value::List list_args;
  list_args.Append(kGetAccountsCallback);

  web_ui().HandleReceivedMessage("getAccounts", list_args);

  // We expect that there was only one callback to the WebUI.
  EXPECT_EQ(web_ui().call_data().size(), 1u);

  const content::TestWebUI::CallData& call_data = FetchCallData(0);

  EXPECT_EQ(call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(call_data.arg1()->GetString(), kGetAccountsCallback);

  // Whether the callback was rejected or not.
  EXPECT_TRUE(call_data.arg2()->GetBool());
  ASSERT_TRUE(call_data.arg3()->is_list());

  const base::Value::List& list = call_data.arg3()->GetList();
  // There is only one account in the identity manager.
  EXPECT_EQ(list.size(), 1u);

  // Ensure that the entry is an account with a the valid email.
  const auto& account = list[0].GetDict();
  const std::string* email = account.FindString("email");
  ASSERT_NE(email, nullptr);
  EXPECT_EQ(*email, kTestUserEmail);
}

TEST_F(ProjectorMessageHandlerUnitTest, GetOAuthTokenForAccount) {
  mock_app_client().SetAutomaticIssueOfAccessTokens(false);

  base::Value::List list_args;
  list_args.Append(kGetOAuthTokenCallback);
  base::Value::List args;
  args.Append(kTestUserEmail);
  list_args.Append(std::move(args));

  web_ui().HandleReceivedMessage("getOAuthTokenForAccount", list_args);
  mock_app_client().WaitForAccessRequest(kTestUserEmail);

  EXPECT_EQ(web_ui().call_data().size(), 1u);

  const content::TestWebUI::CallData& call_data = FetchCallData(0);
  EXPECT_EQ(call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(call_data.arg1()->GetString(), kGetOAuthTokenCallback);
}

TEST_F(ProjectorMessageHandlerUnitTest, SendXhr) {
  const std::string& test_response_body = "{}";

  base::Value::List list_args;
  list_args.Append(kSendXhrCallback);
  base::Value::List args;
  args.Append(kTestXhrUrl);
  args.Append(kTestXhrMethod);
  args.Append(kTestXhrRequestBody);
  // Add useCredentials.
  args.Append(true);
  // Add useApiKey.
  args.Append(false);
  // Add additional headers.
  base::Value::Dict dict;
  dict.Set(kTestXhrHeaderKey, kTestXhrHeaderValue);
  args.Append(std::move(dict));
  args.Append(base::Value());
  list_args.Append(std::move(args));

  mock_app_client().test_url_loader_factory().AddResponse(kTestXhrUrl,
                                                          test_response_body);

  base::RunLoop run_loop;
  message_handler()->SetXhrRequestRunLoopQuitClosure(run_loop.QuitClosure());
  web_ui().HandleReceivedMessage("sendXhr", list_args);
  mock_app_client().WaitForAccessRequest(kTestUserEmail);

  run_loop.Run();

  EXPECT_EQ(web_ui().call_data().size(), 1u);

  const content::TestWebUI::CallData& call_data = FetchCallData(0);
  EXPECT_EQ(call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(call_data.arg1()->GetString(), kSendXhrCallback);

  // Whether the callback was rejected or not.
  EXPECT_TRUE(call_data.arg2()->GetBool());
  ASSERT_TRUE(call_data.arg3()->is_dict());

  // Verify that it is success.
  const base::Value::Dict& arg3_dict = call_data.arg3()->GetDict();
  EXPECT_TRUE(*arg3_dict.FindBool(kXhrResponseSuccessPath));

  // Verify the response.
  const std::string* response = arg3_dict.FindString(kXhrResponseStringPath);
  EXPECT_EQ(test_response_body, *response);

  // Verify error is empty.
  const std::string* error = arg3_dict.FindString(kXhrResponseErrorPath);
  EXPECT_TRUE(error->empty());
}

TEST_F(ProjectorMessageHandlerUnitTest, SendXhrWithEmail) {
  const std::string& test_response_body = "{}";

  base::Value::List list_args;
  list_args.Append(kSendXhrCallback);
  base::Value::List args;
  args.Append(kTestXhrUrl);
  args.Append(kTestXhrMethod);
  args.Append(kTestXhrRequestBody);
  // Add useCredentials.
  args.Append(true);
  // Add useApiKey.
  args.Append(false);
  // Add additional headers.
  base::Value::Dict dict;
  dict.Set(kTestXhrHeaderKey, kTestXhrHeaderValue);
  args.Append(std::move(dict));
  args.Append(kTestUserEmail);
  list_args.Append(std::move(args));

  mock_app_client().test_url_loader_factory().AddResponse(kTestXhrUrl,
                                                          test_response_body);

  base::RunLoop run_loop;
  message_handler()->SetXhrRequestRunLoopQuitClosure(run_loop.QuitClosure());
  web_ui().HandleReceivedMessage("sendXhr", list_args);
  mock_app_client().WaitForAccessRequest(kTestUserEmail);
  run_loop.Run();

  EXPECT_EQ(web_ui().call_data().size(), 1u);

  const content::TestWebUI::CallData& call_data = FetchCallData(0);
  EXPECT_EQ(call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(call_data.arg1()->GetString(), kSendXhrCallback);

  // Whether the callback was rejected or not.
  EXPECT_TRUE(call_data.arg2()->GetBool());
  ASSERT_TRUE(call_data.arg3()->is_dict());

  // Verify that it is success.
  const base::Value::Dict& arg3_dict = call_data.arg3()->GetDict();
  EXPECT_TRUE(*arg3_dict.FindBool(kXhrResponseSuccessPath));

  // Verify the response.
  const std::string* response = arg3_dict.FindString(kXhrResponseStringPath);
  EXPECT_EQ(test_response_body, *response);

  // Verify error is empty.
  const std::string* error = arg3_dict.FindString(kXhrResponseErrorPath);
  EXPECT_TRUE(error->empty());
}

TEST_F(ProjectorMessageHandlerUnitTest, SendXhrFailed) {
  const std::string& test_error_response_body = "error";

  base::Value::List list_args;
  list_args.Append(kSendXhrCallback);
  base::Value::List args;
  args.Append(kTestXhrUrl);
  args.Append(kTestXhrMethod);
  args.Append(kTestXhrRequestBody);
  // Add useCredentials.
  args.Append(true);
  // Add useApiKey.
  args.Append(false);
  // Add additional headers.
  base::Value::Dict dict;
  dict.Set(kTestXhrHeaderKey, kTestXhrHeaderValue);
  args.Append(std::move(dict));
  args.Append(kTestUserEmail);
  list_args.Append(std::move(args));

  mock_app_client().test_url_loader_factory().AddResponse(
      /*url=*/kTestXhrUrl,
      /*content=*/test_error_response_body,
      /*status=*/net::HttpStatusCode::HTTP_NOT_FOUND);

  base::RunLoop run_loop;
  message_handler()->SetXhrRequestRunLoopQuitClosure(run_loop.QuitClosure());
  web_ui().HandleReceivedMessage("sendXhr", list_args);
  mock_app_client().WaitForAccessRequest(kTestUserEmail);

  run_loop.Run();

  EXPECT_EQ(web_ui().call_data().size(), 1u);

  const content::TestWebUI::CallData& call_data = FetchCallData(0);
  EXPECT_EQ(call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(call_data.arg1()->GetString(), kSendXhrCallback);

  // Whether the callback was rejected.
  EXPECT_TRUE(call_data.arg2()->GetBool());
  ASSERT_TRUE(call_data.arg3()->is_dict());

  // Verify that request failed.
  const base::Value::Dict& arg3_dict = call_data.arg3()->GetDict();
  EXPECT_FALSE(*arg3_dict.FindBool(kXhrResponseSuccessPath));

  // Verify the response.
  const std::string* response = arg3_dict.FindString(kXhrResponseStringPath);
  EXPECT_EQ(test_error_response_body, *response);

  // Verify error is empty.
  const std::string* error = arg3_dict.FindString(kXhrResponseErrorPath);
  EXPECT_EQ("XHR_FETCH_FAILURE", *error);
}

TEST_F(ProjectorMessageHandlerUnitTest, SendXhrWithUnSupportedUrl) {
  base::Value::List list_args;
  list_args.Append(kSendXhrCallback);
  base::Value::List args;
  args.Append(kTestXhrUnsupportedUrl);
  args.Append(kTestXhrMethod);
  args.Append(kTestXhrRequestBody);
  // Add useCredentials.
  args.Append(true);
  // Add useApiKey.
  args.Append(false);
  // Add additional headers.
  base::Value::Dict dict;
  dict.Set(kTestXhrHeaderKey, kTestXhrHeaderValue);
  args.Append(std::move(dict));
  args.Append(kTestUserEmail);
  list_args.Append(std::move(args));

  base::RunLoop run_loop;
  message_handler()->SetXhrRequestRunLoopQuitClosure(run_loop.QuitClosure());
  web_ui().HandleReceivedMessage("sendXhr", list_args);
  run_loop.Run();

  EXPECT_EQ(web_ui().call_data().size(), 1u);

  const content::TestWebUI::CallData& call_data = FetchCallData(0);
  EXPECT_EQ(call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(call_data.arg1()->GetString(), kSendXhrCallback);

  // Whether the callback was rejected or not.
  EXPECT_TRUE(call_data.arg2()->GetBool());
  ASSERT_TRUE(call_data.arg3()->is_dict());

  // Verify that it is success.
  const base::Value::Dict& arg3_dict = call_data.arg3()->GetDict();
  EXPECT_TRUE(arg3_dict.FindBool(kXhrResponseSuccessPath));

  // Verify the response.
  const std::string* response = arg3_dict.FindString(kXhrResponseStringPath);
  EXPECT_TRUE(response->empty());

  // Verify error is UNSUPPORTED_URL.
  const std::string* error = arg3_dict.FindString(kXhrResponseErrorPath);
  EXPECT_EQ("UNSUPPORTED_URL", *error);
}

TEST_F(ProjectorMessageHandlerUnitTest, GetPendingScreencasts) {
  const std::string name = "test_pending_screecast";
  const std::string path = "/root/projector_data/test_pending_screecast";
  const PendingScreencastSet expectedScreencasts{ash::PendingScreencast{
      /*container_dir=*/base::FilePath(path), /*name=*/name,
      /*total_size_in_bytes=*/1, /*bytes_untransferred=*/0}};

  ON_CALL(mock_app_client(), GetPendingScreencasts())
      .WillByDefault(testing::ReturnRef(expectedScreencasts));

  base::Value::List list_args;
  list_args.Append(kGetPendingScreencastsCallback);

  web_ui().HandleReceivedMessage("getPendingScreencasts", list_args);

  // We expect that there was only one callback to the WebUI.
  EXPECT_EQ(web_ui().call_data().size(), 1u);

  const content::TestWebUI::CallData& call_data = FetchCallData(0);

  EXPECT_EQ(call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(call_data.arg1()->GetString(), kGetPendingScreencastsCallback);

  // Whether the callback was rejected or not.
  EXPECT_TRUE(call_data.arg2()->GetBool());
  ASSERT_TRUE(call_data.arg3()->is_list());

  const base::Value::List& list = call_data.arg3()->GetList();
  // There is only one screencast.
  EXPECT_EQ(list.size(), 1u);

  const auto& screencast = list[0].GetDict();
  EXPECT_EQ(*screencast.FindString("name"), name);
  EXPECT_EQ(*screencast.FindDouble("createdTime"), 0);
  EXPECT_EQ(*screencast.FindBool("uploadFailed"), false);
}

TEST_F(ProjectorMessageHandlerUnitTest, OnScreencastsStateChange) {
  message_handler()->OnScreencastsPendingStatusChanged(PendingScreencastSet());
  ExpectCallToWebUI(kWebUIListenerCall, "onScreencastsStateChange",
                    /*call_count=*/1u);
}

TEST_F(ProjectorMessageHandlerUnitTest, CreationFlowEnabled) {
  base::Value::List list_args;
  list_args.Append(base::Value(kSetUserPrefCallback));

  base::Value::List func_args;
  func_args.Append(base::Value(ash::prefs::kProjectorCreationFlowEnabled));
  func_args.Append(base::Value(true));
  list_args.Append(std::move(func_args));

  web_ui().HandleReceivedMessage("setUserPref", list_args);

  const content::TestWebUI::CallData& call_data = FetchCallData(0);
  EXPECT_EQ(call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(call_data.arg1()->GetString(), kSetUserPrefCallback);
  EXPECT_EQ(call_data.arg2()->GetBool(), true);

  // Now let's try to read the user's pref.
  list_args.clear();
  list_args.Append(base::Value(kGetUserPrefCallback));
  func_args.clear();
  func_args.Append(ash::prefs::kProjectorCreationFlowEnabled);
  list_args.Append(std::move(func_args));

  web_ui().HandleReceivedMessage("getUserPref", list_args);

  const content::TestWebUI::CallData& get_pref_call_data = FetchCallData(1);
  EXPECT_EQ(get_pref_call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(get_pref_call_data.arg1()->GetString(), kGetUserPrefCallback);
  EXPECT_EQ(get_pref_call_data.arg2()->GetBool(), true);

  const base::Value* args = get_pref_call_data.arg3();
  EXPECT_TRUE(args->is_bool());
  EXPECT_TRUE(args->GetBool());
}

TEST_F(ProjectorMessageHandlerUnitTest, ExcludeTranscriptDialogShownPref) {
  base::Value::List list_args;
  list_args.Append(base::Value(kSetUserPrefCallback));

  base::Value::List func_args;
  func_args.Append(
      base::Value(ash::prefs::kProjectorExcludeTranscriptDialogShown));
  func_args.Append(base::Value(true));
  list_args.Append(std::move(func_args));

  web_ui().HandleReceivedMessage("setUserPref", list_args);

  const content::TestWebUI::CallData& call_data = FetchCallData(0);
  EXPECT_EQ(call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(call_data.arg1()->GetString(), kSetUserPrefCallback);
  EXPECT_EQ(call_data.arg2()->GetBool(), true);

  // Now let's try to read the user's pref.
  list_args.clear();
  list_args.Append(base::Value(kGetUserPrefCallback));
  func_args.clear();
  func_args.Append(ash::prefs::kProjectorExcludeTranscriptDialogShown);
  list_args.Append(std::move(func_args));

  web_ui().HandleReceivedMessage("getUserPref", list_args);

  const content::TestWebUI::CallData& get_pref_call_data = FetchCallData(1);
  EXPECT_EQ(get_pref_call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(get_pref_call_data.arg1()->GetString(), kGetUserPrefCallback);
  EXPECT_EQ(get_pref_call_data.arg2()->GetBool(), true);

  const base::Value* args = get_pref_call_data.arg3();
  EXPECT_TRUE(args->is_bool());
  EXPECT_TRUE(args->GetBool());
}

TEST_F(ProjectorMessageHandlerUnitTest, SetCreationFlowEnabledInvalidValue) {
  base::Value::List list_args;
  list_args.Append(base::Value(kSetUserPrefCallback));

  base::Value::List func_args;
  func_args.Append(ash::prefs::kProjectorCreationFlowEnabled);

  // The value provided is not a boolean. Therefore it will fail.
  func_args.Append(base::Value("temp"));
  list_args.Append(func_args.Clone());

  web_ui().HandleReceivedMessage("setUserPref", list_args);

  const content::TestWebUI::CallData& call_data = FetchCallData(0);
  EXPECT_EQ(call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(call_data.arg1()->GetString(), kSetUserPrefCallback);

  // The request is rejected.
  EXPECT_EQ(call_data.arg2()->GetBool(), false);

  // Validate the rejected message.
  const base::Value::Dict& rejected_args = call_data.arg3()->GetDict();
  EXPECT_EQ(*(rejected_args.FindString(kRejectedRequestMessageKey)),
            kRejectedRequestMessage);
  EXPECT_EQ(*(rejected_args.Find(kRejectedRequestArgsKey)), func_args);
}

TEST_F(ProjectorMessageHandlerUnitTest, OpenFeedbackDialog) {
  base::Value::List list_args;
  list_args.Append(base::Value(kOpenFeedbackDialogCallback));

  web_ui().HandleReceivedMessage("openFeedbackDialog", list_args);

  const content::TestWebUI::CallData& call_data = FetchCallData(0);
  EXPECT_EQ(call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(call_data.arg1()->GetString(), kOpenFeedbackDialogCallback);
}

TEST_F(ProjectorMessageHandlerUnitTest, SetCreationFlowEnabledUnsupportedPref) {
  base::Value::List list_args;
  list_args.Append(base::Value(kSetUserPrefCallback));

  base::Value::List func_args;
  func_args.Append("invalidUserPref");
  func_args.Append(base::Value(true));
  list_args.Append(func_args.Clone());

  web_ui().HandleReceivedMessage("setUserPref", list_args);

  const content::TestWebUI::CallData& call_data = FetchCallData(0);
  EXPECT_EQ(call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(call_data.arg1()->GetString(), kSetUserPrefCallback);

  // Request is rejected.
  EXPECT_EQ(call_data.arg2()->GetBool(), false);

  // Validate the rejected message.
  const base::Value::Dict& rejected_args = call_data.arg3()->GetDict();
  EXPECT_EQ(*(rejected_args.FindString(kRejectedRequestMessageKey)),
            kRejectedRequestMessage);
  EXPECT_EQ(*(rejected_args.Find(kRejectedRequestArgsKey)), func_args);
}

TEST_F(ProjectorMessageHandlerUnitTest, GetVideo) {
  ProjectorScreencastVideo expected_video;
  expected_video.file_id = kVideoFileId;

  EXPECT_CALL(mock_app_client(), GetVideo(kVideoFileId, kResourceKey, _))
      .WillOnce(
          [&expected_video](const std::string& video_file_id,
                            const std::string& resource_key,
                            ProjectorAppClient::OnGetVideoCallback callback) {
            std::move(callback).Run(
                std::make_unique<ProjectorScreencastVideo>(expected_video),
                /*error_message=*/std::string());
          });

  base::Value::List list_args;
  list_args.Append(kGetVideoCallback);
  base::Value::List args;
  args.Append(kVideoFileId);
  args.Append(kResourceKey);
  list_args.Append(std::move(args));

  web_ui().HandleReceivedMessage("getVideo", list_args);

  // We expect that there was only one callback to the WebUI.
  EXPECT_EQ(web_ui().call_data().size(), 1u);

  const content::TestWebUI::CallData& call_data = FetchCallData(0);
  EXPECT_EQ(call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(call_data.arg1()->GetString(), kGetVideoCallback);

  // Expect the callback to be successful.
  EXPECT_TRUE(call_data.arg2()->GetBool());
  ASSERT_TRUE(call_data.arg3()->is_dict());
  EXPECT_EQ(call_data.arg3()->GetDict(), expected_video.ToValue());
}

TEST_F(ProjectorMessageHandlerUnitTest, GetVideoFail) {
  EXPECT_CALL(mock_app_client(), GetVideo(kVideoFileId, _, _))
      .WillOnce([](const std::string& video_file_id,
                   const std::string& resource_key,
                   ProjectorAppClient::OnGetVideoCallback callback) {
        EXPECT_TRUE(resource_key.empty());
        std::move(callback).Run(/*video=*/nullptr, /*error_message=*/"error1");
      });

  base::Value::List list_args;
  list_args.Append(kGetVideoCallback);
  base::Value::List args;
  args.Append(kVideoFileId);
  args.Append(base::Value());
  list_args.Append(std::move(args));

  web_ui().HandleReceivedMessage("getVideo", list_args);

  // We expect that there was only one callback to the WebUI.
  EXPECT_EQ(web_ui().call_data().size(), 1u);

  const content::TestWebUI::CallData& call_data = FetchCallData(0);
  EXPECT_EQ(call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(call_data.arg1()->GetString(), kGetVideoCallback);

  // Expect the callback to fail.
  EXPECT_FALSE(call_data.arg2()->GetBool());
  EXPECT_EQ(call_data.arg3()->GetString(), "error1");
}

class ProjectorStorageDirNameValidationTest
    : public ::testing::WithParamInterface<
          ::testing::tuple<::std::string, bool>>,
      public ProjectorMessageHandlerUnitTest {
 public:
  ProjectorStorageDirNameValidationTest() = default;
  ProjectorStorageDirNameValidationTest(
      const ProjectorStorageDirNameValidationTest&) = delete;
  ProjectorStorageDirNameValidationTest& operator=(
      const ProjectorStorageDirNameValidationTest&) = delete;
  ~ProjectorStorageDirNameValidationTest() override = default;
};

TEST_P(ProjectorStorageDirNameValidationTest, StorageDirNameBackSlash) {
  bool success = std::get<1>(GetParam());
  if (success) {
    EXPECT_CALL(controller(), GetNewScreencastPrecondition());
    ON_CALL(controller(), GetNewScreencastPrecondition)
        .WillByDefault(testing::Return(NewScreencastPrecondition(
            NewScreencastPreconditionState::kEnabled, {})));
  }

  base::Value::List list_args;
  list_args.Append(kStartProjectorSessionCallback);
  base::Value::List args;
  args.Append(std::get<0>(GetParam()));
  list_args.Append(std::move(args));

  web_ui().HandleReceivedMessage("startProjectorSession", list_args);

  // We expect that there was only one callback to the WebUI.
  EXPECT_EQ(web_ui().call_data().size(), 1u);
  const content::TestWebUI::CallData& call_data = FetchCallData(0);

  EXPECT_EQ(call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(call_data.arg1()->GetString(), kStartProjectorSessionCallback);
  EXPECT_TRUE(call_data.arg2()->GetBool());

  EXPECT_EQ(success, call_data.arg3()->GetBool());
}

INSTANTIATE_TEST_SUITE_P(
    StorageDirNameBackSlash,
    ProjectorStorageDirNameValidationTest,
    ::testing::Values(std::make_tuple("Projector recordings", true),
                      std::make_tuple("..\folderId", false),
                      std::make_tuple("../folderId", false)));

class ProjectorSessionStartUnitTest
    : public ::testing::WithParamInterface<NewScreencastPrecondition>,
      public ProjectorMessageHandlerUnitTest {
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

  bool success = precondition.state == NewScreencastPreconditionState::kEnabled;

  EXPECT_CALL(controller(), StartProjectorSession("folderId"))
      .Times(success ? 1 : 0);

  base::Value::List list_args;
  list_args.Append(kStartProjectorSessionCallback);
  base::Value::List args;
  args.Append("folderId");
  list_args.Append(std::move(args));

  web_ui().HandleReceivedMessage("startProjectorSession", list_args);

  // We expect that there was only one callback to the WebUI.
  EXPECT_EQ(web_ui().call_data().size(), 1u);
  const content::TestWebUI::CallData& call_data = FetchCallData(0);

  EXPECT_EQ(call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(call_data.arg1()->GetString(), kStartProjectorSessionCallback);
  EXPECT_TRUE(call_data.arg2()->GetBool());

  EXPECT_EQ(call_data.arg3()->GetBool(), success);
}

INSTANTIATE_TEST_SUITE_P(
    SessionStartSuccessFailTest,
    ProjectorSessionStartUnitTest,
    ::testing::Values(
        NewScreencastPrecondition(NewScreencastPreconditionState::kEnabled, {}),
        NewScreencastPrecondition(
            NewScreencastPreconditionState::kDisabled,
            {NewScreencastPreconditionReason::kInProjectorSession})));

// Tests getting and setting the Projector onboarding preferences.
// Parameterized by the preference strings.
class ProjectorOnboardingFlowPrefTest
    : public ::testing::WithParamInterface<const char*>,
      public ProjectorMessageHandlerUnitTest {
 public:
  ProjectorOnboardingFlowPrefTest() = default;
  ProjectorOnboardingFlowPrefTest(const ProjectorOnboardingFlowPrefTest&) =
      delete;
  ProjectorOnboardingFlowPrefTest& operator=(
      const ProjectorOnboardingFlowPrefTest&) = delete;
  ~ProjectorOnboardingFlowPrefTest() override = default;
};

TEST_P(ProjectorOnboardingFlowPrefTest, OnboardingFlowPrefTest) {
  // Set the user preference.
  base::Value::List set_list_args;
  set_list_args.Append(base::Value(kSetUserPrefCallback));
  base::Value::List func_args;
  func_args.Append(base::Value(GetParam()));
  func_args.Append(base::Value(5));
  set_list_args.Append(std::move(func_args));

  // Set the value of the preference passed to the test as a parameter.
  web_ui().HandleReceivedMessage("setUserPref", set_list_args);

  const content::TestWebUI::CallData& set_call_data = FetchCallData(0);
  EXPECT_EQ(set_call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(set_call_data.arg1()->GetString(), kSetUserPrefCallback);

  // Check that setUserPref succeeded.
  EXPECT_EQ(set_call_data.arg2()->GetBool(), true);

  // Fetch the pref just set
  base::Value::List get_list_args;
  get_list_args.Append(base::Value(kGetUserPrefCallback));
  base::Value::List get_func_args;
  get_func_args.Append(base::Value(GetParam()));
  get_list_args.Append(std::move(get_func_args));
  web_ui().HandleReceivedMessage("getUserPref", get_list_args);

  // Check that getUserPref succeeded.
  const content::TestWebUI::CallData& get_call_data = FetchCallData(1);
  EXPECT_EQ(get_call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(get_call_data.arg1()->GetString(), kGetUserPrefCallback);
  EXPECT_EQ(get_call_data.arg2()->GetBool(), true);
  EXPECT_EQ(get_call_data.arg3()->GetInt(), 5);
}

INSTANTIATE_TEST_SUITE_P(
    OnboardingPrefsTest,
    ProjectorOnboardingFlowPrefTest,
    ::testing::Values(ash::prefs::kProjectorGalleryOnboardingShowCount,
                      ash::prefs::kProjectorViewerOnboardingShowCount));

}  // namespace ash
