// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/birch/tab_app_selection_host.h"

#include "ash/public/cpp/window_properties.h"
#include "ash/wm/overview/birch/birch_chip_button.h"
#include "ash/wm/overview/birch/tab_app_selection_view.h"
#include "ash/wm/window_properties.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

TabAppSelectionHost::TabAppSelectionHost(BirchChipButton* coral_chip)
    : owner_(coral_chip) {
  using InitParams = views::Widget::InitParams;
  InitParams params(InitParams::CLIENT_OWNS_WIDGET, InitParams::TYPE_POPUP);
  params.accept_events = true;
  params.activatable = InitParams::Activatable::kYes;
  params.autosize = true;
  params.name = "TabAppSelectionMenu";
  params.init_properties_container.SetProperty(kHideInDeskMiniViewKey, true);
  params.init_properties_container.SetProperty(kOverviewUiKey, true);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.parent = coral_chip->GetWidget()->GetNativeWindow()->parent();

  Init(std::move(params));
  SetContentsView(std::make_unique<TabAppSelectionView>());
  widget_delegate()->set_desired_bounds_delegate(base::BindRepeating(
      &TabAppSelectionHost::GetDesiredBoundsInScreen, base::Unretained(this)));
  SetBounds(GetDesiredBoundsInScreen());
}

TabAppSelectionHost::~TabAppSelectionHost() = default;

gfx::Rect TabAppSelectionHost::GetDesiredBoundsInScreen() {
  const int preferred_height = GetContentsView()->GetPreferredSize().height();
  gfx::Rect selector_bounds = owner_->GetBoundsInScreen();
  selector_bounds.set_y(selector_bounds.y() - preferred_height);
  selector_bounds.set_height(preferred_height);
  return selector_bounds;
}

}  // namespace ash
