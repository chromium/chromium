// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAHI_MAHI_QUESTION_ANSWER_VIEW_H_
#define ASH_SYSTEM_MAHI_MAHI_QUESTION_ANSWER_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/mahi/mahi_ui_controller.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view_tracker.h"

namespace views {
class View;
}  // namespace views

namespace ash {

class MahiUiUpdate;
enum class VisibilityState;

// Mahi Q&A View.
class ASH_EXPORT MahiQuestionAnswerView : public views::FlexLayoutView,
                                          public MahiUiController::Delegate {
  METADATA_HEADER(MahiQuestionAnswerView, views::FlexLayoutView)

 public:
  explicit MahiQuestionAnswerView(MahiUiController* ui_controller);
  MahiQuestionAnswerView(const MahiQuestionAnswerView&) = delete;
  MahiQuestionAnswerView& operator=(const MahiQuestionAnswerView&) = delete;
  ~MahiQuestionAnswerView() override;

 private:
  // MahiUiController::Delegate:
  views::View* GetView() override;
  bool GetViewVisibility(VisibilityState state) const override;
  void OnUpdated(const MahiUiUpdate& update) override;

  // Tracks the bubble that presents the error introduced by the most recent
  // question. The bubble is created when the error occurs and is destroyed when
  // the user asks a new question.
  views::ViewTracker error_bubble_;
};

BEGIN_VIEW_BUILDER(ASH_EXPORT, MahiQuestionAnswerView, views::FlexLayoutView)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(ASH_EXPORT, ash::MahiQuestionAnswerView)

#endif  // ASH_SYSTEM_MAHI_MAHI_QUESTION_ANSWER_VIEW_H_
