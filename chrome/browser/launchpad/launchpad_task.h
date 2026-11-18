// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LAUNCHPAD_LAUNCHPAD_TASK_H_
#define CHROME_BROWSER_LAUNCHPAD_LAUNCHPAD_TASK_H_

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/time/time.h"
#include "url/gurl.h"

namespace launchpad {

// Gmail reference from which a `LaunchpadTask` was generated.
struct GmailReference {
  GURL url;
  std::string subject;
};

// Chrome history entry reference from which a `LaunchpadTask` was generated.
struct ChromeHistoryEntry {
  GURL url;
  std::string title;
  base::Time visit_timestamp;
};

// Source reference from which a `LaunchpadTask` was generated.
using SourceReference = std::variant<GmailReference, ChromeHistoryEntry>;

// Structure representing a Launchpad task stored in `TasksStore`.
struct LaunchpadTask {
  enum class Status {
    kActive,
    kCompleted,
    kDismissed,
    kMaxValue = kDismissed,
  };

  // Unique identifier for the task, formatted as a UUID string assigned
  // server-side.
  std::string id;
  std::string title;
  std::string description;

  // Status of the task.
  Status status = Status::kActive;

  // Importance score from 0.0 to 1.0.
  float importance_score = 0.0f;

  // Source references from which the task was generated.
  std::vector<SourceReference> source_references;

  // Actionable URL where the user can complete the task. `std::nullopt` if
  // using source references instead.
  std::optional<GURL> actionable_url;
};

}  // namespace launchpad

#endif  // CHROME_BROWSER_LAUNCHPAD_LAUNCHPAD_TASK_H_
