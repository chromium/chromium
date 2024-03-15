// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAHI_MAHI_PANEL_VIEW_H_
#define ASH_SYSTEM_MAHI_MAHI_PANEL_VIEW_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout_view.h"

namespace ui {
class Event;
}  // namespace ui

namespace ash {

class IconButton;
class MahiQuestionAnswerView;
class SummaryOutlinesSection;

// The code for Mahi main panel view. This view is placed within
// `MahiPanelWidget`.
class ASH_EXPORT MahiPanelView : public views::FlexLayoutView {
  METADATA_HEADER(MahiPanelView, views::FlexLayoutView)

 public:
  MahiPanelView();
  MahiPanelView(const MahiPanelView&) = delete;
  MahiPanelView& operator=(const MahiPanelView&) = delete;
  ~MahiPanelView() override;

 private:
  // Callbacks for buttons and link.
  void OnCloseButtonPressed(const ui::Event& event);
  void OnLearnMoreLinkClicked();
  void OnBackButtonPressed();
  void OnSendButtonPressed();

  // Owned by the views hierarchy.
  raw_ptr<IconButton> back_button_;
  raw_ptr<MahiQuestionAnswerView> question_answer_view_;
  raw_ptr<SummaryOutlinesSection> summary_outlines_section_;

  base::WeakPtrFactory<MahiPanelView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_MAHI_MAHI_PANEL_VIEW_H_
