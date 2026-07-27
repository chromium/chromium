// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_TYPES_H_
#define CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_TYPES_H_

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/functional/callback.h"

namespace glic {

// Outcome of a device opt-in request for experimental triggering.
enum class DeviceOptInResult {
  kUnknown,
  kAccepted,
  kDeclined,
  kFailed,
};

// Parent conversation metadata attached to triggering requests.
struct ParentConversationMetadata {
  std::string conversation_id;
  std::string conversation_title;
};

// Metadata for tracking conversation and message sequence ordering.
struct TaskMetadata {
  std::string conversation_id;
  std::string task_id;
  std::optional<int64_t> sender_sequence_number;
  std::optional<int64_t> last_seen_sequence_number;
  std::optional<ParentConversationMetadata> parent_conversation_metadata;
};

// Streaming update payload representing current task status.
struct TaskUpdate {
  // Task execution state.
  enum class State {
    kUnknown,
    kStarting,
    kRunning,
    kComplete,
    kStopped,
    kFailed,
    kPaused,
    kYield,
    kResumed,
  };

  // Type of data accompanying the status update.
  enum class DataType {
    kUnknown,
    kWorklog,
    kPartialResponse,
    kErrorMessage,
    kFinalResponse,
  };

  State state = State::kUnknown;
  std::optional<DataType> data_type;
  std::string data;
  std::map<std::string, std::string> metadata;
};

// Payload for triggering a new actuation session.
struct TriggerActuationRequest {
  std::string initial_prompt;
};

// Payload for continuing an active actuation session.
struct ContinueActuationRequest {
  std::string continuation_prompt;
};

// Payload for stopping an active actuation session.
struct StopActuationRequest {
  std::string stop_reason;
};

// Payload for updating device opt-in status.
struct DeviceOptInRequest {
  std::string triggering_source;
};

// Payload notification for parent metadata updates.
struct TaskMetadataUpdated {};

// Payload indicating request proto was set but inner payload variant was not
// set.
struct RequestPayloadNotSet {};

// Payload for requesting an encrypted page screenshot.
struct GetScreenshotRequest {
  std::vector<uint8_t> public_key;
  std::vector<uint8_t> auth_secret;
  std::vector<uint8_t> request_token;
};

// Incoming request payload container for experimental triggering.
struct ExperimentalTriggeringRequest {
  std::optional<int32_t> version;
  std::string context_id;
  std::optional<TaskMetadata> task_metadata;

  using Payload = std::variant<std::monostate,
                               RequestPayloadNotSet,
                               TriggerActuationRequest,
                               ContinueActuationRequest,
                               StopActuationRequest,
                               DeviceOptInRequest,
                               TaskMetadataUpdated,
                               GetScreenshotRequest>;
  Payload payload;
};

// Result payload for encrypted screenshot capture operations.
struct ScreenshotResult {
  // Status of the screenshot capture operation.
  enum class Status {
    kUnspecified,
    kSuccess,
    kErrorCapture,
    kErrorServer,
  };
  Status status = Status::kUnspecified;
  std::string file_token;
  std::string error_message;
  std::vector<uint8_t> request_token;
};

// Outgoing response container for experimental triggering.
struct ExperimentalTriggeringResponse {
  std::string context_id;
  std::optional<TaskMetadata> task_metadata;

  // At most one of the following payload fields should be populated;
  // they map to a protobuf `oneof response` payload on the wire.
  std::optional<TaskUpdate> task_update;
  std::optional<DeviceOptInResult> device_opt_in_result;
  std::optional<ScreenshotResult> screenshot_result;
};

using GlicExperimentalTriggeringResponseCallback =
    base::OnceCallback<void(ExperimentalTriggeringResponse)>;
using GlicExperimentalTriggeringUpdateCallback =
    base::RepeatingCallback<void(ExperimentalTriggeringResponse)>;

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_TYPES_H_
