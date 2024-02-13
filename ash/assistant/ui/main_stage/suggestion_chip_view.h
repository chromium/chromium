// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_SUGGESTION_CHIP_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_SUGGESTION_CHIP_VIEW_H_

#include "ash/assistant/ui/main_stage/chip_view.h"
#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "chromeos/ash/services/libassistant/public/cpp/assistant_suggestion.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

class AssistantViewDelegate;

// View representing a suggestion chip for Assistant.
class COMPONENT_EXPORT(ASSISTANT_UI) SuggestionChipView : public ChipView {
  METADATA_HEADER(SuggestionChipView, ChipView)

 public:
  using AssistantSuggestion = assistant::AssistantSuggestion;

  SuggestionChipView(AssistantViewDelegate* delegate,
                     const AssistantSuggestion& suggestion);
  SuggestionChipView(const SuggestionChipView&) = delete;
  SuggestionChipView& operator=(const SuggestionChipView&) = delete;
  ~SuggestionChipView() override;

  const base::UnguessableToken& suggestion_id() const { return suggestion_id_; }

 private:
  const raw_ptr<AssistantViewDelegate> delegate_;
  const base::UnguessableToken suggestion_id_;

  base::WeakPtrFactory<SuggestionChipView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_SUGGESTION_CHIP_VIEW_H_
