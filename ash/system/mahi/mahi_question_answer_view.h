// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAHI_MAHI_QUESTION_ANSWER_VIEW_H_
#define ASH_SYSTEM_MAHI_MAHI_QUESTION_ANSWER_VIEW_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/system/mahi/mahi_ui_controller.h"
#include "base/scoped_observation.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout_view.h"

namespace ash {

// Mahi Q&A View.
class ASH_EXPORT MahiQuestionAnswerView : public views::FlexLayoutView,
                                          public MahiUiController::Observer {
  METADATA_HEADER(MahiQuestionAnswerView, views::FlexLayoutView)

 public:
  explicit MahiQuestionAnswerView(MahiUiController* ui_controller);
  MahiQuestionAnswerView(const MahiQuestionAnswerView&) = delete;
  MahiQuestionAnswerView& operator=(const MahiQuestionAnswerView&) = delete;
  ~MahiQuestionAnswerView() override;

 private:
  // MahiUiController::Observer:
  void OnAnswerLoaded(const std::u16string& answer) override;
  void OnContentsRefreshInitiated() override;
  void OnError(chromeos::MahiResponseStatus status) override;
  void OnNavigatedToSummaryOutlinesSection() override;
  void OnQuestionPosted(const std::u16string& question) override;

  base::ScopedObservation<MahiUiController, MahiUiController::Observer>
      controller_observation_{this};
};

BEGIN_VIEW_BUILDER(ASH_EXPORT, MahiQuestionAnswerView, views::FlexLayoutView)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(ASH_EXPORT, ash::MahiQuestionAnswerView)

#endif  // ASH_SYSTEM_MAHI_MAHI_QUESTION_ANSWER_VIEW_H_
