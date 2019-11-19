// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_test_api_impl.h"

#include "ash/root_window_controller.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"

namespace {

ash::Shelf* GetShelf() {
  return ash::Shell::Get()->GetPrimaryRootWindowController()->shelf();
}

ash::ShelfWidget* GetShelfWidget() {
  return ash::Shell::GetRootWindowControllerWithDisplayId(
             display::Screen::GetScreen()->GetPrimaryDisplay().id())
      ->shelf()
      ->shelf_widget();
}

}  // namespace

namespace ash {

ShelfTestApiImpl::ShelfTestApiImpl() = default;
ShelfTestApiImpl::~ShelfTestApiImpl() = default;

bool ShelfTestApiImpl::IsVisible() {
  return GetShelf()->shelf_layout_manager()->IsVisible();
}

bool ShelfTestApiImpl::IsAlignmentBottomLocked() {
  return GetShelf()->alignment() == SHELF_ALIGNMENT_BOTTOM_LOCKED;
}

views::View* ShelfTestApiImpl::GetHomeButton() {
  return GetShelfWidget()->navigation_widget()->GetHomeButton();
}

// static
std::unique_ptr<ShelfTestApi> ShelfTestApi::Create() {
  return std::make_unique<ShelfTestApiImpl>();
}

}  // namespace ash
