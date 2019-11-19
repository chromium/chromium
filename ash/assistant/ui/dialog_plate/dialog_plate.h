// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_DIALOG_PLATE_DIALOG_PLATE_H_
#define ASH_ASSISTANT_UI_DIALOG_PLATE_DIALOG_PLATE_H_

#include <memory>
#include <string>

#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_query_history.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace ui {
class CallbackLayerAnimationObserver;
}  // namespace ui

namespace views {
class ImageButton;
}  // namespace views

namespace ash {

enum class AssistantButtonId;
class AssistantViewDelegate;
class MicView;

// DialogPlate -----------------------------------------------------------------

// DialogPlate is the child of AssistantMainView concerned with providing the
// means by which a user converses with Assistant. To this end, DialogPlate
// provides a textfield for use with the keyboard input modality, and a MicView
// which serves to toggle voice interaction as appropriate for use with the
// voice input modality.
class COMPONENT_EXPORT(ASSISTANT_UI) DialogPlate
    : public views::View,
      public views::TextfieldController,
      public AssistantInteractionModelObserver,
      public AssistantUiModelObserver,
      public views::ButtonListener {
 public:
  explicit DialogPlate(AssistantViewDelegate* delegate);
  ~DialogPlate() override;

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void RequestFocus() override;

  // ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::TextfieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

  // AssistantInteractionModelObserver:
  void OnInputModalityChanged(InputModality input_modality) override;
  void OnCommittedQueryChanged(const AssistantQuery& committed_query) override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      base::Optional<AssistantEntryPoint> entry_point,
      base::Optional<AssistantExitPoint> exit_point) override;

  // Returns the first focusable view or nullptr to defer to views::FocusSearch.
  views::View* FindFirstFocusableView();

 private:
  void InitLayout();
  void InitKeyboardLayoutContainer();
  void InitVoiceLayoutContainer();

  void OnButtonPressed(AssistantButtonId id);

  void OnAnimationStarted(const ui::CallbackLayerAnimationObserver& observer);
  bool OnAnimationEnded(const ui::CallbackLayerAnimationObserver& observer);

  void SetFocus(InputModality modality);

  AssistantViewDelegate* const delegate_;

  views::View* input_modality_layout_container_;     // Owned by view hierarchy.
  views::View* keyboard_layout_container_;           // Owned by view hierarchy.
  views::View* voice_layout_container_;              // Owned by view hierarchy.
  views::ImageButton* keyboard_input_toggle_;        // Owned by view hierarchy.
  views::ImageButton* voice_input_toggle_;           // Owned by view hierarchy.
  MicView* animated_voice_input_toggle_;             // Owned by view hierarchy.
  views::ImageButton* settings_button_;              // Owned by view hierarchy.
  views::Textfield* textfield_;                      // Owned by view hierarchy.

  std::unique_ptr<ui::CallbackLayerAnimationObserver> animation_observer_;
  std::unique_ptr<AssistantQueryHistory::Iterator> query_history_iterator_;

  DISALLOW_COPY_AND_ASSIGN(DialogPlate);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_DIALOG_PLATE_DIALOG_PLATE_H_
