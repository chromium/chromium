// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_image_view_delegate.h"

#include <memory>

#include "ash/app_list/model/search/search_result.h"
#include "ash/app_list/views/search_result_image_view.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view_utils.h"

namespace ash {
namespace {
SearchResultImageViewDelegate* g_instance = nullptr;
}  // namespace

// static
SearchResultImageViewDelegate* SearchResultImageViewDelegate::Get() {
  return g_instance;
}

SearchResultImageViewDelegate::SearchResultImageViewDelegate() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

SearchResultImageViewDelegate::~SearchResultImageViewDelegate() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

bool SearchResultImageViewDelegate::CanStartDragForView(
    views::View* sender,
    const gfx::Point& press_pt,
    const gfx::Point& current_pt) {
  const gfx::Vector2d delta = current_pt - press_pt;
  return views::View::ExceededDragThreshold(delta);
}

int SearchResultImageViewDelegate::GetDragOperationsForView(
    views::View* sender,
    const gfx::Point& press_pt) {
  return ui::DragDropTypes::DRAG_COPY;
}

void SearchResultImageViewDelegate::WriteDragDataForView(
    views::View* sender,
    const gfx::Point& press_pt,
    ui::OSExchangeData* data) {
  auto* image_view = views::AsViewClass<SearchResultImageView>(sender);
  DUMP_WILL_BE_CHECK(image_view);
  data->provider().SetDragImage(image_view->CreateDragImage(),
                                press_pt.OffsetFromOrigin());

  base::FilePath file_path = image_view->result()->file_path();
  DUMP_WILL_BE_CHECK(!file_path.empty());
  data->SetFilename(file_path);
  return;
}

}  // namespace ash
