// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_SELECT_TO_SPEAK_SELECT_TO_SPEAK_MENU_VIEW_H_
#define ASH_SYSTEM_ACCESSIBILITY_SELECT_TO_SPEAK_SELECT_TO_SPEAK_MENU_VIEW_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/metadata/view_factory.h"

namespace ash {

class FloatingMenuButton;

// View for the Select-to-Speak floating menu panel.
class SelectToSpeakMenuView : public views::BoxLayoutView {
  METADATA_HEADER(SelectToSpeakMenuView, views::BoxLayoutView)

 public:
  class ASH_EXPORT Delegate {
   public:
    Delegate() {}
    virtual ~Delegate() {}
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    // Invoked when user selects an option in the reading speed list.
    virtual void OnActionSelected(SelectToSpeakPanelAction action) = 0;
  };

  // Button IDs
  enum class ButtonId {
    kPause = 1,
    kPrevParagraph = 2,
    kPrevSentence = 3,
    kNextParagraph = 4,
    kNextSentence = 5,
    kStop = 6,
    kSpeed = 7,
  };

  explicit SelectToSpeakMenuView(Delegate* delegate);
  SelectToSpeakMenuView(const SelectToSpeakMenuView&) = delete;
  SelectToSpeakMenuView& operator=(const SelectToSpeakMenuView&) = delete;
  ~SelectToSpeakMenuView() override = default;

  // Update paused status.
  void SetPaused(bool is_paused);

  void SetInitialFocus();

  void SetSpeedButtonFocused();

  // Sets the speech rate that should be selected.
  void SetInitialSpeechRate(double initial_speech_rate);

  // Sets reading speed button into toggled state.
  void SetSpeedButtonToggled(bool toggled);

 private:
  void OnButtonPressed(views::Button* sender);

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* key_event) override;

  // Owned by views hierarchy.
  raw_ptr<FloatingMenuButton> prev_paragraph_button_ = nullptr;
  raw_ptr<FloatingMenuButton> prev_sentence_button_ = nullptr;
  raw_ptr<FloatingMenuButton> pause_button_ = nullptr;
  raw_ptr<FloatingMenuButton> next_sentence_button_ = nullptr;
  raw_ptr<FloatingMenuButton> next_paragraph_button_ = nullptr;
  raw_ptr<FloatingMenuButton> stop_button_ = nullptr;
  raw_ptr<FloatingMenuButton> speed_button_ = nullptr;

  raw_ptr<Delegate> delegate_;
  bool is_paused_ = false;
};

BEGIN_VIEW_BUILDER(/* no export */, SelectToSpeakMenuView, views::BoxLayoutView)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::SelectToSpeakMenuView)

#endif  // ASH_SYSTEM_ACCESSIBILITY_SELECT_TO_SPEAK_SELECT_TO_SPEAK_MENU_VIEW_H_
