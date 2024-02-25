// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_DICTATION_BUBBLE_CONTROLLER_H_
#define ASH_SYSTEM_ACCESSIBILITY_DICTATION_BUBBLE_CONTROLLER_H_

#include <optional>
#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/style/color_mode_observer.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/views/view_observer.h"

namespace ui {
class InputMethod;
class TextInputClient;
}  // namespace ui

namespace views {
class View;
class Widget;
}  // namespace views

namespace ash {

enum class DictationBubbleHintType;
enum class DictationBubbleIconType;
class DictationBubbleView;

// Manages the Dictation bubble view.
class ASH_EXPORT DictationBubbleController : public ui::InputMethodObserver,
                                             public views::ViewObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called whenever the Dictation bubble UI is updated.
    virtual void OnBubbleUpdated() = 0;
  };

  DictationBubbleController();
  DictationBubbleController(const DictationBubbleController&) = delete;
  DictationBubbleController& operator=(const DictationBubbleController&) =
      delete;
  ~DictationBubbleController() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Updates the bubble's visibility and text content.
  void UpdateBubble(
      bool visible,
      DictationBubbleIconType icon,
      const std::optional<std::u16string>& text,
      const std::optional<std::vector<DictationBubbleHintType>>& hints);

  // ui::InputMethodObserver:
  void OnFocus() override {}
  void OnBlur() override {}
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override;
  void OnTextInputStateChanged(const ui::TextInputClient* client) override {}
  void OnInputMethodDestroyed(const ui::InputMethod* input_method) override {}

  // views::ViewObserver:
  void OnViewIsDeleting(views::View* observed_view) override;

 private:
  friend class DictationBubbleControllerTest;
  friend class DictationBubbleTestHelper;

  // Performs initialization if necessary.
  void MaybeInitialize();

  // Updates the view and widget.
  void Update(DictationBubbleIconType icon,
              const std::optional<std::u16string>& text,
              const std::optional<std::vector<DictationBubbleHintType>>& hints);

  base::ObserverList<Observer> observers_;

  // Owned by views hierarchy.
  raw_ptr<DictationBubbleView> dictation_bubble_view_ = nullptr;
  raw_ptr<views::Widget> widget_ = nullptr;

  base::ScopedObservation<ui::InputMethod, ui::InputMethodObserver>
      input_method_observer_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_DICTATION_BUBBLE_CONTROLLER_H_
