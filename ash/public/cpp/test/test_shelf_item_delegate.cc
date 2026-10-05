// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/test_shelf_item_delegate.h"

#include <string>
#include <vector>

#include "base/types/expected.h"
#include "ui/aura/window.h"

namespace ash {

TestShelfItemDelegate::TestShelfItemDelegate(const ShelfID& shelf_id)
    : ShelfItemDelegate(shelf_id) {}

TestShelfItemDelegate::~TestShelfItemDelegate() = default;

void TestShelfItemDelegate::AddWindow(aura::Window* window) {
  windows_.push_back(window);
  window_observations_.AddObservation(window);
}

ShelfItemDelegate::AppMenuItems TestShelfItemDelegate::GetAppMenuItems(
    int event_flags,
    const ItemFilterPredicate& filter_predicate) {
  AppMenuItems items;
  int command_id = -1;
  for (aura::Window* window : windows_) {
    ++command_id;
    if (!filter_predicate.is_null() && !filter_predicate.Run(window)) {
      continue;
    }
    items.push_back({command_id, window->GetTitle(), gfx::ImageSkia()});
  }
  return items;
}

base::expected<aura::Window*, std::u16string>
TestShelfItemDelegate::GetAppMenuItemWindow(int command_id) {
  if (command_id < 0 || static_cast<size_t>(command_id) >= windows_.size()) {
    return base::unexpected(std::u16string());
  }
  return windows_[command_id];
}

void TestShelfItemDelegate::ExecuteCommand(bool from_context_menu,
                                           int64_t command_id,
                                           int32_t event_flags,
                                           int64_t display_id) {}

void TestShelfItemDelegate::Close() {}

void TestShelfItemDelegate::OnWindowDestroying(aura::Window* window) {
  window_observations_.RemoveObservation(window);
  std::erase(windows_, window);
}

}  // namespace ash
