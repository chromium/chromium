// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_SERVICE_GLIC_ACTIVITY_TYPES_H_
#define CHROME_BROWSER_GLIC_PUBLIC_SERVICE_GLIC_ACTIVITY_TYPES_H_

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/actor/core/task_id.h"
#include "ui/gfx/vector_icon_types.h"

namespace glic {

// The type of asynchronous activity occurring in Glic.
enum class GlicActivityType {
  kActorTask,
  kConversationResponse,
};

// State of an individual Glic activity.
enum class GlicActivityState {
  kInProgress,      // Acting, Generating
  kNeedsAttention,  // User action/confirmation needed, auth needed, paused
  kPaused,          // Paused by user
  kCompleted,       // Finished, response ready
  kFailed,          // Error
};

// Expresses the degree of user interruption requested for an activity update.
enum class ActivityInterruptionLevel {
  // Silent: Background update only. Updates drawer list and badge count.
  // No bubble, no pill text, no sound.
  kSilent,

  // Passive Nudge: Updates the top-chrome split button text pill
  // (e.g. "Response ready", "Task done") without popping open a window.
  kPassiveNudge,

  // Delayed Task Start: Alerts the user that automated work has begun.
  // Pops open the bubble with a delay if Chrome is focused, or delegates
  // to an OS notification if Chrome is in the background.
  kDelayedTaskStart,
};

// Parameters for updating a conversation activity's state and metadata.
struct GlicConversationActivityUpdate {
  GlicActivityState state;
  std::string_view title = "";
  std::optional<std::string_view> preview_snippet = std::nullopt;
  bool is_currently_visible = false;
};

// Key uniquely identifying an activity.
struct GlicActivityKey {
  GlicActivityKey(GlicActivityType type, std::string id);

  GlicActivityType type;
  std::string id;

  bool operator==(const GlicActivityKey& other) const = default;

  template <typename H>
  friend H AbslHashValue(H h, const GlicActivityKey& key) {
    return H::combine(std::move(h), key.type, key.id);
  }
};

// Complete metadata for an ongoing or recently completed activity.
struct GlicActivityItem {
  GlicActivityItem(GlicActivityKey key, GlicActivityState state);
  GlicActivityItem(const GlicActivityItem&);
  GlicActivityItem& operator=(const GlicActivityItem&);
  GlicActivityItem(GlicActivityItem&&);
  GlicActivityItem& operator=(GlicActivityItem&&);
  ~GlicActivityItem();

  GlicActivityType type() const { return key.type; }

  GlicActivityKey key;
  std::optional<actor::TaskId> task_id;
  GlicActivityState state;
  std::u16string title;
  std::u16string subtitle;
  raw_ptr<const gfx::VectorIcon> icon = nullptr;
  // True if the user has viewed this activity in the Glic panel (e.g. by
  // having the panel open during completion, or by navigating to it from the
  // Activity Hub drawer). When false, the activity is considered unseen and
  // contributes to the split button's notification badge count.
  bool has_been_seen = false;
  bool has_tab = true;
  bool is_enabled = true;
  base::Time timestamp;
};

// Represents the aggregate state of all activities for rendering tabstrip /
// toolbar nudges and split button badges.
struct GlicActivityNudgeState {
  enum class Text {
    kDefault,         // Icon only
    kNeedsAttention,  // "Check task" / "Action needed"
    kCompleteTasks,   // "N tasks complete"
    kResponseReady,   // "Response ready"
  };

  bool operator==(const GlicActivityNudgeState& other) const = default;

  Text text = Text::kDefault;
  size_t count = 0;
  bool has_attention_badge = false;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_SERVICE_GLIC_ACTIVITY_TYPES_H_
