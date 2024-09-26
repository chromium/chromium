// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/birch/tab_app_selection_host.h"

#include "ash/birch/birch_coral_item.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/wm/overview/birch/birch_chip_button.h"
#include "ash/wm/overview/birch/birch_chip_button_base.h"
#include "ash/wm/overview/birch/tab_app_selection_view.h"
#include "ash/wm/window_properties.h"
#include "ui/aura/window.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

TabAppSelectionHost::TabAppSelectionHost(BirchChipButton* coral_chip)
    : owner_(coral_chip) {
  using InitParams = views::Widget::InitParams;
  InitParams params(InitParams::CLIENT_OWNS_WIDGET, InitParams::TYPE_MENU);
  params.accept_events = true;
  params.activatable = InitParams::Activatable::kNo;
  params.autosize = true;
  params.name = "TabAppSelectionMenu";
  params.init_properties_container.SetProperty(kHideInDeskMiniViewKey, true);
  params.init_properties_container.SetProperty(kOverviewUiKey, true);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;

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

gfx::Rect TabAppSelectionHost::GetDesiredBoundsInScreen() {
  const int preferred_height = GetContentsView()->GetPreferredSize().height();
  gfx::Rect selector_bounds = owner_->GetBoundsInScreen();
  selector_bounds.set_y(selector_bounds.y() - preferred_height);
  selector_bounds.set_height(preferred_height);
  return selector_bounds;
}

}  // namespace ash
