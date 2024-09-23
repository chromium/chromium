// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/screen_orientation_delegate_lacros.h"

#include "content/public/browser/web_contents.h"
#include "ui/display/tablet_state.h"
#include "ui/platform_window/extensions/wayland_extension.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_lacros.h"
#include "ui/views/widget/widget.h"

namespace {

ui::WaylandOrientationLockType ToWaylandOrientationLockType(
    device::mojom::ScreenOrientationLockType blink_orientation_lock) {
  switch (blink_orientation_lock) {
    case device::mojom::ScreenOrientationLockType::DEFAULT:
    case device::mojom::ScreenOrientationLockType::ANY:
      return ui::WaylandOrientationLockType::kAny;
    case device::mojom::ScreenOrientationLockType::PORTRAIT:
      return ui::WaylandOrientationLockType::kPortrait;
    case device::mojom::ScreenOrientationLockType::PORTRAIT_PRIMARY:
      return ui::WaylandOrientationLockType::kPortraitPrimary;
    case device::mojom::ScreenOrientationLockType::PORTRAIT_SECONDARY:
      return ui::WaylandOrientationLockType::kPortraitSecondary;
    case device::mojom::ScreenOrientationLockType::LANDSCAPE:
      return ui::WaylandOrientationLockType::kLandscape;
    case device::mojom::ScreenOrientationLockType::LANDSCAPE_PRIMARY:
      return ui::WaylandOrientationLockType::kLandscapePrimary;
    case device::mojom::ScreenOrientationLockType::LANDSCAPE_SECONDARY:
      return ui::WaylandOrientationLockType::kLandscapeSecondary;
    case device::mojom::ScreenOrientationLockType::NATURAL:
      return ui::WaylandOrientationLockType::kNatural;
  }
  NOTREACHED_IN_MIGRATION();
  return ui::WaylandOrientationLockType::kAny;
}

}  // namespace

ScreenOrientationDelegateLacros::ScreenOrientationDelegateLacros() {
  content::WebContents::SetScreenOrientationDelegate(this);
}

ScreenOrientationDelegateLacros::~ScreenOrientationDelegateLacros() {
  content::WebContents::SetScreenOrientationDelegate(nullptr);
}

bool ScreenOrientationDelegateLacros::FullScreenRequired(
    content::WebContents* web_contents) {
  return true;
}

ui::WaylandToplevelExtension* GetWaylandToplevelExtensionFromWebContents(
    content::WebContents* web_contents) {
  aura::Window* window = web_contents->GetNativeView();
  if (!window->GetHost())
    return nullptr;

  auto* dwth_platform =
      views::DesktopWindowTreeHostLacros::From(window->GetHost());
  if (!dwth_platform)
    return nullptr;

  return dwth_platform->GetWaylandToplevelExtension();
}

void ScreenOrientationDelegateLacros::Lock(
    content::WebContents* web_contents,
    device::mojom::ScreenOrientationLockType orientation_lock) {
  auto* wayland_extension =
      GetWaylandToplevelExtensionFromWebContents(web_contents);
  if (!wayland_extension)
    return;

  wayland_extension->Lock(ToWaylandOrientationLockType(orientation_lock));
}

bool ScreenOrientationDelegateLacros::ScreenOrientationProviderSupported(
    content::WebContents* web_contents) {
  auto* wayland_extension =
      GetWaylandToplevelExtensionFromWebContents(web_contents);
  if (!wayland_extension)
    return false;

  return wayland_extension->GetTabletMode();
}

void ScreenOrientationDelegateLacros::Unlock(
    content::WebContents* web_contents) {
  auto* wayland_extension =
      GetWaylandToplevelExtensionFromWebContents(web_contents);
  if (wayland_extension == nullptr)
    return;

  wayland_extension->Unlock();
}
