// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_METRICS_PICKER_SESSION_METRICS_H_
#define ASH_PICKER_METRICS_PICKER_SESSION_METRICS_H_

#include <optional>
#include <string>

#include "ash/ash_export.h"
#include "ash/picker/picker_category.h"
#include "ash/picker/picker_search_result.h"

namespace ui {
class TextInputClient;
}  // namespace ui

class PrefRegistrySimple;
class PrefService;

namespace ash {

// Records metrics for a session of using Picker.
class ASH_EXPORT PickerSessionMetrics {
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
    // User opens a file.
    kOpenFile = 5,
    // User opens a link.
    kOpenLink = 6,
    // User creates a google workspace file or webpage.
    kCreate = 7,
    kMaxValue = kCreate,
  };

  PickerSessionMetrics();
  explicit PickerSessionMetrics(PrefService* prefs);
  ~PickerSessionMetrics();

  // Registers prefs to the provided `registry`.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Sets session outcome. This is expected to be called exactly once during
  // a session.
  void SetOutcome(SessionOutcome outcome);

  // Sets the last category selected by the user during the session.
  // This can be multiple times per session. Only the last category is recorded.
  void SetSelectedCategory(PickerCategory category);

  // Sets the search result which user selects to finish the session.
  // This is expected to be called at most once during a session.
  void SetSelectedResult(PickerSearchResult selected_result, int index);

  // Updates the search query to latest and accumulates total edits.
  void UpdateSearchQuery(std::u16string_view search_query);

  // Records CrOS event metrics when a picker session starts.
  void OnStartSession(ui::TextInputClient* client);

  // Records if caps lock toggle is displayed in the zero state view.
  void SetCapsLockDisplayed(bool displayed);

  SessionOutcome GetOutcomeForTesting() { return outcome_; }

 private:
  // Records CrOS event metrics when a picker session finishes.
  void OnFinishSession();

  // Updates caps lock related prefs.
  void UpdateCapLockPrefs(bool caps_lock_selected);

  SessionOutcome outcome_ = SessionOutcome::kUnknown;

  std::optional<PickerCategory> last_category_;

  std::optional<PickerSearchResult> selected_result_;
  int result_index_ = -1;

  int search_query_total_edits_ = 0;
  int search_query_length_ = 0;

  bool caps_lock_displayed_ = false;

  raw_ptr<PrefService> prefs_;
};

}  // namespace ash

#endif  // ASH_PICKER_METRICS_PICKER_SESSION_METRICS_H_
