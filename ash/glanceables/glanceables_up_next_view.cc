// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/glanceables_up_next_view.h"

#include <memory>
#include <string>
#include <tuple>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace ash {

// TODO(crbug.com/1353495): file-level todo list:
// - fetch events for today using the existing calendar api/model;
// - limit events list height and switch to `views::ScrollView`;
// - remove events from the list on their end times;
// - move fonts/colors/sizes to a config.

GlanceablesUpNextView::GlanceablesUpNextView() {
  SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(15, 0)));
  SetLayoutManager(std::make_unique<views::BoxLayout>());
  CreateEventsListView();
}

GlanceablesUpNextView::~GlanceablesUpNextView() = default;

void GlanceablesUpNextView::CreateEventsListItemView() {
  auto* item = events_list_view_->AddChildView(std::make_unique<views::View>());
  auto* layout = item->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 10));

  auto* event_title_label =
      item->AddChildView(std::make_unique<views::Label>(u"James / Artsiom"));
  event_title_label->SetAutoColorReadabilityEnabled(false);
  event_title_label->SetEnabledColor(SK_ColorWHITE);
  event_title_label->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_LEFT);

  auto* event_time_label =
      item->AddChildView(std::make_unique<views::Label>(u"2:00 – 2:30pm"));
  event_time_label->SetAutoColorReadabilityEnabled(false);
  event_time_label->SetEnabledColor(SK_ColorWHITE);

  events_list_items_views_.emplace_back(event_title_label, event_time_label);

  layout->SetFlexForView(event_title_label, 1);
  layout->SetFlexForView(event_time_label, 0, true);
}

void GlanceablesUpNextView::CreateEventsListView() {
  events_list_view_ = AddChildView(std::make_unique<views::View>());
  events_list_view_->SetPreferredSize(gfx::Size(300, 150));
  auto* events_list_layout =
      events_list_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  events_list_layout->set_between_child_spacing(4);

  for (size_t i = 0; i < 5; ++i)
    CreateEventsListItemView();
}

BEGIN_METADATA(GlanceablesUpNextView, views::View)
END_METADATA

}  // namespace ash
