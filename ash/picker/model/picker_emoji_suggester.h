// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_MODEL_PICKER_EMOJI_SUGGESTER_H_
#define ASH_PICKER_MODEL_PICKER_EMOJI_SUGGESTER_H_

#include <vector>

#include "ash/ash_export.h"
#include "base/memory/raw_ref.h"

namespace ash {

class PickerEmojiHistoryModel;
class PickerSearchResult;

class ASH_EXPORT PickerEmojiSuggester {
 public:
  explicit PickerEmojiSuggester(PickerEmojiHistoryModel* history_model);

  std::vector<PickerSearchResult> GetSuggestedEmoji() const;

 private:
  raw_ref<PickerEmojiHistoryModel> history_model_;
};

}  // namespace ash

#endif  // ASH_PICKER_MODEL_PICKER_EMOJI_SUGGESTER_H_
