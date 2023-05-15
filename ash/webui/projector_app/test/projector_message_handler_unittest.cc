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
#include "content/public/test/test_web_ui.h"
#include "net/http/http_status_code.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::_;

const char kVideoFileId[] = "video_file_id";
const char kResourceKey[] = "resource_key";

const char kWebUIResponse[] = "cr.webUIResponse";
const char kGetVideoCallback[] = "getVideoCallback";

}  // namespace

namespace ash {

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
    message_handler_ = std::make_unique<ProjectorMessageHandler>();
    message_handler_->set_web_ui_for_test(&web_ui());
    message_handler_->RegisterMessages();
  }

  void TearDown() override { message_handler_.reset(); }

  const content::TestWebUI::CallData& FetchCallData(int sequence_number) {
    return *(web_ui().call_data()[sequence_number]);
  }

  ProjectorMessageHandler* message_handler() { return message_handler_.get(); }
  content::TestWebUI& web_ui() { return web_ui_; }
  MockProjectorController& controller() { return mock_controller_; }
  MockAppClient& mock_app_client() { return mock_app_client_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  std::unique_ptr<ProjectorMessageHandler> message_handler_;
  MockProjectorController mock_controller_;
  MockAppClient mock_app_client_;
  content::TestWebUI web_ui_;
};

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

}  // namespace ash
