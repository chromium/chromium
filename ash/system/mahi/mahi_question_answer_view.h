// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAHI_MAHI_QUESTION_ANSWER_VIEW_H_
#define ASH_SYSTEM_MAHI_MAHI_QUESTION_ANSWER_VIEW_H_

#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout_view.h"

namespace ash {

// Mahi Q&A View.
class ASH_EXPORT MahiQuestionAnswerView : public views::FlexLayoutView {
  METADATA_HEADER(MahiQuestionAnswerView, views::FlexLayoutView)

 public:
  MahiQuestionAnswerView();
  MahiQuestionAnswerView(const MahiQuestionAnswerView&) = delete;
  MahiQuestionAnswerView& operator=(const MahiQuestionAnswerView&) = delete;
  ~MahiQuestionAnswerView() override;

  // Creates a question text bubble populated with the provided `question_text`
  // and forwards the question to the manager so it can be answered.
  void CreateQuestion(const std::u16string& question_text);

 private:
  // Callback provided to `MahiManager` that runs when the answer is available,
  // and creates an answer text bubble with the supplied contents.
  void OnAnswerLoaded(std::optional<std::u16string> answer_text,
                      chromeos::MahiResponseStatus status);

  base::WeakPtrFactory<MahiQuestionAnswerView> weak_ptr_factory_{this};
};

BEGIN_VIEW_BUILDER(ASH_EXPORT, MahiQuestionAnswerView, views::FlexLayoutView)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(ASH_EXPORT, ash::MahiQuestionAnswerView)

#endif  // ASH_SYSTEM_MAHI_MAHI_QUESTION_ANSWER_VIEW_H_
