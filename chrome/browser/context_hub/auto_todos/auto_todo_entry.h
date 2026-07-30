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

// Data for Gmail-based AutoTodos.
struct FirstPartyData {
  // Source URLs (e.g., Gmail message URLs) where the AutoTodo was generated
  // from.
  std::vector<GURL> source_references;

  // Actionable URL where the user can complete the todo.
  GURL actionable_url;
};

// Data for Browser tab-based AutoTodos.
struct ThirdPartyData {
  // Associated tab Session ID.
  int64_t tab_id = 0;

  // Timestamp when the associated tab was last active.
  base::Time last_active_timestamp;
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
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_AUTO_TODOS_AUTO_TODO_ENTRY_H_
