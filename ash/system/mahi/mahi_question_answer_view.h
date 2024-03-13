// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAHI_MAHI_QUESTION_ANSWER_VIEW_H_
#define ASH_SYSTEM_MAHI_MAHI_QUESTION_ANSWER_VIEW_H_

#include "ash/ash_export.h"
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

  // Creates sample conversation bubbles for testing.
  void CreateSampleQuestionAnswer();
};

BEGIN_VIEW_BUILDER(ASH_EXPORT, MahiQuestionAnswerView, views::FlexLayoutView)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(ASH_EXPORT, ash::MahiQuestionAnswerView)

#endif  // ASH_SYSTEM_MAHI_MAHI_QUESTION_ANSWER_VIEW_H_
