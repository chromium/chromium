// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_DIALOG_PLATE_DIALOG_PLATE_H_
#define ASH_ASSISTANT_UI_DIALOG_PLATE_DIALOG_PLATE_H_

#include <memory>
#include <string>

#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/assistant/ui/dialog_plate/action_view.h"
#include "base/macros.h"
#include "base/observer_list.h"
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

class AssistantController;
class ActionView;

// DialogPlateButtonId ---------------------------------------------------------

enum class DialogPlateButtonId {
  kKeyboardInputToggle = 1,
  kVoiceInputToggle,
  kSettings,
};

// DialogPlateObserver ---------------------------------------------------------

class DialogPlateObserver {
 public:
  // Invoked when the dialog plate button identified by |id| is pressed.
  virtual void OnDialogPlateButtonPressed(DialogPlateButtonId id) {}

  // Invoked on dialog plate contents committed event.
  virtual void OnDialogPlateContentsCommitted(const std::string& text) {}

 protected:
  virtual ~DialogPlateObserver() = default;
};

// DialogPlate -----------------------------------------------------------------

// DialogPlate is the child of AssistantMainView concerned with providing the
// means by which a user converses with Assistant. To this end, DialogPlate
// provides a textfield for use with the keyboard input modality, and an
// ActionView which serves to either commit a text query, or toggle voice
// interaction as appropriate for the user's current input modality.
class DialogPlate : public views::View,
                    public views::TextfieldController,
                    public AssistantInteractionModelObserver,
                    public AssistantUiModelObserver,
                    public views::ButtonListener {
 public:
  explicit DialogPlate(AssistantController* assistant_controller);
  ~DialogPlate() override;

  // Adds/removes the specified |observer|.
  void AddObserver(DialogPlateObserver* observer);
  void RemoveObserver(DialogPlateObserver* observer);

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

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(AssistantVisibility new_visibility,
                             AssistantVisibility old_visibility,
                             AssistantSource source) override;

  // Returns the first focusable view or nullptr to defer to views::FocusSearch.
  views::View* FindFirstFocusableView();

 private:
  void InitLayout();
  void InitKeyboardLayoutContainer();
  void InitVoiceLayoutContainer();

  void OnButtonPressed(DialogPlateButtonId id);

  void OnAnimationStarted(const ui::CallbackLayerAnimationObserver& observer);
  bool OnAnimationEnded(const ui::CallbackLayerAnimationObserver& observer);

  void SetFocus(InputModality modality);

  AssistantController* const assistant_controller_;  // Owned by Shell.

  views::View* input_modality_layout_container_;     // Owned by view hierarchy.
  views::View* keyboard_layout_container_;           // Owned by view hierarchy.
  views::View* voice_layout_container_;              // Owned by view hierarchy.
  views::ImageButton* keyboard_input_toggle_;        // Owned by view hierarchy.
  views::ImageButton* voice_input_toggle_;           // Owned by view hierarchy.
  ActionView* animated_voice_input_toggle_;          // Owned by view hierarchy.
  views::ImageButton* settings_button_;              // Owned by view hierarchy.
  views::Textfield* textfield_;                      // Owned by view hierarchy.

  std::unique_ptr<ui::CallbackLayerAnimationObserver> animation_observer_;

  base::ObserverList<DialogPlateObserver>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(DialogPlate);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_DIALOG_PLATE_DIALOG_PLATE_H_
