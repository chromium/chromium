// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_ACTUATOR_INTERNALS_SESSION_STREAM_RECORDER_H_
#define CHROME_BROWSER_BROWSER_ACTUATOR_INTERNALS_SESSION_STREAM_RECORDER_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "components/browser_actuator/internal/proto/transport_messages.pb.h"
#include "components/browser_actuator/proto/actuator_downstream_message.pb.h"
#include "components/browser_actuator/public/transport_handler.h"

namespace google::protobuf {
class MessageLite;
}  // namespace google::protobuf

namespace browser_actuator {

// Metadata for an active or closed transport session.
struct SessionMetadata {
  std::string session_id;
  base::TimeTicks start_time;
  size_t total_downstream_messages = 0;
  size_t total_upstream_messages = 0;
  bool is_active = true;
};

// Records canonical protobuf messages (ActuatorDownstreamMessage and
// ActuatorUpstreamMessage) for a single TransportSession using an unbounded
// std::vector. Observers can subscribe to receive live messages as they occur.
class SessionStreamRecorder : public TransportHandler {
 public:
  // Stored entry pairing timestamp with canonical protobuf message.
  struct Entry {
    base::TimeTicks timestamp;
    std::variant<ActuatorDownstreamMessage, ActuatorUpstreamMessage> message;
  };

  using DestructionCallback =
      base::OnceCallback<void(const SessionMetadata&, std::vector<Entry>)>;

  // Direct proto observer interface for real-time streaming.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnDownstreamMessage(base::TimeTicks timestamp,
                                     const ActuatorDownstreamMessage& message) {
    }
    virtual void OnUpstreamMessage(base::TimeTicks timestamp,
                                   const ActuatorUpstreamMessage& message) {}
  };

  explicit SessionStreamRecorder(std::string_view session_id);
  ~SessionStreamRecorder() override;

  SessionStreamRecorder(const SessionStreamRecorder&) = delete;
  SessionStreamRecorder& operator=(const SessionStreamRecorder&) = delete;

  // TransportHandler implementation:
  void OnMessage(const google::protobuf::MessageLite& message) override;

  // Direct recording methods:
  void RecordDownstreamMessage(ActuatorDownstreamMessage message);
  void RecordUpstreamMessage(ActuatorUpstreamMessage message);

  // Observer management:
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  std::string_view session_id() const;
  const SessionMetadata& metadata() const;
  const std::vector<Entry>& entries() const;

  // Marks the session as closed.
  void MarkSessionClosed();

  // Sets a callback invoked on destruction to retain history for dumps.
  void SetDestructionCallback(DestructionCallback callback);

  base::WeakPtr<SessionStreamRecorder> GetWeakPtr();

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  const std::string session_id_;
  SessionMetadata metadata_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::vector<Entry> entries_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::ObserverList<Observer> observers_ GUARDED_BY_CONTEXT(sequence_checker_);

  DestructionCallback destruction_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<SessionStreamRecorder> weak_ptr_factory_{this};
};

}  // namespace browser_actuator

#endif  // CHROME_BROWSER_BROWSER_ACTUATOR_INTERNALS_SESSION_STREAM_RECORDER_H_
