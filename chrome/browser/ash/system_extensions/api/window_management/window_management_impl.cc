// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_extensions/api/window_management/window_management_impl.h"

#include "ash/wm/window_state.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/services/app_service/public/cpp/instance.h"
#include "components/services/app_service/public/mojom/types.mojom-shared.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "third_party/blink/public/mojom/chromeos/system_extensions/window_management/cros_window_management.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

WindowManagementImpl::WindowManagementImpl(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

void WindowManagementImpl::GetAllWindows(GetAllWindowsCallback callback) {
  apps::AppServiceProxy* proxy = apps::AppServiceProxyFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context_));
  std::vector<blink::mojom::CrosWindowPtr> windows;
  proxy->InstanceRegistry().ForEachInstance(
      [&windows](const apps::InstanceUpdate& update) {
        auto window = blink::mojom::CrosWindow::New();
        aura::Window* target = update.Window();
        views::Widget* widget =
            views::Widget::GetTopLevelWidgetForNativeView(target);
        if (!target || !widget) {
          return;
        }
        window->id = update.InstanceId();
        window->title =
            base::UTF16ToUTF8(widget->widget_delegate()->GetWindowTitle());
        window->app_id = update.AppId();
        window->bounds = target->bounds();
        window->is_fullscreen = widget->IsFullscreen();
        window->is_minimized = widget->IsMinimized();
        window->is_visible = widget->IsVisible();
        windows.push_back(std::move(window));
      });
  std::move(callback).Run(std::move(windows));
}

void WindowManagementImpl::SetWindowBounds(const base::UnguessableToken& id,
                                           int32_t x,
                                           int32_t y,
                                           int32_t width,
                                           int32_t height) {
  aura::Window* target = GetWindow(id);
  // TODO(crbug.com/1253318): Ensure this works with multiple screens.
  if (target) {
    target->SetBounds(gfx::Rect(x, y, width, height));
  }
}

void WindowManagementImpl::SetFullscreen(const base::UnguessableToken& id,
                                         bool fullscreen) {
  aura::Window* target = GetWindow(id);
  // TODO(b/223320570): Add error handling for stale ids.
  if (!target) {
    return;
  }
  views::Widget::GetWidgetForNativeWindow(target)->SetFullscreen(fullscreen);
}

void WindowManagementImpl::Maximize(const base::UnguessableToken& id) {
  aura::Window* target = GetWindow(id);
  // TODO(b/223320570): Add error handling for stale ids.
  if (target) {
    WindowState::Get(target)->Maximize();
  }
}

void WindowManagementImpl::Minimize(const base::UnguessableToken& id) {
  aura::Window* target = GetWindow(id);
  // TODO(b/223320570): Add error handling for stale ids.
  if (target) {
    WindowState::Get(target)->Minimize();
  }
}

void WindowManagementImpl::Focus(const base::UnguessableToken& id) {
  aura::Window* target = GetWindow(id);
  // TODO(b/223320570): Add error handling for stale ids.
  if (target) {
    target->Focus();
  }
}

void WindowManagementImpl::Close(const base::UnguessableToken& id) {
  aura::Window* target = GetWindow(id);
  // TODO(b/223320570): Add error handling for stale ids.
  if (target) {
    views::Widget::GetWidgetForNativeWindow(target)->Close();
  }
}

aura::Window* WindowManagementImpl::GetWindow(
    const base::UnguessableToken& id) {
  aura::Window* target = nullptr;
  apps::AppServiceProxy* proxy = apps::AppServiceProxyFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context_));
  proxy->InstanceRegistry().ForOneInstance(
      id, [&target](const apps::InstanceUpdate& update) {
        target = update.Window();
      });

  return target;
}

}  // namespace ash
