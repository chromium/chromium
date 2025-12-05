// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_HANDOFF_BUTTON_CONTROLLER_H_
#define CHROME_BROWSER_ACTOR_UI_HANDOFF_BUTTON_CONTROLLER_H_

#include <string_view>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/ui/actor_ui_window_controller.h"
#include "chrome/browser/actor/ui/states/handoff_button_state.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {
class LabelButton;
class Widget;
class WidgetDelegate;
}  // namespace views

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace ui {
class ImageModel;
}  // namespace ui

namespace actor::ui {
class ActorUiTabControllerInterface;
}

namespace actor::ui {

inline const char16_t* const TAKE_OVER_TASK_TEXT = u"Take over task";
inline const char16_t* const GIVE_TASK_BACK_TEXT = u"Give task back";

class HandoffButtonWidget : public views::Widget {
 public:
  HandoffButtonWidget();
  ~HandoffButtonWidget() override;

  using HoverCallback = base::RepeatingCallback<void(bool)>;

  void SetHoveredCallback(HoverCallback callback);
  void OnMouseEvent(::ui::MouseEvent* event) override;

 private:
  HoverCallback hover_callback_;
};

class HandoffButtonController : public views::ViewObserver {
 public:
  explicit HandoffButtonController(views::View* anchor_view,
                                   ActorUiWindowController* window_controller);
  ~HandoffButtonController() override;

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kHandoffButtonElementId);

  HandoffButtonController(const HandoffButtonController&) = delete;
  HandoffButtonController& operator=(const HandoffButtonController&) = delete;

  virtual void UpdateState(HandoffButtonState state,
                           bool is_visible,
                           base::OnceClosure callback);
  // Returns true if the mouse is currently hovering over the handoff button.
  virtual bool IsHovering();
  // Returns true if the Handoff Button View is focused.
  virtual bool IsFocused();

  base::WeakPtr<HandoffButtonController> GetWeakPtr();

  // Registers the current tab interface.
  [[nodiscard]] base::ScopedClosureRunner RegisterTabInterface(
      tabs::TabInterface* tab_interface);

 protected:
  void UnregisterTabInterface();
  void OnButtonPressed();
  gfx::Rect GetHandoffButtonBounds();
  void UpdateButtonHoverStatus(bool is_hovered);
  void UpdateButtonFocusStatus(bool is_focused);
  // views::ViewObserver:
  void OnViewFocused(views::View* observed_view) override;
  void OnViewBlurred(views::View* observed_view) override;

  std::unique_ptr<views::WidgetDelegate> delegate_ = nullptr;
  std::unique_ptr<HandoffButtonWidget> widget_ = nullptr;
  raw_ptr<views::LabelButton> button_view_ = nullptr;

 private:
  void CreateAndShowButton(const std::u16string& text,
                           const std::u16string& a11y_text,
                           const ::ui::ImageModel& icon);
  virtual void CloseButton(views::Widget::ClosedReason reason);
  virtual ActorUiTabControllerInterface* GetTabController();
  virtual void UpdateBounds();

  void OnWidgetDestroying(views::Widget::ClosedReason reason);

  bool is_visible_ = false;
  bool is_hovering_ = false;
  bool is_focused_ = false;
  bool was_immersive_ = false;
  bool was_toolbar_pinned_ = false;

  base::ScopedObservation<views::View, views::ViewObserver> view_observer_{
      this};
  HandoffButtonState::ControlOwnership ownership_ =
      HandoffButtonState::ControlOwnership::kActor;

  raw_ptr<views::View> anchor_view_ = nullptr;
  raw_ptr<ActorUiWindowController> window_controller_ = nullptr;
  raw_ptr<tabs::TabInterface> tab_interface_ = nullptr;

  base::WeakPtrFactory<HandoffButtonController> weak_ptr_factory_{this};
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_HANDOFF_BUTTON_CONTROLLER_H_
