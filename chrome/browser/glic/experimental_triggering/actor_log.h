// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_ACTOR_LOG_H_
#define CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_ACTOR_LOG_H_

#include <string_view>

namespace actor {
class ActorKeyedService;
}  // namespace actor

namespace components_sharing_message {
class GlicExperimentalTriggering;
}  // namespace components_sharing_message

namespace glic {

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

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_ACTOR_LOG_H_
