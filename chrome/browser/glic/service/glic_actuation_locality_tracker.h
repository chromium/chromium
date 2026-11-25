// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_GLIC_ACTUATION_LOCALITY_TRACKER_H_
#define CHROME_BROWSER_GLIC_SERVICE_GLIC_ACTUATION_LOCALITY_TRACKER_H_

#include <optional>
#include <string>
#include <string_view>

class Profile;

namespace actor {
struct TaskSourceInfo;
}  // namespace actor

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace glic {

// Extracts the opaque Gemini conversation ID from `source_info` if the task
// originated from a Gemini conversation with a non-empty ID, or `std::nullopt`
// otherwise.
std::optional<std::string> GetGeminiConversationId(
    const actor::TaskSourceInfo& source_info);

// Resolves the live target tab of the most recently started non-completed
// Gemini `ActorTask` whose conversation ID matches `conversation_id` exactly in
// `profile`, or nullptr if `profile` is null, `conversation_id` is empty, or no
// such task has a live controlled tab.
tabs::TabInterface* FindActiveGeminiActuationTab(
    Profile* profile,
    std::string_view conversation_id);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_ACTUATION_LOCALITY_TRACKER_H_
