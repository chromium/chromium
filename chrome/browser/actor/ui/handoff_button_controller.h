// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_HANDOFF_BUTTON_CONTROLLER_H_
#define CHROME_BROWSER_ACTOR_UI_HANDOFF_BUTTON_CONTROLLER_H_

#include <string_view>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/ui/states/handoff_button_state.h"
#include "ui/views/widget/widget.h"

namespace views {
class LabelButton;
class Widget;
class WidgetDelegate;
}  // namespace views

namespace tabs {
class TabInterface;
class TabDialogManager;
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

class HandoffButtonController {
 public:
  explicit HandoffButtonController(tabs::TabInterface& tab_interface);
  virtual ~HandoffButtonController();

  HandoffButtonController(const HandoffButtonController&) = delete;
  HandoffButtonController& operator=(const HandoffButtonController&) = delete;

  virtual void UpdateState(const HandoffButtonState& state, bool is_visible);

 protected:
  void OnButtonPressed();
  void ShouldShowButton(bool& show);
  gfx::Rect GetHandoffButtonBounds(views::Widget* widget);
  void UpdateButtonHoverStatus(bool is_hovered);

  std::unique_ptr<views::WidgetDelegate> delegate_ = nullptr;
  std::unique_ptr<HandoffButtonWidget> widget_ = nullptr;
  raw_ptr<views::LabelButton> button_view_ = nullptr;

 private:
  void CreateAndShowButton(const std::u16string& text,
                           const ::ui::ImageModel& icon);
  virtual void CloseButton(views::Widget::ClosedReason reason);
  virtual ActorUiTabControllerInterface* GetTabController();
  virtual void UpdateBounds();
  virtual void UpdateVisibility();

  tabs::TabDialogManager* GetTabDialogManager();

  bool is_active_ = false;
  bool is_visible_ = false;
  HandoffButtonState::ControlOwnership ownership_ =
      HandoffButtonState::ControlOwnership::kActor;
  const raw_ref<tabs::TabInterface> tab_interface_;

  base::WeakPtrFactory<HandoffButtonController> weak_ptr_factory_{this};
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_HANDOFF_BUTTON_CONTROLLER_H_
