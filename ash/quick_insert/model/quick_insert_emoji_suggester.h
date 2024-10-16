// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_MODEL_QUICK_INSERT_EMOJI_SUGGESTER_H_
#define ASH_QUICK_INSERT_MODEL_QUICK_INSERT_EMOJI_SUGGESTER_H_

#include <string>
#include <string_view>
#include <vector>

#include "ash/ash_export.h"
#include "ash/quick_insert/quick_insert_search_result.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"

namespace ash {

class PickerEmojiHistoryModel;

class ASH_EXPORT PickerEmojiSuggester {
 public:
  using GetNameCallback =
      base::RepeatingCallback<std::string(std::string_view emoji)>;
  explicit PickerEmojiSuggester(PickerEmojiHistoryModel* history_model,
                                GetNameCallback get_name);
  ~PickerEmojiSuggester();

  std::vector<QuickInsertEmojiResult> GetSuggestedEmoji() const;

 private:
  GetNameCallback get_name_;

  raw_ref<PickerEmojiHistoryModel> history_model_;
};

}  // namespace ash

#endif  // ASH_QUICK_INSERT_MODEL_QUICK_INSERT_EMOJI_SUGGESTER_H_
