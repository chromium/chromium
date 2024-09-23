// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_announcer.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"

namespace ash::input_method {
namespace {

gfx::NativeView GetParentViewFromRootWindow() {
  aura::Window* active_window = ash::window_util::GetActiveWindow();
  return ash::Shell::GetContainer(
      active_window ? active_window->GetRootWindow()
                    : ash::Shell::GetRootWindowForNewWindows(),
      ash::kShellWindowId_MenuContainer);
}

}  // namespace

EditorLiveRegionAnnouncer::EditorLiveRegionAnnouncer(std::u16string_view name)
    : live_region_(name) {}

void EditorLiveRegionAnnouncer::Announce(const std::u16string& message) {
  live_region_.Announce(message);
}

EditorLiveRegionAnnouncer::LiveRegion::LiveRegion(std::u16string_view name)
    : announcement_view_name_(name) {}

EditorLiveRegionAnnouncer::LiveRegion::~LiveRegion() = default;

void EditorLiveRegionAnnouncer::LiveRegion::Announce(
    const std::u16string& message) {
  if (announcement_view_ == nullptr) {
    CreateAnnouncementView();
  }
  announcement_view_->Announce(message);
}

void EditorLiveRegionAnnouncer::LiveRegion::OnWidgetDestroying(
    views::Widget* widget) {
  if (announcement_view_ != nullptr &&
      widget == announcement_view_->GetWidget()) {
    obs_.Reset();
    announcement_view_ = nullptr;
  }
}

void EditorLiveRegionAnnouncer::LiveRegion::CreateAnnouncementView() {
  // This view's lifetime is handled by DialogDelegateView which it inherits
  // from and thus will not leak here without a corresponding delete.
  announcement_view_ = new ui::ime::AnnouncementView(
      GetParentViewFromRootWindow(), announcement_view_name_);
  obs_.Observe(announcement_view_->GetWidget());
}

}  // namespace ash::input_method
