// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_converters.h"

#include <optional>
#include <utility>
#include <vector>

namespace glic {

namespace {

using ProtoResponse = components_sharing_message::GlicExperimentalTriggering::
    ExperimentalTriggeringResponse;
using ProtoTaskUpdate = ProtoResponse::TaskUpdate;

ProtoTaskUpdate::State ToProtoState(TaskUpdate::State state) {
  switch (state) {
    case TaskUpdate::State::kUnknown:
      return ProtoTaskUpdate::UNKNOWN_STATE;
    case TaskUpdate::State::kStarting:
      return ProtoTaskUpdate::STARTING;
    case TaskUpdate::State::kRunning:
      return ProtoTaskUpdate::RUNNING;
    case TaskUpdate::State::kComplete:
      return ProtoTaskUpdate::COMPLETE;
    case TaskUpdate::State::kStopped:
      return ProtoTaskUpdate::STOPPED;
    case TaskUpdate::State::kFailed:
      return ProtoTaskUpdate::FAILED;
    case TaskUpdate::State::kPaused:
      return ProtoTaskUpdate::PAUSED;
    case TaskUpdate::State::kYield:
      return ProtoTaskUpdate::YIELD;
    case TaskUpdate::State::kResumed:
      return ProtoTaskUpdate::RESUMED;
  }
}

ProtoTaskUpdate::DataType ToProtoDataType(TaskUpdate::DataType type) {
  switch (type) {
    case TaskUpdate::DataType::kUnknown:
      return ProtoTaskUpdate::UNKNOWN_DATA_TYPE;
    case TaskUpdate::DataType::kWorklog:
      return ProtoTaskUpdate::WORKLOG;
    case TaskUpdate::DataType::kPartialResponse:
      return ProtoTaskUpdate::PARTIAL_RESPONSE;
    case TaskUpdate::DataType::kErrorMessage:
      return ProtoTaskUpdate::ERROR_MESSAGE;
    case TaskUpdate::DataType::kFinalResponse:
      return ProtoTaskUpdate::FINAL_RESPONSE;
  }
}

}  // namespace

ExperimentalTriggeringRequest ProtoToRequest(
    const components_sharing_message::GlicExperimentalTriggering& proto) {
  ExperimentalTriggeringRequest request;
  if (proto.has_glic_experimental_triggering_version()) {
    request.version = proto.glic_experimental_triggering_version();
  }
  if (proto.has_context_id()) {
    request.context_id = proto.context_id();
  }

  if (proto.has_task_metadata()) {
    TaskMetadata task_metadata;
    const auto& proto_meta = proto.task_metadata();
    if (proto_meta.has_conversation_id()) {
      task_metadata.conversation_id = proto_meta.conversation_id();
    }
    if (proto_meta.has_task_id()) {
      task_metadata.task_id = proto_meta.task_id();
    }
    if (proto_meta.has_sender_sequence_number()) {
      task_metadata.sender_sequence_number =
          proto_meta.sender_sequence_number();
    }
    if (proto_meta.has_last_seen_sequence_number()) {
      task_metadata.last_seen_sequence_number =
          proto_meta.last_seen_sequence_number();
    }
    if (proto_meta.has_parent_conversation_metadata()) {
      ParentConversationMetadata parent_meta;
      if (proto_meta.parent_conversation_metadata().has_conversation_id()) {
        parent_meta.conversation_id =
            proto_meta.parent_conversation_metadata().conversation_id();
      }
      if (proto_meta.parent_conversation_metadata().has_conversation_title()) {
        parent_meta.conversation_title =
            proto_meta.parent_conversation_metadata().conversation_title();
      }
      task_metadata.parent_conversation_metadata = std::move(parent_meta);
    }
    request.task_metadata = std::move(task_metadata);
  }

  if (proto.has_task_metadata_updated()) {
    request.payload = TaskMetadataUpdated();
  } else if (proto.has_request()) {
    const auto& req_proto = proto.request();
    switch (req_proto.payload_case()) {
      case components_sharing_message::GlicExperimentalTriggering::
          ExperimentalTriggeringRequest::kTriggerActuationRequest: {
        TriggerActuationRequest trigger_req;
        if (req_proto.trigger_actuation_request().has_initial_prompt()) {
          trigger_req.initial_prompt =
              req_proto.trigger_actuation_request().initial_prompt();
        }
        request.payload = std::move(trigger_req);
        break;
      }
      case components_sharing_message::GlicExperimentalTriggering::
          ExperimentalTriggeringRequest::kContinueActuationRequest: {
        ContinueActuationRequest continue_req;
        if (req_proto.continue_actuation_request().has_continuation_prompt()) {
          continue_req.continuation_prompt =
              req_proto.continue_actuation_request().continuation_prompt();
        }
        request.payload = std::move(continue_req);
        break;
      }
      case components_sharing_message::GlicExperimentalTriggering::
          ExperimentalTriggeringRequest::kStopActuationRequest: {
        StopActuationRequest stop_req;
        if (req_proto.stop_actuation_request().has_stop_reason()) {
          stop_req.stop_reason =
              req_proto.stop_actuation_request().stop_reason();
        }
        request.payload = std::move(stop_req);
        break;
      }
      case components_sharing_message::GlicExperimentalTriggering::
          ExperimentalTriggeringRequest::kDeviceOptInRequest: {
        DeviceOptInRequest opt_in_req;
        if (req_proto.device_opt_in_request().has_triggering_source()) {
          opt_in_req.triggering_source =
              req_proto.device_opt_in_request().triggering_source();
        }
        request.payload = std::move(opt_in_req);
        break;
      }
      case components_sharing_message::GlicExperimentalTriggering::
          ExperimentalTriggeringRequest::kGetScreenshotRequest: {
        GetScreenshotRequest screenshot_req;
        const auto& screenshot_proto = req_proto.get_screenshot_request();
        if (screenshot_proto.has_public_key()) {
          screenshot_req.public_key = {screenshot_proto.public_key().begin(),
                                       screenshot_proto.public_key().end()};
        }
        if (screenshot_proto.has_auth_secret()) {
          screenshot_req.auth_secret = {screenshot_proto.auth_secret().begin(),
                                        screenshot_proto.auth_secret().end()};
        }
        if (screenshot_proto.has_request_token()) {
          screenshot_req.request_token = {
              screenshot_proto.request_token().begin(),
              screenshot_proto.request_token().end()};
        }
        request.payload = std::move(screenshot_req);
        break;
      }
      case components_sharing_message::GlicExperimentalTriggering::
          ExperimentalTriggeringRequest::PAYLOAD_NOT_SET:
        request.payload = RequestPayloadNotSet();
        break;
    }
  }

  return request;
}

components_sharing_message::SharingMessage ResponseToProto(
    const ExperimentalTriggeringResponse& response) {
  components_sharing_message::SharingMessage message;
  auto* triggering = message.mutable_glic_experimental_triggering();
  if (!response.context_id.empty()) {
    triggering->set_context_id(response.context_id);
  }

  if (response.task_metadata.has_value()) {
    auto* metadata = triggering->mutable_task_metadata();
    if (response.task_metadata->sender_sequence_number.has_value()) {
      metadata->set_sender_sequence_number(
          *response.task_metadata->sender_sequence_number);
    }
    if (response.task_metadata->last_seen_sequence_number.has_value()) {
      metadata->set_last_seen_sequence_number(
          *response.task_metadata->last_seen_sequence_number);
    }
    if (!response.task_metadata->conversation_id.empty()) {
      metadata->set_conversation_id(response.task_metadata->conversation_id);
    }
    if (!response.task_metadata->task_id.empty()) {
      metadata->set_task_id(response.task_metadata->task_id);
    }

    if (response.task_metadata->parent_conversation_metadata.has_value()) {
      auto* parent_meta = metadata->mutable_parent_conversation_metadata();
      if (!response.task_metadata->parent_conversation_metadata->conversation_id
               .empty()) {
        parent_meta->set_conversation_id(
            response.task_metadata->parent_conversation_metadata
                ->conversation_id);
      }
      if (!response.task_metadata->parent_conversation_metadata
               ->conversation_title.empty()) {
        parent_meta->set_conversation_title(
            response.task_metadata->parent_conversation_metadata
                ->conversation_title);
      }
    }
  }

  if (response.task_update.has_value()) {
    auto* proto_update = triggering->mutable_response()->mutable_task_update();
    proto_update->set_state(ToProtoState(response.task_update->state));
    if (response.task_update->data_type.has_value()) {
      proto_update->set_data_type(
          ToProtoDataType(*response.task_update->data_type));
    }
    proto_update->set_data(response.task_update->data);
    for (const auto& [key, value] : response.task_update->metadata) {
      (*proto_update->mutable_metadata())[key] = value;
    }
  }

  if (response.device_opt_in_result.has_value()) {
    switch (*response.device_opt_in_result) {
      case DeviceOptInResult::kUnknown:
        triggering->mutable_response()->set_device_opt_in_result(
            ProtoResponse::UNKNOWN);
        break;
      case DeviceOptInResult::kAccepted:
        triggering->mutable_response()->set_device_opt_in_result(
            ProtoResponse::ACCEPTED);
        break;
      case DeviceOptInResult::kDeclined:
        triggering->mutable_response()->set_device_opt_in_result(
            ProtoResponse::DECLINED);
        break;
      case DeviceOptInResult::kFailed:
        triggering->mutable_response()->set_device_opt_in_result(
            ProtoResponse::FAILED);
        break;
    }
  }

  if (response.screenshot_result.has_value()) {
    auto* proto_screenshot =
        triggering->mutable_response()->mutable_screenshot_result();
    switch (response.screenshot_result->status) {
      case ScreenshotResult::Status::kUnspecified:
        proto_screenshot->set_status(
            ProtoResponse::ScreenshotResult::UNSPECIFIED);
        break;
      case ScreenshotResult::Status::kSuccess:
        proto_screenshot->set_status(ProtoResponse::ScreenshotResult::SUCCESS);
        break;
      case ScreenshotResult::Status::kErrorCapture:
        proto_screenshot->set_status(
            ProtoResponse::ScreenshotResult::ERROR_CAPTURE);
        break;
      case ScreenshotResult::Status::kErrorServer:
        proto_screenshot->set_status(
            ProtoResponse::ScreenshotResult::ERROR_SERVER);
        break;
    }
    if (!response.screenshot_result->file_token.empty()) {
      proto_screenshot->set_file_token(response.screenshot_result->file_token);
    }
    if (!response.screenshot_result->error_message.empty()) {
      proto_screenshot->set_error_message(
          response.screenshot_result->error_message);
    }
    if (!response.screenshot_result->request_token.empty()) {
      proto_screenshot->set_request_token(
          response.screenshot_result->request_token.data(),
          response.screenshot_result->request_token.size());
    }
  }

  return message;
}

}  // namespace glic
