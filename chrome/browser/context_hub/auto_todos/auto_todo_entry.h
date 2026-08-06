// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_AUTO_TODOS_AUTO_TODO_ENTRY_H_
#define CHROME_BROWSER_CONTEXT_HUB_AUTO_TODOS_AUTO_TODO_ENTRY_H_

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/time/time.h"
#include "url/gurl.h"

namespace context_hub {

// Source reference from which an AutoTodo was generated.
struct SourceReference {
  GURL url;
  std::string subject;
};

// Data for Gmail-based AutoTodos.
struct FirstPartyData {
  // Source references (e.g., Gmail message URLs and subjects) where the
  // AutoTodo was generated from.
  std::vector<SourceReference> source_references;

  // Actionable URL where the user can complete the todo.
  GURL actionable_url;
};

// Data for Browser tab-based AutoTodos.
struct ThirdPartyData {
  enum class GroupType {
    kNoMatch,
    kNudgeToClose,
    kReadingList,
    kUnfinishedAction,
    kMaxValue = kUnfinishedAction,
  };

  // Associated tab Session ID.
  int64_t tab_id = 0;

  // Timestamp when the associated tab was last active.
  base::Time last_active_timestamp;

  // Type of todo group.
  GroupType group_type = GroupType::kNoMatch;
};

// Structure representing an AutoTodo entry stored in the store.
struct AutoTodoEntry {
  enum class Status {
    kActive,
    kCompleted,
    kDismissed,
    kMaxValue = kDismissed,
  };

  std::string id;
  std::string title;
  std::string description;

  // Status of the AutoTodo.
  Status status = Status::kActive;

  // Importance score from 0.0 to 1.0.
  float importance_score = 0.0f;

  // Type specific payload.
  std::variant<FirstPartyData, ThirdPartyData> data;

  // Helper methods to inspect variant data.
  bool is_third_party() const {
    return std::holds_alternative<ThirdPartyData>(data);
  }

  bool is_first_party() const {
    return std::holds_alternative<FirstPartyData>(data);
  }

  std::optional<int64_t> tab_id() const {
    if (const auto* third_party = std::get_if<ThirdPartyData>(&data)) {
      return third_party->tab_id;
    }
    return std::nullopt;
  }

  std::optional<ThirdPartyData::GroupType> group_type() const {
    if (const auto* third_party = std::get_if<ThirdPartyData>(&data)) {
      return third_party->group_type;
    }
    return std::nullopt;
  }
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_AUTO_TODOS_AUTO_TODO_ENTRY_H_
