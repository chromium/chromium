// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_HANDOFF_BUTTON_CONTROLLER_H_
#define CHROME_BROWSER_ACTOR_UI_HANDOFF_BUTTON_CONTROLLER_H_

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

class HandoffButtonController {
 public:
  explicit HandoffButtonController(tabs::TabInterface& tab_interface);
  virtual ~HandoffButtonController();

  HandoffButtonController(const HandoffButtonController&) = delete;
  HandoffButtonController& operator=(const HandoffButtonController&) = delete;

  void UpdateState(const HandoffButtonState& state, bool is_visible);

 protected:
  void OnButtonPressed();
  void ShouldShowButton(bool& show);

  std::unique_ptr<views::WidgetDelegate> delegate_ = nullptr;
  std::unique_ptr<views::Widget> widget_ = nullptr;
  raw_ptr<views::LabelButton> button_view_ = nullptr;

 private:
  void CreateAndShowButton(const std::u16string& text,
                           const ::ui::ImageModel& icon);
  virtual void CloseButton(views::Widget::ClosedReason reason);
  tabs::TabDialogManager* GetTabDialogManager();

  bool is_active_ = false;
  bool is_visible_ = false;
  const raw_ref<tabs::TabInterface> tab_interface_;

  base::WeakPtrFactory<HandoffButtonController> weak_ptr_factory_{this};
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_HANDOFF_BUTTON_CONTROLLER_H_
