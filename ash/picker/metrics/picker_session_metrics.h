// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_METRICS_PICKER_SESSION_METRICS_H_
#define ASH_PICKER_METRICS_PICKER_SESSION_METRICS_H_

#include <optional>
#include <string>

#include "ash/public/cpp/picker/picker_category.h"
#include "ash/public/cpp/picker/picker_search_result.h"

namespace ui {
class TextInputClient;
}  // namespace ui

namespace ash {

// Records metrics for a session of using Picker.
class PickerSessionMetrics {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class SessionOutcome {
    // The outcome is unknown.
    kUnknown = 0,
    // User inserts or copies a result.
    kInsertedOrCopied = 1,
    // User abandons the session (e.g. by closing the window without inserting).
    kAbandoned = 2,
    // User selects an action to open another window, e.g. the Emoji picker.
    kRedirected = 3,
    // User selects an action related to text format.
    kFormat = 4,
    kMaxValue = kFormat,
  };

  PickerSessionMetrics();
  ~PickerSessionMetrics();

  // Sets session outcome. This is expected to be called exactly once during
  // a session.
  void SetOutcome(SessionOutcome outcome);

  // Sets user action. This is expected to be called at most once during a
  // session.
  // TODO(b/336402739): replace the argument type with some action enum after
  // refactor.
  void SetAction(PickerCategory action);

  // Sets the search result which user inserts. This is expected to be called at
  // most once during a session.
  void SetInsertedResult(PickerSearchResult inserted_result, int index);

  // Updates the search query to latest and accumulates total edits.
  void UpdateSearchQuery(std::u16string_view search_query);

  // Records CrOS event metrics when a picker session starts.
  void OnStartSession(ui::TextInputClient* client);

 private:
  // Records CrOS event metrics when a picker session finishes.
  void OnFinishSession();

  SessionOutcome outcome_ = SessionOutcome::kUnknown;

  // TODO(b/336402739): replace the type with some action enum after refactor.
  std::optional<PickerCategory> action_;

  std::optional<PickerSearchResult> inserted_result_;
  int result_index_ = -1;

  int search_query_total_edits_ = 0;
  int search_query_length_ = 0;
};

}  // namespace ash

#endif  // ASH_PICKER_METRICS_PICKER_SESSION_METRICS_H_
