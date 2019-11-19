// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_menu_model_adapter.h"

#include "base/metrics/histogram_macros.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/view.h"

namespace ash {

ShelfMenuModelAdapter::ShelfMenuModelAdapter(
    const std::string& app_id,
    std::unique_ptr<ui::SimpleMenuModel> model,
    views::View* menu_owner,
    ui::MenuSourceType source_type,
    base::OnceClosure on_menu_closed_callback,
    bool is_tablet_mode)
    : AppMenuModelAdapter(app_id,
                          std::move(model),
                          menu_owner->GetWidget(),
                          source_type,
                          std::move(on_menu_closed_callback),
                          is_tablet_mode),
      menu_owner_(menu_owner) {}

ShelfMenuModelAdapter::~ShelfMenuModelAdapter() = default;

void ShelfMenuModelAdapter::RecordHistogramOnMenuClosed() {
  base::TimeDelta user_journey_time = base::TimeTicks::Now() - menu_open_time();
  // If the menu is for a ShelfButton.
  if (!app_id().empty()) {
    UMA_HISTOGRAM_TIMES("Apps.ContextMenuUserJourneyTime.ShelfButton",
                        user_journey_time);
    UMA_HISTOGRAM_ENUMERATION("Apps.ContextMenuShowSource.ShelfButton",
                              source_type(), ui::MENU_SOURCE_TYPE_LAST);
    if (is_tablet_mode()) {
      UMA_HISTOGRAM_TIMES(
          "Apps.ContextMenuUserJourneyTime.ShelfButton.TabletMode",
          user_journey_time);
      UMA_HISTOGRAM_ENUMERATION(
          "Apps.ContextMenuShowSource.ShelfButton.TabletMode", source_type(),
          ui::MenuSourceType::MENU_SOURCE_TYPE_LAST);
    } else {
      UMA_HISTOGRAM_TIMES(
          "Apps.ContextMenuUserJourneyTime.ShelfButton.ClamshellMode",
          user_journey_time);
      UMA_HISTOGRAM_ENUMERATION(
          "Apps.ContextMenuShowSource.ShelfButton.ClamshellMode", source_type(),
          ui::MenuSourceType::MENU_SOURCE_TYPE_LAST);
    }
    return;
  }

  UMA_HISTOGRAM_TIMES("Apps.ContextMenuUserJourneyTime.Shelf",
                      user_journey_time);
  UMA_HISTOGRAM_ENUMERATION("Apps.ContextMenuShowSource.Shelf", source_type(),
                            ui::MENU_SOURCE_TYPE_LAST);
  if (is_tablet_mode()) {
    UMA_HISTOGRAM_TIMES("Apps.ContextMenuUserJourneyTime.Shelf.TabletMode",
                        user_journey_time);
    UMA_HISTOGRAM_ENUMERATION("Apps.ContextMenuShowSource.Shelf.TabletMode",
                              source_type(),
                              ui::MenuSourceType::MENU_SOURCE_TYPE_LAST);
  } else {
    UMA_HISTOGRAM_TIMES("Apps.ContextMenuUserJourneyTime.Shelf.ClamshellMode",
                        user_journey_time);
    UMA_HISTOGRAM_ENUMERATION("Apps.ContextMenuShowSource.Shelf.ClamshellMode",
                              source_type(),
                              ui::MenuSourceType::MENU_SOURCE_TYPE_LAST);
  }
}

bool ShelfMenuModelAdapter::IsShowingMenuForView(
    const views::View& view) const {
  return IsShowingMenu() && menu_owner_ == &view;
}

}  // namespace ash
