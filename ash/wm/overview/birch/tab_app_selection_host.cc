// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/birch/tab_app_selection_host.h"

#include "ash/birch/birch_coral_item.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/style/icon_button.h"
#include "ash/wm/overview/birch/birch_chip_button.h"
#include "ash/wm/overview/birch/birch_chip_button_base.h"
#include "ash/wm/overview/birch/tab_app_selection_view.h"
#include "ash/wm/window_properties.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/aura/window.h"
#include "ui/events/event_handler.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

class TabAppSelectionHost::SelectionHostHider : public ui::EventHandler {
 public:
  explicit SelectionHostHider(TabAppSelectionHost* owner) : owner_(owner) {
    Shell::Get()->AddPreTargetHandler(this);
  }
  SelectionHostHider(const SelectionHostHider&) = delete;
  SelectionHostHider& operator=(const SelectionHostHider&) = delete;
  ~SelectionHostHider() override { Shell::Get()->RemovePreTargetHandler(this); }

  // ui::EventHandler:
  void OnEvent(ui::Event* event) override {
    if (event->type() == ui::EventType::kMousePressed ||
        event->type() == ui::EventType::kTouchPressed) {
      // Ignore all events if the host widget is not visible.
      if (!owner_->IsVisible()) {
        return;
      }

      gfx::Point event_screen_point = event->AsLocatedEvent()->root_location();
      wm::ConvertPointToScreen(
          static_cast<aura::Window*>(event->target())->GetRootWindow(),
          &event_screen_point);
      // Unless the event is on the host widget, hide it and stop the event from
      // propagating.
      if (!owner_->GetWindowBoundsInScreen().Contains(event_screen_point)) {
        owner_->Hide();
        event->SetHandled();
        event->StopPropagation();
      }
    }
  }
  std::string_view GetLogContext() const override {
    return "TabAppSelectionHost::SelectionHostHider";
  }

 private:
  const raw_ptr<TabAppSelectionHost> owner_;
};

TabAppSelectionHost::TabAppSelectionHost(BirchChipButton* coral_chip)
    : hider_(std::make_unique<SelectionHostHider>(this)), owner_(coral_chip) {
  using InitParams = views::Widget::InitParams;
  InitParams params(InitParams::CLIENT_OWNS_WIDGET, InitParams::TYPE_MENU);
  params.accept_events = true;
  params.activatable = InitParams::Activatable::kNo;
  params.autosize = true;
  params.name = "TabAppSelectionMenu";
  params.init_properties_container.SetProperty(kHideInDeskMiniViewKey, true);
  params.init_properties_container.SetProperty(kOverviewUiKey, true);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;

  Init(std::move(params));
  SetContentsView(std::make_unique<TabAppSelectionView>(
      static_cast<BirchCoralItem*>(coral_chip->GetItem())));
  widget_delegate()->set_desired_bounds_delegate(base::BindRepeating(
      &TabAppSelectionHost::GetDesiredBoundsInScreen, base::Unretained(this)));
  SetBounds(GetDesiredBoundsInScreen());
}

TabAppSelectionHost::~TabAppSelectionHost() = default;

void TabAppSelectionHost::ProcessKeyEvent(ui::KeyEvent* event) {
  if (event->type() != ui::EventType::kKeyPressed) {
    return;
  }

  event->SetHandled();
  event->StopPropagation();

  if (event->key_code() == ui::VKEY_ESCAPE) {
    Hide();
    return;
  }
  views::AsViewClass<TabAppSelectionView>(GetContentsView())
      ->ProcessKeyEvent(event);
}

void TabAppSelectionHost::OnNativeWidgetVisibilityChanged(bool visible) {
  views::Widget::OnNativeWidgetVisibilityChanged(visible);
  views::AsViewClass<IconButton>(owner_->addon_view())
      ->SetVectorIcon(visible ? vector_icons::kCaretDownIcon
                              : vector_icons::kCaretUpIcon);
  owner_->SetTopHalfRounded(!visible);
}

gfx::Rect TabAppSelectionHost::GetDesiredBoundsInScreen() {
  const int preferred_height = GetContentsView()->GetPreferredSize().height();
  gfx::Rect selector_bounds = owner_->GetBoundsInScreen();
  selector_bounds.set_y(selector_bounds.y() - preferred_height);
  selector_bounds.set_height(preferred_height);
  return selector_bounds;
}

}  // namespace ash
