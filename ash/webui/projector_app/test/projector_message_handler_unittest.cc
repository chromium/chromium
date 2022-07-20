// Copyright 2021 The Chromium Authors. All rights reserved.
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
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestUserEmail[] = "testuser1@gmail.com";

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
const char kGetNewScreencastPreconditionCallback[] =
    "getNewScreencastPreconditionCallback";
const char kStartProjectorSessionCallback[] = "startProjectorSessionCallback";
const char kGetOAuthTokenCallback[] = "getOAuthTokenCallback";
const char kSendXhrCallback[] = "sendXhrCallback";
const char kOnNewScreencastPreconditionChanged[] =
    "onNewScreencastPreconditionChanged";
const char kOnSodaInstallProgressUpdated[] = "onSodaInstallProgressUpdated";
const char kOnSodaInstalled[] = "onSodaInstalled";
const char kOnSodaInstallError[] = "onSodaInstallError";
const char kGetScreencastCallback[] = "getScreencastCallback";

const char kShouldDownloadSodaCallback[] = "shouldDownloadSodaCallbck";
const char kInstallSodaCallback[] = "installSodaCallback";
const char kGetPendingScreencastsCallback[] = "getPendingScreencastsCallback";

const char kOpenFeedbackDialogCallback[] = "openFeedbackDialog";

const char kSetUserPrefCallback[] = "setUserPrefCallback";
const char kGetUserPrefCallback[] = "getUserPrefCallback";

constexpr char kRejectedRequestMessage[] = "Request Rejected";
constexpr char kRejectedRequestMessageKey[] = "message";
constexpr char kRejectedRequestArgsKey[] = "requestArgs";

constexpr char kState[] = "state";
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
  base::ListValue list_args;
  list_args.Append(kGetAccountsCallback);

  web_ui().HandleReceivedMessage("getAccounts", &list_args);

  // We expect that there was only one callback to the WebUI.
  EXPECT_EQ(web_ui().call_data().size(), 1u);

  const content::TestWebUI::CallData& call_data = FetchCallData(0);

  EXPECT_EQ(call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(call_data.arg1()->GetString(), kGetAccountsCallback);

  // Whether the callback was rejected or not.
  EXPECT_TRUE(call_data.arg2()->GetBool());
  ASSERT_TRUE(call_data.arg3()->is_list());

  const auto& list_view = call_data.arg3()->GetListDeprecated();
  // There is only one account in the identity manager.
  EXPECT_EQ(list_view.size(), 1u);

  // Ensure that the entry is an account with a the valid email.
  const auto& account = list_view[0];
  const std::string* email = account.FindStringPath("email");
  ASSERT_NE(email, nullptr);
  EXPECT_EQ(*email, kTestUserEmail);
}

TEST_F(ProjectorMessageHandlerUnitTest, CanStartProjectorSession) {
  NewScreencastPrecondition precondition;
  precondition.state = NewScreencastPreconditionState::kEnabled;

  EXPECT_CALL(controller(), GetNewScreencastPrecondition());
  ON_CALL(controller(), GetNewScreencastPrecondition)
      .WillByDefault(testing::Return(precondition));

  base::ListValue list_args;
  list_args.Append(kGetNewScreencastPreconditionCallback);

  web_ui().HandleReceivedMessage("getNewScreencastPreconditionState",
                                 &list_args);

  // We expect that there was only one callback to the WebUI.
  EXPECT_EQ(web_ui().call_data().size(), 1u);

  const content::TestWebUI::CallData& call_data = FetchCallData(0);
  EXPECT_EQ(call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(call_data.arg1()->GetString(),
            kGetNewScreencastPreconditionCallback);
  EXPECT_TRUE(call_data.arg2()->GetBool());
  const auto* args = call_data.arg3();
  EXPECT_EQ(*(args->FindIntKey(kState)),
            static_cast<int>(NewScreencastPreconditionState::kEnabled));
}

TEST_F(ProjectorMessageHandlerUnitTest, GetOAuthTokenForAccount) {
  mock_app_client().SetAutomaticIssueOfAccessTokens(false);

  base::ListValue list_args;
  list_args.Append(kGetOAuthTokenCallback);
  base::ListValue args;
  args.Append(kTestUserEmail);
  list_args.Append(std::move(args));

  web_ui().HandleReceivedMessage("getOAuthTokenForAccount", &list_args);
  mock_app_client().WaitForAccessRequest(kTestUserEmail);

  EXPECT_EQ(web_ui().call_data().size(), 1u);

  const content::TestWebUI::CallData& call_data = FetchCallData(0);
  EXPECT_EQ(call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(call_data.arg1()->GetString(), kGetOAuthTokenCallback);
}

TEST_F(ProjectorMessageHandlerUnitTest, SendXhr) {
  const std::string& test_response_body = "{}";

  base::ListValue list_args;
  list_args.Append(kSendXhrCallback);
  base::ListValue args;
  args.Append(kTestXhrUrl);
  args.Append(kTestXhrMethod);
  args.Append(kTestXhrRequestBody);
  // Add useCredentials.
  args.Append(true);
  // Add additional headers.
  base::Value::Dict dict;
  dict.Set(kTestXhrHeaderKey, kTestXhrHeaderValue);
  args.Append(std::move(dict));
  list_args.Append(std::move(args));

  mock_app_client().test_url_loader_factory().AddResponse(kTestXhrUrl,
                                                          test_response_body);

  base::RunLoop run_loop;
  message_handler()->SetXhrRequestRunLoopQuitClosure(run_loop.QuitClosure());
  web_ui().HandleReceivedMessage("sendXhr", &list_args);
  run_loop.Run();

  EXPECT_EQ(web_ui().call_data().size(), 1u);

  const content::TestWebUI::CallData& call_data = FetchCallData(0);
  EXPECT_EQ(call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(call_data.arg1()->GetString(), kSendXhrCallback);

  // Whether the callback was rejected or not.
  EXPECT_TRUE(call_data.arg2()->GetBool());
  ASSERT_TRUE(call_data.arg3()->is_dict());

  // Verify that it is success.
  EXPECT_TRUE(call_data.arg3()->FindBoolPath(kXhrResponseSuccessPath));

  // Verify the response.
  const std::string* response =
      call_data.arg3()->FindStringPath(kXhrResponseStringPath);
  EXPECT_EQ(test_response_body, *response);

  // Verify error is empty.
  const std::string* error =
      call_data.arg3()->FindStringPath(kXhrResponseErrorPath);
  EXPECT_TRUE(error->empty());
}

TEST_F(ProjectorMessageHandlerUnitTest, SendXhrWithUnSupportedUrl) {
  base::ListValue list_args;
  list_args.Append(kSendXhrCallback);
  base::ListValue args;
  args.Append(kTestXhrUnsupportedUrl);
  args.Append(kTestXhrMethod);
  args.Append(kTestXhrRequestBody);
  // Add useCredentials.
  args.Append(true);
  // Add additional headers.
  base::Value::Dict dict;
  dict.Set(kTestXhrHeaderKey, kTestXhrHeaderValue);
  args.Append(std::move(dict));
  list_args.Append(std::move(args));

  base::RunLoop run_loop;
  message_handler()->SetXhrRequestRunLoopQuitClosure(run_loop.QuitClosure());
  web_ui().HandleReceivedMessage("sendXhr", &list_args);
  run_loop.Run();

  EXPECT_EQ(web_ui().call_data().size(), 1u);

  const content::TestWebUI::CallData& call_data = FetchCallData(0);
  EXPECT_EQ(call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(call_data.arg1()->GetString(), kSendXhrCallback);

  // Whether the callback was rejected or not.
  EXPECT_TRUE(call_data.arg2()->GetBool());
  ASSERT_TRUE(call_data.arg3()->is_dict());

  // Verify that it is success.
  EXPECT_TRUE(call_data.arg3()->FindBoolPath(kXhrResponseSuccessPath));

  // Verify the response.
  const std::string* response =
      call_data.arg3()->FindStringPath(kXhrResponseStringPath);
  EXPECT_TRUE(response->empty());

  // Verify error is empty.
  const std::string* error =
      call_data.arg3()->FindStringPath(kXhrResponseErrorPath);
  EXPECT_EQ("UNSUPPORTED_URL", *error);
}

TEST_F(ProjectorMessageHandlerUnitTest, NewScreencastPreconditionChanged) {
  NewScreencastPrecondition precondition;
  precondition.state = NewScreencastPreconditionState::kEnabled;
  message_handler()->OnNewScreencastPreconditionChanged(precondition);
  const content::TestWebUI::CallData& call_data = *(web_ui().call_data()[0]);
  EXPECT_EQ(call_data.function_name(), kWebUIListenerCall);
  EXPECT_EQ(call_data.arg1()->GetString(), kOnNewScreencastPreconditionChanged);
  EXPECT_EQ(*(call_data.arg2()), precondition.ToValue());
}

TEST_F(ProjectorMessageHandlerUnitTest, OnSodaProgress) {
  static_cast<ProjectorAppClient::Observer*>(message_handler())
      ->OnSodaProgress(50);
  const content::TestWebUI::CallData& call_data = FetchCallData(0);
  EXPECT_EQ(call_data.function_name(), kWebUIListenerCall);
  EXPECT_EQ(call_data.arg1()->GetString(), kOnSodaInstallProgressUpdated);
  EXPECT_EQ(call_data.arg2()->GetInt(), 50);
}

TEST_F(ProjectorMessageHandlerUnitTest, OnSodaInstalled) {
  static_cast<ProjectorAppClient::Observer*>(message_handler())
      ->OnSodaInstalled();
  const content::TestWebUI::CallData& call_data = FetchCallData(0);
  EXPECT_EQ(call_data.function_name(), kWebUIListenerCall);
  EXPECT_EQ(call_data.arg1()->GetString(), kOnSodaInstalled);
}

TEST_F(ProjectorMessageHandlerUnitTest, OnSodaError) {
  static_cast<ProjectorAppClient::Observer*>(message_handler())->OnSodaError();
  const content::TestWebUI::CallData& call_data = FetchCallData(0);
  EXPECT_EQ(call_data.function_name(), kWebUIListenerCall);
  EXPECT_EQ(call_data.arg1()->GetString(), kOnSodaInstallError);
}

TEST_F(ProjectorMessageHandlerUnitTest, ShouldDownloadSoda) {
  ON_CALL(mock_app_client(), ShouldDownloadSoda())
      .WillByDefault(testing::Return(true));

  base::ListValue list_args;
  list_args.Append(base::Value(kShouldDownloadSodaCallback));

  web_ui().HandleReceivedMessage("shouldDownloadSoda", &list_args);

  const content::TestWebUI::CallData& call_data = FetchCallData(0);
  EXPECT_EQ(call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(call_data.arg1()->GetString(), kShouldDownloadSodaCallback);
  EXPECT_EQ(call_data.arg2()->GetBool(), true);
  EXPECT_EQ(call_data.arg3()->GetBool(), true);
}

TEST_F(ProjectorMessageHandlerUnitTest, InstallSoda) {
  ON_CALL(mock_app_client(), InstallSoda()).WillByDefault(testing::Return());

  base::ListValue list_args;
  list_args.Append(base::Value(kInstallSodaCallback));

  web_ui().HandleReceivedMessage("installSoda", &list_args);

  const content::TestWebUI::CallData& call_data = FetchCallData(0);
  EXPECT_EQ(call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(call_data.arg1()->GetString(), kInstallSodaCallback);
  EXPECT_EQ(call_data.arg2()->GetBool(), true);
  EXPECT_EQ(call_data.arg3()->GetBool(), true);
}

TEST_F(ProjectorMessageHandlerUnitTest, GetPendingScreencasts) {
  const std::string name = "test_pending_screecast";
  const std::string path = "/root/projector_data/test_pending_screecast";
  const PendingScreencastSet expectedScreencasts{ash::PendingScreencast{
      /*container_dir=*/base::FilePath(path), /*name=*/name,
      /*total_size_in_bytes=*/1, /*bytes_untransferred=*/0}};

  ON_CALL(mock_app_client(), GetPendingScreencasts())
      .WillByDefault(testing::ReturnRef(expectedScreencasts));

  base::ListValue list_args;
  list_args.Append(kGetPendingScreencastsCallback);

  web_ui().HandleReceivedMessage("getPendingScreencasts", &list_args);

  // We expect that there was only one callback to the WebUI.
  EXPECT_EQ(web_ui().call_data().size(), 1u);

  const content::TestWebUI::CallData& call_data = FetchCallData(0);

  EXPECT_EQ(call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(call_data.arg1()->GetString(), kGetPendingScreencastsCallback);

  // Whether the callback was rejected or not.
  EXPECT_TRUE(call_data.arg2()->GetBool());
  ASSERT_TRUE(call_data.arg3()->is_list());

  const auto& list_view = call_data.arg3()->GetListDeprecated();
  // There is only one screencast.
  EXPECT_EQ(list_view.size(), 1u);

  const auto& screencast = list_view[0];
  EXPECT_EQ(*(screencast.FindStringPath("name")), name);
  EXPECT_EQ(*(screencast.FindDoublePath("createdTime")), 0);
  EXPECT_EQ(*(screencast.FindBoolPath("uploadFailed")), false);
}

TEST_F(ProjectorMessageHandlerUnitTest, OnScreencastsStateChange) {
  message_handler()->OnScreencastsPendingStatusChanged(PendingScreencastSet());
  ExpectCallToWebUI(kWebUIListenerCall, "onScreencastsStateChange",
                    /*call_count=*/1u);
}

TEST_F(ProjectorMessageHandlerUnitTest, CreationFlowEnabled) {
  base::ListValue list_args;
  list_args.Append(base::Value(kSetUserPrefCallback));

  base::ListValue func_args;
  func_args.Append(base::Value(ash::prefs::kProjectorCreationFlowEnabled));
  func_args.Append(base::Value(true));
  list_args.Append(std::move(func_args));

  web_ui().HandleReceivedMessage("setUserPref", &list_args);

  const content::TestWebUI::CallData& call_data = FetchCallData(0);
  EXPECT_EQ(call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(call_data.arg1()->GetString(), kSetUserPrefCallback);
  EXPECT_EQ(call_data.arg2()->GetBool(), true);

  // Now let's try to read the user's pref.
  list_args.ClearList();
  list_args.Append(base::Value(kGetUserPrefCallback));
  func_args.ClearList();
  func_args.Append(ash::prefs::kProjectorCreationFlowEnabled);
  list_args.Append(std::move(func_args));

  web_ui().HandleReceivedMessage("getUserPref", &list_args);

  const content::TestWebUI::CallData& get_pref_call_data = FetchCallData(1);
  EXPECT_EQ(get_pref_call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(get_pref_call_data.arg1()->GetString(), kGetUserPrefCallback);
  EXPECT_EQ(get_pref_call_data.arg2()->GetBool(), true);

  const base::Value* args = get_pref_call_data.arg3();
  EXPECT_TRUE(args->is_bool());
  EXPECT_TRUE(args->GetBool());
}

TEST_F(ProjectorMessageHandlerUnitTest, ExcludeTranscriptDialogShownPref) {
  base::ListValue list_args;
  list_args.Append(base::Value(kSetUserPrefCallback));

  base::ListValue func_args;
  func_args.Append(
      base::Value(ash::prefs::kProjectorExcludeTranscriptDialogShown));
  func_args.Append(base::Value(true));
  list_args.Append(std::move(func_args));

  web_ui().HandleReceivedMessage("setUserPref", &list_args);

  const content::TestWebUI::CallData& call_data = FetchCallData(0);
  EXPECT_EQ(call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(call_data.arg1()->GetString(), kSetUserPrefCallback);
  EXPECT_EQ(call_data.arg2()->GetBool(), true);

  // Now let's try to read the user's pref.
  list_args.ClearList();
  list_args.Append(base::Value(kGetUserPrefCallback));
  func_args.ClearList();
  func_args.Append(ash::prefs::kProjectorExcludeTranscriptDialogShown);
  list_args.Append(std::move(func_args));

  web_ui().HandleReceivedMessage("getUserPref", &list_args);

  const content::TestWebUI::CallData& get_pref_call_data = FetchCallData(1);
  EXPECT_EQ(get_pref_call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(get_pref_call_data.arg1()->GetString(), kGetUserPrefCallback);
  EXPECT_EQ(get_pref_call_data.arg2()->GetBool(), true);

  const base::Value* args = get_pref_call_data.arg3();
  EXPECT_TRUE(args->is_bool());
  EXPECT_TRUE(args->GetBool());
}

TEST_F(ProjectorMessageHandlerUnitTest, SetCreationFlowEnabledInvalidValue) {
  base::ListValue list_args;
  list_args.Append(base::Value(kSetUserPrefCallback));

  base::ListValue func_args;
  func_args.Append(ash::prefs::kProjectorCreationFlowEnabled);

  // The value provided is not a boolean. Therefore it will fail.
  func_args.Append(base::Value("temp"));
  list_args.Append(func_args.Clone());

  web_ui().HandleReceivedMessage("setUserPref", &list_args);

  const content::TestWebUI::CallData& call_data = FetchCallData(0);
  EXPECT_EQ(call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(call_data.arg1()->GetString(), kSetUserPrefCallback);

  // The request is rejected.
  EXPECT_EQ(call_data.arg2()->GetBool(), false);

  // Validate the rejected message.
  const base::Value* rejected_args = call_data.arg3();
  EXPECT_EQ(*(rejected_args->FindStringPath(kRejectedRequestMessageKey)),
            kRejectedRequestMessage);
  EXPECT_EQ(*(rejected_args->FindPath(kRejectedRequestArgsKey)), func_args);
}

TEST_F(ProjectorMessageHandlerUnitTest, OpenFeedbackDialog) {
  base::ListValue list_args;
  list_args.Append(base::Value(kOpenFeedbackDialogCallback));

  web_ui().HandleReceivedMessage("openFeedbackDialog", &list_args);

  const content::TestWebUI::CallData& call_data = FetchCallData(0);
  EXPECT_EQ(call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(call_data.arg1()->GetString(), kOpenFeedbackDialogCallback);
}

TEST_F(ProjectorMessageHandlerUnitTest, SetCreationFlowEnabledUnsupportedPref) {
  base::ListValue list_args;
  list_args.Append(base::Value(kSetUserPrefCallback));

  base::ListValue func_args;
  func_args.Append("invalidUserPref");
  func_args.Append(base::Value(true));
  list_args.Append(func_args.Clone());

  web_ui().HandleReceivedMessage("setUserPref", &list_args);

  const content::TestWebUI::CallData& call_data = FetchCallData(0);
  EXPECT_EQ(call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(call_data.arg1()->GetString(), kSetUserPrefCallback);

  // Request is rejected.
  EXPECT_EQ(call_data.arg2()->GetBool(), false);

  // Validate the rejected message.
  const base::Value* rejected_args = call_data.arg3();
  EXPECT_EQ(*(rejected_args->FindStringPath(kRejectedRequestMessageKey)),
            kRejectedRequestMessage);
  EXPECT_EQ(*(rejected_args->FindPath(kRejectedRequestArgsKey)), func_args);
}

TEST_F(ProjectorMessageHandlerUnitTest, GetScreencast) {
  base::ListValue list_args;
  list_args.Append(kGetScreencastCallback);
  base::ListValue args;
  const std::string container_folder_id = "test_container_id";
  args.Append(container_folder_id);

  list_args.Append(std::move(args));

  web_ui().HandleReceivedMessage("getScreencast", &list_args);

  EXPECT_EQ(web_ui().call_data().size(), 1u);

  const content::TestWebUI::CallData& call_data = FetchCallData(0);
  EXPECT_EQ(call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(call_data.arg1()->GetString(), kGetScreencastCallback);

  // Whether the callback was rejected or not.
  EXPECT_TRUE(call_data.arg2()->GetBool());
  ASSERT_TRUE(call_data.arg3()->is_dict());
  const base::Value::Dict& dict = std::move(call_data.arg3()->GetDict());

  EXPECT_EQ(*dict.FindString("containerFolderId"), container_folder_id);
  // TODO(b/236857019) Updates the |name| value when getting screencast name by
  // using DriveFS service.
  EXPECT_EQ(*dict.FindString("name"), "name");
  EXPECT_EQ(*dict.FindDict("video"), ash::ProjectorScreencastVideo().ToValue());
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

  base::ListValue list_args;
  list_args.Append(kStartProjectorSessionCallback);
  base::ListValue args;
  args.Append(std::get<0>(GetParam()));
  list_args.Append(std::move(args));

  web_ui().HandleReceivedMessage("startProjectorSession", &list_args);

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

  base::ListValue list_args;
  list_args.Append(kStartProjectorSessionCallback);
  base::ListValue args;
  args.Append("folderId");
  list_args.Append(std::move(args));

  web_ui().HandleReceivedMessage("startProjectorSession", &list_args);

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
  base::ListValue set_list_args;
  set_list_args.Append(base::Value(kSetUserPrefCallback));
  base::ListValue func_args;
  func_args.Append(base::Value(GetParam()));
  func_args.Append(base::Value(5));
  set_list_args.Append(std::move(func_args));

  // Set the value of the preference passed to the test as a parameter.
  web_ui().HandleReceivedMessage("setUserPref", &set_list_args);

  const content::TestWebUI::CallData& set_call_data = FetchCallData(0);
  EXPECT_EQ(set_call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(set_call_data.arg1()->GetString(), kSetUserPrefCallback);

  // Check that setUserPref succeeded.
  EXPECT_EQ(set_call_data.arg2()->GetBool(), true);

  // Fetch the pref just set
  base::ListValue get_list_args;
  get_list_args.Append(base::Value(kGetUserPrefCallback));
  base::ListValue get_func_args;
  get_func_args.Append(base::Value(GetParam()));
  get_list_args.Append(std::move(get_func_args));
  web_ui().HandleReceivedMessage("getUserPref", &get_list_args);

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
