// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_item_base.h"

#include <vector>

#include "ash/public/cpp/window_properties.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_group_item.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/snap_group/snap_group.h"
#include "ash/wm/snap_group/snap_group_controller.h"

namespace ash {

OverviewItemBase::OverviewItemBase(OverviewSession* overview_session,
                                   OverviewGrid* overview_grid,
                                   aura::Window* root_window)
    : root_window_(root_window),
      overview_session_(overview_session),
      overview_grid_(overview_grid) {}

OverviewItemBase::~OverviewItemBase() = default;

// static
std::unique_ptr<OverviewItemBase> OverviewItemBase::Create(
    aura::Window* window,
    OverviewSession* overview_session,
    OverviewGrid* overview_grid) {
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  if (snap_group_controller) {
    if (SnapGroup* snap_group =
            snap_group_controller->GetSnapGroupForGivenWindow(window)) {
      return std::make_unique<OverviewGroupItem>(
          std::vector<aura::Window*>{snap_group->window1(),
                                     snap_group->window2()},
          overview_session, overview_grid);
    }
  }

  return std::make_unique<OverviewItem>(window, overview_session,
                                        overview_grid);
}

bool OverviewItemBase::IsDragItem() const {
  return overview_session_->GetCurrentDraggedOverviewItem() == this;
}

views::Widget::InitParams OverviewItemBase::CreateOverviewItemWidgetParams(
    aura::Window* parent_window,
    const std::string& widget_name) const {
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.visible_on_all_workspaces = true;
  params.layer_type = ui::LAYER_NOT_DRAWN;
  params.name = widget_name;
  params.activatable = views::Widget::InitParams::Activatable::kDefault;
  params.accept_events = true;
  params.parent = parent_window;
  params.init_properties_container.SetProperty(kHideInDeskMiniViewKey, true);
  return params;
}

void OverviewItemBase::ConfigureTheShadow() {
  shadow_ = SystemShadow::CreateShadowOnNinePatchLayer(kDefaultShadowType);
  auto* shadow_layer = shadow_->GetLayer();
  auto* widget_layer = item_widget_->GetLayer();
  widget_layer->Add(shadow_layer);
  widget_layer->StackAtBottom(shadow_layer);
  shadow_->ObserveColorProviderSource(item_widget_.get());
}

}  // namespace ash
