// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_ACTOR_LOG_H_
#define CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_ACTOR_LOG_H_

#include <cstddef>
#include <string_view>

#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_metrics.h"

namespace actor {
class ActorKeyedService;
class AggregatedJournal;
}  // namespace actor

namespace components_sharing_message {
class GlicExperimentalTriggering;
}  // namespace components_sharing_message

namespace glic {

// Snapshot of an ExperimentalTriggeringUpdatesHandler's live-work state
// captured at the start of its destructor (before teardown stops owned work).
struct UpdatesHandlerDestroyedJournalSnapshot {
  bool has_instance = false;
  bool has_actions_runner = false;
  bool awaiting_opt_in = false;
  size_t pending_invoke_count = 0;
  bool is_actuation_started = false;
  bool terminal_update_sent = false;
};

// Returns stable diagnostic string names for actor journal detail values.
std::string_view ExperimentalTriggeringHandlerDestroyReasonToString(
    GlicExperimentalTriggeringHandlerDestroyReason reason);

// Logs a GlicExperimentalTriggering protobuf message to the actor journal.
// `actor_service` is used to access the journal. `event_name` identifies the
// logged event. `context_id` associates the message with a session track.
// Extracts key metadata from `proto` request/response payloads (such as task
// IDs, conversation IDs, and task update states) and serializes `proto` for
// visualization in chrome://actor-internals.
void LogGlicExperimentalTriggeringProto(
    actor::ActorKeyedService* actor_service,
    std::string_view event_name,
    std::string_view context_id,
    const components_sharing_message::GlicExperimentalTriggering& proto);

// Logs an instant `UpdatesHandlerDestroyed` entry on `context_id`'s
// GlicExperimentalTriggering track when `journal` is non-null.
void LogGlicExperimentalTriggeringHandlerDestroyed(
    actor::AggregatedJournal* journal,
    std::string_view context_id,
    GlicExperimentalTriggeringHandlerDestroyReason reason,
    const UpdatesHandlerDestroyedJournalSnapshot& snapshot);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_ACTOR_LOG_H_
