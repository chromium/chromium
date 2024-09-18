// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_PICKER_SUGGESTIONS_CONTROLLER_H_
#define ASH_PICKER_PICKER_SUGGESTIONS_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/picker/picker_clipboard_history_provider.h"
#include "ash/picker/picker_search_result.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"

namespace ash {

enum class PickerCategory;
class PickerClient;
class PickerModel;

class ASH_EXPORT PickerSuggestionsController {
 public:
  using SuggestionsCallback =
      base::RepeatingCallback<void(std::vector<PickerSearchResult>)>;

  PickerSuggestionsController();
  PickerSuggestionsController(const PickerSuggestionsController&) = delete;
  PickerSuggestionsController& operator=(const PickerSuggestionsController&) =
      delete;
  ~PickerSuggestionsController();

  // `client` only needs to remain valid until the function ends.
  void GetSuggestions(PickerClient& client,
                      const PickerModel& model,
                      SuggestionsCallback callback);
  // `client` only needs to remain valid until the function ends.
  void GetSuggestionsForCategory(PickerClient& client,
                                 PickerCategory category,
                                 SuggestionsCallback callback);

 private:
  raw_ptr<PickerClient> client_;
  PickerClipboardHistoryProvider clipboard_provider_;
};

}  // namespace ash

#endif  // ASH_PICKER_PICKER_SUGGESTIONS_CONTROLLER_H_
