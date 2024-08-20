// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAHI_MAHI_PANEL_VIEW_H_
#define ASH_SYSTEM_MAHI_MAHI_PANEL_VIEW_H_

#include <optional>
#include <string>

#include "ash/ash_export.h"
#include "ash/system/mahi/mahi_ui_controller.h"
#include "ash/wm/system_panel_view.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/textfield/textfield_controller.h"

namespace ui {
class LocatedEvent;
}

namespace views {
class FlexLayoutView;
class Textfield;
}  // namespace views

namespace ash {

class IconButton;
class MahiContentSourceButton;
class MahiQuestionAnswerView;
class MahiUiUpdate;
class SummaryOutlinesSection;
enum class VisibilityState;

// The code for Mahi main panel view. This view is placed within
// `MahiPanelWidget`.
class ASH_EXPORT MahiPanelView : public SystemPanelView,
                                 public views::TextfieldController,
                                 public MahiUiController::Delegate {
  METADATA_HEADER(MahiPanelView, SystemPanelView)

 public:
  explicit MahiPanelView(MahiUiController* ui_controller);
  MahiPanelView(const MahiPanelView&) = delete;
  MahiPanelView& operator=(const MahiPanelView&) = delete;
  ~MahiPanelView() override;

  // Shows the pop in animation for the panel.
  void AnimatePopIn(const gfx::Rect& start_bounds);

 private:
  // views::TextfieldController:
  bool HandleKeyEvent(views::Textfield* textfield,
                      const ui::KeyEvent& key_event) override;

  // MahiUiController::Delegate:
  views::View* GetView() override;
  bool GetViewVisibility(VisibilityState state) const override;
  void OnUpdated(const MahiUiUpdate& update) override;

  // Creates the header row, which includes a back button (visible only
  // in the Q&A view), the panel title, an experiment badge and a close button.
  std::unique_ptr<views::View> CreateHeaderRow();

  // Callbacks for buttons and link.
  void OnCloseButtonPressed(const ui::Event& event);
  void OnLearnMoreLinkClicked();
  void OnBackButtonPressed();
  void OnSendButtonPressed();

  // Handles drag events to reposition the panel. Events that are not part of a
  // drag event sequence are ignored.
  void HandleDragEventIfNeeded(ui::LocatedEvent* event);

  // Callbacks when the feedback buttons are toggled from inactive to active.
  void OnThumbsUpButtonActive();
  void OnThumbsDownButtonActive();

  // `ui_controller_` will outlive `this`.
  const raw_ptr<MahiUiController> ui_controller_;

  // Owned by the views hierarchy.
  raw_ptr<views::FlexLayoutView> main_container_ = nullptr;
  raw_ptr<views::View> back_button_ = nullptr;
  raw_ptr<MahiContentSourceButton> content_source_button_ = nullptr;
  raw_ptr<MahiQuestionAnswerView> question_answer_view_ = nullptr;
  raw_ptr<SummaryOutlinesSection> summary_outlines_section_ = nullptr;
  raw_ptr<views::Textfield> question_textfield_ = nullptr;
  raw_ptr<IconButton> send_button_ = nullptr;
  raw_ptr<IconButton> thumbs_up_button_ = nullptr;
  raw_ptr<IconButton> thumbs_down_button_ = nullptr;

  // The time when this view is constructed, which is when the user opens this
  // view.
  base::TimeTicks open_time_;

  base::WeakPtrFactory<MahiPanelView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_MAHI_MAHI_PANEL_VIEW_H_
