// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAHI_MAHI_PANEL_VIEW_H_
#define ASH_SYSTEM_MAHI_MAHI_PANEL_VIEW_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/layout/flex_layout_view.h"

namespace ash {

class MahiUiController;
class MahiQuestionAnswerView;
class SummaryOutlinesSection;
class SystemTextfield;

// The code for Mahi main panel view. This view is placed within
// `MahiPanelWidget`.
class ASH_EXPORT MahiPanelView : public views::FlexLayoutView,
                                 public views::TextfieldController {
  METADATA_HEADER(MahiPanelView, views::FlexLayoutView)

 public:
  explicit MahiPanelView(MahiUiController* ui_controller);
  MahiPanelView(const MahiPanelView&) = delete;
  MahiPanelView& operator=(const MahiPanelView&) = delete;
  ~MahiPanelView() override;

 protected:
  // views::TextfieldController:
  bool HandleKeyEvent(views::Textfield* textfield,
                      const ui::KeyEvent& key_event) override;

 private:
  // Creates the header row, which includes a back button (visible only
  // in the Q&A view), the panel title, an experiment badge and a close button.
  std::unique_ptr<views::View> CreateHeaderRow();

  // Callbacks for buttons and link.
  void OnCloseButtonPressed(const ui::Event& event);
  void OnLearnMoreLinkClicked();
  void OnBackButtonPressed();
  void OnSendButtonPressed();

  const raw_ptr<MahiUiController> ui_controller_;

  // Owned by the views hierarchy.
  raw_ptr<views::View> back_button_;
  raw_ptr<MahiQuestionAnswerView> question_answer_view_;
  raw_ptr<SummaryOutlinesSection> summary_outlines_section_;
  raw_ptr<SystemTextfield> question_textfield_;

  base::WeakPtrFactory<MahiPanelView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_MAHI_MAHI_PANEL_VIEW_H_
