// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/glic_experimental_triggering/glic_experimental_triggering_converters.h"

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

TEST(GlicExperimentalTriggeringConvertersTest, TriggerActuationRequest) {
  components_sharing_message::GlicExperimentalTriggering proto;
  proto.set_glic_experimental_triggering_version(1);
  proto.set_context_id("test_context");

  auto* metadata = proto.mutable_task_metadata();
  metadata->set_conversation_id("conv_123");
  metadata->set_task_id("task_456");
  metadata->set_sender_sequence_number(42);
  metadata->set_last_seen_sequence_number(41);

  auto* parent_meta = metadata->mutable_parent_conversation_metadata();
  parent_meta->set_conversation_id("parent_conv");
  parent_meta->set_conversation_title("Parent Title");

  proto.mutable_request()
      ->mutable_trigger_actuation_request()
      ->set_initial_prompt("hello world");

  auto request = ProtoToRequest(proto);
  EXPECT_EQ(request.version, 1);
  EXPECT_EQ(request.context_id, "test_context");
  ASSERT_TRUE(request.task_metadata.has_value());
  EXPECT_EQ(request.task_metadata->conversation_id, "conv_123");
  EXPECT_EQ(request.task_metadata->task_id, "task_456");
  EXPECT_EQ(request.task_metadata->sender_sequence_number, 42);
  EXPECT_EQ(request.task_metadata->last_seen_sequence_number, 41);
  ASSERT_TRUE(request.task_metadata->parent_conversation_metadata.has_value());
  EXPECT_EQ(
      request.task_metadata->parent_conversation_metadata->conversation_id,
      "parent_conv");
  EXPECT_EQ(
      request.task_metadata->parent_conversation_metadata->conversation_title,
      "Parent Title");

  ASSERT_TRUE(std::holds_alternative<TriggerActuationRequest>(request.payload));
  EXPECT_EQ(std::get<TriggerActuationRequest>(request.payload).initial_prompt,
            "hello world");
}

TEST(GlicExperimentalTriggeringConvertersTest,
     ProtoToRequest_ParentConversationMetadata_EmptyFields) {
  {
    components_sharing_message::GlicExperimentalTriggering proto;
    auto* parent_meta =
        proto.mutable_task_metadata()->mutable_parent_conversation_metadata();
    parent_meta->set_conversation_id("p_conv");

    auto request = ProtoToRequest(proto);
    ASSERT_TRUE(request.task_metadata.has_value());
    ASSERT_TRUE(
        request.task_metadata->parent_conversation_metadata.has_value());
    EXPECT_EQ(
        request.task_metadata->parent_conversation_metadata->conversation_id,
        "p_conv");
    EXPECT_TRUE(request.task_metadata->parent_conversation_metadata
                    ->conversation_title.empty());
  }
  {
    components_sharing_message::GlicExperimentalTriggering proto;
    auto* parent_meta =
        proto.mutable_task_metadata()->mutable_parent_conversation_metadata();
    parent_meta->set_conversation_title("p_title");

    auto request = ProtoToRequest(proto);
    ASSERT_TRUE(request.task_metadata.has_value());
    ASSERT_TRUE(
        request.task_metadata->parent_conversation_metadata.has_value());
    EXPECT_TRUE(request.task_metadata->parent_conversation_metadata
                    ->conversation_id.empty());
    EXPECT_EQ(
        request.task_metadata->parent_conversation_metadata->conversation_title,
        "p_title");
  }
  {
    components_sharing_message::GlicExperimentalTriggering proto;
    proto.mutable_task_metadata()->mutable_parent_conversation_metadata();

    auto request = ProtoToRequest(proto);
    ASSERT_TRUE(request.task_metadata.has_value());
    ASSERT_TRUE(
        request.task_metadata->parent_conversation_metadata.has_value());
    EXPECT_TRUE(request.task_metadata->parent_conversation_metadata
                    ->conversation_id.empty());
    EXPECT_TRUE(request.task_metadata->parent_conversation_metadata
                    ->conversation_title.empty());
  }
  {
    components_sharing_message::GlicExperimentalTriggering proto;
    proto.mutable_task_metadata();  // parent_conversation_metadata not set

    auto request = ProtoToRequest(proto);
    ASSERT_TRUE(request.task_metadata.has_value());
    EXPECT_FALSE(
        request.task_metadata->parent_conversation_metadata.has_value());
  }
}

TEST(GlicExperimentalTriggeringConvertersTest, ContinueActuationRequest) {
  components_sharing_message::GlicExperimentalTriggering proto;
  proto.mutable_task_metadata()->set_conversation_id("conv_123");
  proto.mutable_request()
      ->mutable_continue_actuation_request()
      ->set_continuation_prompt("continue please");

  auto request = ProtoToRequest(proto);
  ASSERT_TRUE(
      std::holds_alternative<ContinueActuationRequest>(request.payload));
  EXPECT_EQ(
      std::get<ContinueActuationRequest>(request.payload).continuation_prompt,
      "continue please");
}

TEST(GlicExperimentalTriggeringConvertersTest, StopActuationRequest) {
  components_sharing_message::GlicExperimentalTriggering proto;
  proto.mutable_task_metadata()->set_conversation_id("conv_123");
  proto.mutable_request()->mutable_stop_actuation_request()->set_stop_reason(
      "STOPPED_BY_USER");

  auto request = ProtoToRequest(proto);
  ASSERT_TRUE(std::holds_alternative<StopActuationRequest>(request.payload));
  EXPECT_EQ(std::get<StopActuationRequest>(request.payload).stop_reason,
            "STOPPED_BY_USER");
}

TEST(GlicExperimentalTriggeringConvertersTest, DeviceOptInRequest) {
  components_sharing_message::GlicExperimentalTriggering proto;
  proto.mutable_task_metadata()->set_conversation_id("conv_123");
  proto.mutable_request()
      ->mutable_device_opt_in_request()
      ->set_triggering_source("SETTINGS");

  auto request = ProtoToRequest(proto);
  ASSERT_TRUE(std::holds_alternative<DeviceOptInRequest>(request.payload));
  EXPECT_EQ(std::get<DeviceOptInRequest>(request.payload).triggering_source,
            "SETTINGS");
}

TEST(GlicExperimentalTriggeringConvertersTest, GetScreenshotRequest) {
  components_sharing_message::GlicExperimentalTriggering proto;
  proto.mutable_task_metadata()->set_conversation_id("conv_123");
  auto* req = proto.mutable_request()->mutable_get_screenshot_request();
  req->set_public_key("pubkey_bytes");
  req->set_auth_secret("secret_bytes");

  auto request = ProtoToRequest(proto);
  ASSERT_TRUE(std::holds_alternative<GetScreenshotRequest>(request.payload));
  const auto& screenshot_req = std::get<GetScreenshotRequest>(request.payload);
  EXPECT_EQ(screenshot_req.public_key,
            std::vector<uint8_t>(
                {'p', 'u', 'b', 'k', 'e', 'y', '_', 'b', 'y', 't', 'e', 's'}));
  EXPECT_EQ(screenshot_req.auth_secret,
            std::vector<uint8_t>(
                {'s', 'e', 'c', 'r', 'e', 't', '_', 'b', 'y', 't', 'e', 's'}));
}

TEST(GlicExperimentalTriggeringConvertersTest, TaskMetadataUpdated) {
  components_sharing_message::GlicExperimentalTriggering proto;
  proto.mutable_task_metadata()->set_conversation_id("conv_123");
  proto.mutable_task_metadata_updated();

  auto request = ProtoToRequest(proto);
  ASSERT_TRUE(std::holds_alternative<TaskMetadataUpdated>(request.payload));
}

TEST(GlicExperimentalTriggeringConvertersTest, RequestPayloadNotSet) {
  components_sharing_message::GlicExperimentalTriggering proto;
  proto.mutable_task_metadata()->set_conversation_id("conv_123");
  proto.mutable_request();  // request oneof not set

  auto request = ProtoToRequest(proto);
  ASSERT_TRUE(std::holds_alternative<RequestPayloadNotSet>(request.payload));
}

TEST(GlicExperimentalTriggeringConvertersTest, MonostatePayload) {
  components_sharing_message::GlicExperimentalTriggering proto;
  proto.mutable_task_metadata()->set_conversation_id("conv_123");
  // neither request nor task_metadata_updated set

  auto request = ProtoToRequest(proto);
  ASSERT_TRUE(std::holds_alternative<std::monostate>(request.payload));
}

TEST(GlicExperimentalTriggeringConvertersTest,
     AbsentVersionAndSequenceNumbers) {
  components_sharing_message::GlicExperimentalTriggering proto;
  proto.mutable_task_metadata()->set_conversation_id("conv_123");
  proto.mutable_request()->mutable_trigger_actuation_request();

  auto request = ProtoToRequest(proto);
  EXPECT_TRUE(request.context_id.empty());
  EXPECT_FALSE(request.version.has_value());
  ASSERT_TRUE(request.task_metadata.has_value());
  EXPECT_FALSE(request.task_metadata->sender_sequence_number.has_value());
  EXPECT_FALSE(request.task_metadata->last_seen_sequence_number.has_value());
}

TEST(GlicExperimentalTriggeringConvertersTest, ResponseToProto_TaskUpdate) {
  ExperimentalTriggeringResponse response;
  response.context_id = "test_context";
  TaskMetadata task_metadata;
  task_metadata.conversation_id = "conv_123";
  task_metadata.task_id = "task_456";
  task_metadata.sender_sequence_number = 100;
  task_metadata.last_seen_sequence_number = 99;
  task_metadata.parent_conversation_metadata = ParentConversationMetadata{
      .conversation_id = "p_conv",
      .conversation_title = "p_title",
  };
  response.task_metadata = std::move(task_metadata);
  response.task_update = TaskUpdate{
      .state = TaskUpdate::State::kRunning,
      .data_type = TaskUpdate::DataType::kWorklog,
      .data = "step 1",
  };

  auto sharing_message = ResponseToProto(response);
  EXPECT_FALSE(sharing_message.has_server_channel_configuration());

  const auto& proto = sharing_message.glic_experimental_triggering();
  EXPECT_EQ(proto.context_id(), "test_context");
  EXPECT_EQ(proto.task_metadata().conversation_id(), "conv_123");
  EXPECT_EQ(proto.task_metadata().task_id(), "task_456");
  EXPECT_EQ(proto.task_metadata().sender_sequence_number(), 100);
  EXPECT_EQ(proto.task_metadata().last_seen_sequence_number(), 99);
  EXPECT_EQ(
      proto.task_metadata().parent_conversation_metadata().conversation_id(),
      "p_conv");
  EXPECT_EQ(
      proto.task_metadata().parent_conversation_metadata().conversation_title(),
      "p_title");

  const auto& resp_proto = proto.response();
  EXPECT_EQ(resp_proto.task_update().state(),
            components_sharing_message::GlicExperimentalTriggering::
                ExperimentalTriggeringResponse::TaskUpdate::RUNNING);
  EXPECT_EQ(resp_proto.task_update().data_type(),
            components_sharing_message::GlicExperimentalTriggering::
                ExperimentalTriggeringResponse::TaskUpdate::WORKLOG);
  EXPECT_EQ(resp_proto.task_update().data(), "step 1");
}

TEST(GlicExperimentalTriggeringConvertersTest,
     ResponseToProto_TaskUpdate_Resumed) {
  ExperimentalTriggeringResponse response;
  response.context_id = "test_resumed";
  TaskMetadata task_metadata;
  task_metadata.conversation_id = "conv_123";
  response.task_metadata = std::move(task_metadata);
  response.task_update = TaskUpdate{
      .state = TaskUpdate::State::kResumed,
  };

  auto sharing_message = ResponseToProto(response);
  const auto& proto = sharing_message.glic_experimental_triggering();
  const auto& resp_proto = proto.response();
  EXPECT_EQ(resp_proto.task_update().state(),
            components_sharing_message::GlicExperimentalTriggering::
                ExperimentalTriggeringResponse::TaskUpdate::RESUMED);
}

TEST(GlicExperimentalTriggeringConvertersTest, MissingTaskMetadata) {
  components_sharing_message::GlicExperimentalTriggering proto;
  proto.mutable_request()->mutable_trigger_actuation_request();

  auto request = ProtoToRequest(proto);
  EXPECT_FALSE(request.task_metadata.has_value());
}

TEST(GlicExperimentalTriggeringConvertersTest,
     ResponseToProto_ParentConversationMetadata_EmptyFields) {
  {
    ExperimentalTriggeringResponse response;
    TaskMetadata task_metadata;
    task_metadata.parent_conversation_metadata = ParentConversationMetadata{
        .conversation_id = "",
        .conversation_title = "p_title",
    };
    response.task_metadata = std::move(task_metadata);

    auto sharing_message = ResponseToProto(response);
    const auto& parent_meta = sharing_message.glic_experimental_triggering()
                                  .task_metadata()
                                  .parent_conversation_metadata();
    EXPECT_FALSE(parent_meta.has_conversation_id());
    EXPECT_TRUE(parent_meta.has_conversation_title());
    EXPECT_EQ(parent_meta.conversation_title(), "p_title");
  }
  {
    ExperimentalTriggeringResponse response;
    TaskMetadata task_metadata;
    task_metadata.parent_conversation_metadata = ParentConversationMetadata{
        .conversation_id = "p_conv",
        .conversation_title = "",
    };
    response.task_metadata = std::move(task_metadata);

    auto sharing_message = ResponseToProto(response);
    const auto& parent_meta = sharing_message.glic_experimental_triggering()
                                  .task_metadata()
                                  .parent_conversation_metadata();
    EXPECT_TRUE(parent_meta.has_conversation_id());
    EXPECT_EQ(parent_meta.conversation_id(), "p_conv");
    EXPECT_FALSE(parent_meta.has_conversation_title());
  }
  {
    ExperimentalTriggeringResponse response;
    TaskMetadata task_metadata;
    task_metadata.parent_conversation_metadata = ParentConversationMetadata{
        .conversation_id = "",
        .conversation_title = "",
    };
    response.task_metadata = std::move(task_metadata);

    auto sharing_message = ResponseToProto(response);
    const auto& parent_meta = sharing_message.glic_experimental_triggering()
                                  .task_metadata()
                                  .parent_conversation_metadata();
    EXPECT_FALSE(parent_meta.has_conversation_id());
    EXPECT_FALSE(parent_meta.has_conversation_title());
  }
}

TEST(GlicExperimentalTriggeringConvertersTest,
     ResponseToProto_TaskUpdate_PartialResponseAndMetadata) {
  ExperimentalTriggeringResponse response;
  response.context_id = "test_context";
  response.task_update = TaskUpdate{
      .state = TaskUpdate::State::kRunning,
      .data_type = TaskUpdate::DataType::kPartialResponse,
      .data = "partial text",
      .metadata = {{"key1", "val1"}, {"key2", "val2"}},
  };

  auto sharing_message = ResponseToProto(response);
  const auto& resp_proto =
      sharing_message.glic_experimental_triggering().response();
  EXPECT_EQ(resp_proto.task_update().state(),
            components_sharing_message::GlicExperimentalTriggering::
                ExperimentalTriggeringResponse::TaskUpdate::RUNNING);
  EXPECT_EQ(resp_proto.task_update().data_type(),
            components_sharing_message::GlicExperimentalTriggering::
                ExperimentalTriggeringResponse::TaskUpdate::PARTIAL_RESPONSE);
  EXPECT_EQ(resp_proto.task_update().data(), "partial text");
  EXPECT_EQ(resp_proto.task_update().metadata().at("key1"), "val1");
  EXPECT_EQ(resp_proto.task_update().metadata().at("key2"), "val2");
}

TEST(GlicExperimentalTriggeringConvertersTest,
     ResponseToProto_DeviceOptInResult) {
  {
    ExperimentalTriggeringResponse response;
    response.context_id = "test_context";
    response.device_opt_in_result = DeviceOptInResult::kUnknown;

    auto sharing_message = ResponseToProto(response);
    const auto& resp_proto =
        sharing_message.glic_experimental_triggering().response();
    EXPECT_EQ(resp_proto.device_opt_in_result(),
              components_sharing_message::GlicExperimentalTriggering::
                  ExperimentalTriggeringResponse::UNKNOWN);
  }
  {
    ExperimentalTriggeringResponse response;
    response.context_id = "test_context";
    response.device_opt_in_result = DeviceOptInResult::kAccepted;

    auto sharing_message = ResponseToProto(response);
    const auto& resp_proto =
        sharing_message.glic_experimental_triggering().response();
    EXPECT_EQ(resp_proto.device_opt_in_result(),
              components_sharing_message::GlicExperimentalTriggering::
                  ExperimentalTriggeringResponse::ACCEPTED);
  }
  {
    ExperimentalTriggeringResponse response;
    response.context_id = "test_context";
    response.device_opt_in_result = DeviceOptInResult::kDeclined;

    auto sharing_message = ResponseToProto(response);
    const auto& resp_proto =
        sharing_message.glic_experimental_triggering().response();
    EXPECT_EQ(resp_proto.device_opt_in_result(),
              components_sharing_message::GlicExperimentalTriggering::
                  ExperimentalTriggeringResponse::DECLINED);
  }
  {
    ExperimentalTriggeringResponse response;
    response.context_id = "test_context";
    response.device_opt_in_result = DeviceOptInResult::kFailed;

    auto sharing_message = ResponseToProto(response);
    const auto& resp_proto =
        sharing_message.glic_experimental_triggering().response();
    EXPECT_EQ(resp_proto.device_opt_in_result(),
              components_sharing_message::GlicExperimentalTriggering::
                  ExperimentalTriggeringResponse::FAILED);
  }
}

TEST(GlicExperimentalTriggeringConvertersTest,
     ResponseToProto_ScreenshotResult) {
  ExperimentalTriggeringResponse response;
  response.context_id = "test_context";
  response.screenshot_result = ScreenshotResult{
      .status = ScreenshotResult::Status::kSuccess,
      .file_token = "token_abc",
      .error_message = "",
  };

  auto sharing_message = ResponseToProto(response);
  const auto& resp_proto =
      sharing_message.glic_experimental_triggering().response();
  EXPECT_EQ(resp_proto.screenshot_result().status(),
            components_sharing_message::GlicExperimentalTriggering::
                ExperimentalTriggeringResponse::ScreenshotResult::SUCCESS);
  EXPECT_EQ(resp_proto.screenshot_result().file_token(), "token_abc");
  EXPECT_FALSE(resp_proto.screenshot_result().has_error_message());
}

}  // namespace glic
