// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_actuator/internals/session_stream_recorder.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/browser_actuator/internal/proto/transport_messages.pb.h"
#include "components/browser_actuator/proto/actuator_downstream_message.pb.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace browser_actuator {

SessionStreamRecorder::SessionStreamRecorder(std::string_view session_id)
    : session_id_(session_id),
      metadata_{
          .session_id = std::string(session_id),
          .start_time = base::TimeTicks::Now(),
          .total_downstream_messages = 0,
          .total_upstream_messages = 0,
          .is_active = true,
      } {}

SessionStreamRecorder::~SessionStreamRecorder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (destruction_callback_) {
    std::move(destruction_callback_).Run(metadata_, std::move(entries_));
  }
}

void SessionStreamRecorder::OnMessage(
    const google::protobuf::MessageLite& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (const auto* downstream =
          google::protobuf::DynamicCastMessage<ActuatorDownstreamMessage>(
              &message)) {
    RecordDownstreamMessage(*downstream);
    return;
  }

  if (const auto* upstream =
          google::protobuf::DynamicCastMessage<ActuatorUpstreamMessage>(
              &message)) {
    RecordUpstreamMessage(*upstream);
    return;
  }

  // Any other payload received via TransportHandler::OnMessage is an unpacked
  // downstream payload (such as ControlCommand). Record as a downstream
  // message with its typed payload.
  ActuatorDownstreamMessage downstream;
  downstream.set_session_id(session_id_);
  auto* typed_payload = downstream.add_typed_payloads();

  std::string_view type_name = message.GetTypeName();
  if (type_name.find("ControlCommand") != std::string_view::npos) {
    typed_payload->set_payload_type(
        ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND);
  } else if (type_name.find("ExperimentalTriggering") !=
             std::string_view::npos) {
    // TODO(b/560176806): Currently, ExperimentalTriggering payloads only arrive
    // via the FCM SharingMessage push path (BrowserActuatorMessageHandler).
    // Update this handling once TransportSessionImpl::ProcessDownstreamMessage
    // routes EXPERIMENTAL_TRIGGERING stream payloads to registered handlers.
    typed_payload->set_payload_type(
        ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_EXPERIMENTAL_TRIGGERING);
  } else {
    typed_payload->set_payload_type(
        ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_UNSPECIFIED);
  }
  typed_payload->mutable_proto_payload()->set_value(
      message.SerializeAsString());
  RecordDownstreamMessage(std::move(downstream));
}

void SessionStreamRecorder::RecordDownstreamMessage(
    ActuatorDownstreamMessage message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  metadata_.total_downstream_messages++;
  base::TimeTicks now = base::TimeTicks::Now();
  entries_.push_back(Entry{
      .timestamp = now,
      .message = std::move(message),
  });
  const auto& recorded_message =
      std::get<ActuatorDownstreamMessage>(entries_.back().message);
  for (auto& observer : observers_) {
    observer.OnDownstreamMessage(now, recorded_message);
  }
}

void SessionStreamRecorder::RecordUpstreamMessage(
    ActuatorUpstreamMessage message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  metadata_.total_upstream_messages++;
  base::TimeTicks now = base::TimeTicks::Now();
  entries_.push_back(Entry{
      .timestamp = now,
      .message = std::move(message),
  });
  const auto& recorded_message =
      std::get<ActuatorUpstreamMessage>(entries_.back().message);
  for (auto& observer : observers_) {
    observer.OnUpstreamMessage(now, recorded_message);
  }
}

void SessionStreamRecorder::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void SessionStreamRecorder::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

std::string_view SessionStreamRecorder::session_id() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return session_id_;
}

const SessionMetadata& SessionStreamRecorder::metadata() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return metadata_;
}

const std::vector<SessionStreamRecorder::Entry>&
SessionStreamRecorder::entries() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return entries_;
}

void SessionStreamRecorder::MarkSessionClosed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (metadata_.is_active) {
    metadata_.is_active = false;
  }
}

void SessionStreamRecorder::SetDestructionCallback(
    DestructionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  destruction_callback_ = std::move(callback);
}

base::WeakPtr<SessionStreamRecorder> SessionStreamRecorder::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace browser_actuator
