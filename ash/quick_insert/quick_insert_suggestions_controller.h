// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_QUICK_INSERT_SUGGESTIONS_CONTROLLER_H_
#define ASH_QUICK_INSERT_QUICK_INSERT_SUGGESTIONS_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/quick_insert/model/quick_insert_link_suggester.h"
#include "ash/quick_insert/quick_insert_clipboard_history_provider.h"
#include "ash/quick_insert/quick_insert_search_result.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"

namespace ash {

enum class QuickInsertCategory;
class QuickInsertClient;
class QuickInsertModel;

class ASH_EXPORT QuickInsertSuggestionsController {
 public:
  using SuggestionsCallback =
      base::RepeatingCallback<void(std::vector<QuickInsertSearchResult>)>;

  QuickInsertSuggestionsController();
  QuickInsertSuggestionsController(const QuickInsertSuggestionsController&) =
      delete;
  QuickInsertSuggestionsController& operator=(
      const QuickInsertSuggestionsController&) = delete;
  ~QuickInsertSuggestionsController();

  // `client` only needs to remain valid until the function ends.
  void GetSuggestions(QuickInsertClient& client,
                      const QuickInsertModel& model,
                      SuggestionsCallback callback);
  // `client` only needs to remain valid until the function ends.
  void GetSuggestionsForCategory(QuickInsertClient& client,
                                 QuickInsertCategory category,
                                 SuggestionsCallback callback);

 private:
  raw_ptr<QuickInsertClient> client_;
  QuickInsertLinkSuggester link_suggester_;
  QuickInsertClipboardHistoryProvider clipboard_provider_;
};

}  // namespace ash

#endif  // ASH_QUICK_INSERT_QUICK_INSERT_SUGGESTIONS_CONTROLLER_H_
