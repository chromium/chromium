// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAHI_MAHI_QUESTION_ANSWER_VIEW_H_
#define ASH_SYSTEM_MAHI_MAHI_QUESTION_ANSWER_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/mahi/mahi_ui_controller.h"
#include "base/time/time.h"
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
  // A helper class to count questions and report the metric data.
  class QuestionCountReporter {
   public:
    QuestionCountReporter();
    QuestionCountReporter(const QuestionCountReporter&) = delete;
    QuestionCountReporter& operator=(const QuestionCountReporter&) = delete;
    ~QuestionCountReporter();

    void IncreaseQuestionCount();

    // Reports `question_count_` before reset.
    void ReportDataAndReset();

   private:
    int question_count_ = 0;
  };

  // MahiUiController::Delegate:
  views::View* GetView() override;
  bool GetViewVisibility(VisibilityState state) const override;
  void OnUpdated(const MahiUiUpdate& update) override;

  void RemoveLoadingAnimatedImage();

  const raw_ptr<MahiUiController> ui_controller_;

  // Tracks the animated image that shows the loading animation when waiting for
  // an answer. The image is created when waiting and destroyed when the answer
  // is loaded.
  views::ViewTracker answer_loading_animated_image_;

  // Records the time when `answer_loading_animated_image_` starts showing and
  // playing the animation. Used for metrics collection.
  base::TimeTicks answer_start_loading_time_;

  // Counts questions and reports the metric data when:
  // 1. `MahiQuestionAnswerView` is destroyed.
  // 2. `MahiQuestionAnswerView` is refreshed.
  QuestionCountReporter question_count_reporter_;
};

BEGIN_VIEW_BUILDER(ASH_EXPORT, MahiQuestionAnswerView, views::FlexLayoutView)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(ASH_EXPORT, ash::MahiQuestionAnswerView)

#endif  // ASH_SYSTEM_MAHI_MAHI_QUESTION_ANSWER_VIEW_H_
