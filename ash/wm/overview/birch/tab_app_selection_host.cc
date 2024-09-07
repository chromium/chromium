// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/birch/tab_app_selection_host.h"

#include "ash/birch/birch_item.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/wm/overview/birch/birch_bar_controller.h"
#include "ash/wm/overview/birch/birch_bar_view.h"
#include "ash/wm/overview/birch/birch_chip_button_base.h"
#include "ash/wm/overview/birch/tab_app_selection_view.h"
#include "ash/wm/window_properties.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

BirchChipButtonBase* GetCoralChip() {
  BirchBarView* bar_view = BirchBarController::Get()->primary_birch_bar_view();
  auto it =
      base::ranges::find_if(bar_view->chips(), [](BirchChipButtonBase* button) {
        return button->GetItem()->GetType() == BirchItemType::kCoral;
      });
  return it == bar_view->chips().end() ? nullptr : *it;
}

}  // namespace

TabAppSelectionHost::TabAppSelectionHost(BirchChipButtonBase* coral_button)
    : owner_(coral_button) {}

TabAppSelectionHost::~TabAppSelectionHost() = default;

// static
std::unique_ptr<TabAppSelectionHost> TabAppSelectionHost::Create() {
  BirchChipButtonBase* coral_chip = GetCoralChip();
  if (!coral_chip) {
    return nullptr;
  }

  using InitParams = views::Widget::InitParams;
  InitParams params(InitParams::CLIENT_OWNS_WIDGET, InitParams::TYPE_POPUP);
  params.accept_events = true;
  params.activatable = InitParams::Activatable::kYes;
  params.autosize = true;
  params.name = "TabAppSelectionMenu";
  params.init_properties_container.SetProperty(kHideInDeskMiniViewKey, true);
  params.init_properties_container.SetProperty(kOverviewUiKey, true);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;

  auto widget = std::make_unique<TabAppSelectionHost>(coral_chip);
  widget->Init(std::move(params));
  widget->SetContentsView(std::make_unique<TabAppSelectionView>());
  widget->widget_delegate()->set_desired_bounds_delegate(
      base::BindRepeating(&TabAppSelectionHost::GetDesiredBoundsInScreen,
                          base::Unretained(widget.get())));
  widget->Show();
  widget->SetBounds(widget->GetDesiredBoundsInScreen());
  return widget;
}

gfx::Rect TabAppSelectionHost::GetDesiredBoundsInScreen() {
  const int preferred_height = GetContentsView()->GetPreferredSize().height();
  gfx::Rect selector_bounds = owner_->GetBoundsInScreen();
  selector_bounds.set_y(selector_bounds.y() - preferred_height);
  selector_bounds.set_height(preferred_height);
  return selector_bounds;
}

}  // namespace ash
