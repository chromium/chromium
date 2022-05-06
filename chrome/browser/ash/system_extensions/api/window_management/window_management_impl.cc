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
#include "ui/aura/client/focus_client.h"
#include "ui/base/ui_base_types.h"
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
  std::vector<blink::mojom::CrosWindowInfoPtr> windows;
  proxy->InstanceRegistry().ForEachInstance(
      [&windows](const apps::InstanceUpdate& update) {
        auto window = blink::mojom::CrosWindowInfo::New();
        aura::Window* target = update.Window()->GetToplevelWindow();
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

        // Set window state (states are mutually exclusive)
        if (widget->IsFullscreen()) {
          window->window_state = blink::mojom::WindowState::kFullscreen;
        } else if (widget->IsMaximized()) {
          window->window_state = blink::mojom::WindowState::kMaximized;
        } else if (widget->IsMinimized()) {
          window->window_state = blink::mojom::WindowState::kMinimized;
        } else {
          window->window_state = blink::mojom::WindowState::kNormal;
        }
        // Instance registry references the activatable component of a window
        // which itself does not have focus but contains the child focusable. To
        // detect focus on the window, we assert that the focused window has our
        // activatable as its top level parent
        window->is_focused = target == aura::client::GetFocusClient(target)
                                           ->GetFocusedWindow()
                                           ->GetToplevelWindow();
        window->visibility_state = widget->IsVisible()
                                       ? blink::mojom::VisibilityState::kShown
                                       : blink::mojom::VisibilityState::kHidden;
        windows.push_back(std::move(window));
      });
  std::move(callback).Run(std::move(windows));
}

void WindowManagementImpl::SetWindowBounds(const base::UnguessableToken& id,
                                           int32_t x,
                                           int32_t y,
                                           int32_t width,
                                           int32_t height,
                                           SetWindowBoundsCallback callback) {
  aura::Window* target = GetWindow(id);
  // TODO(crbug.com/1253318): Ensure this works with multiple screens.
  if (!target) {
    std::move(callback).Run(
        blink::mojom::CrosWindowManagementStatus::kWindowNotFound);
    return;
  }

  target->SetBounds(gfx::Rect(x, y, width, height));
  std::move(callback).Run(blink::mojom::CrosWindowManagementStatus::kSuccess);
}

void WindowManagementImpl::SetFullscreen(const base::UnguessableToken& id,
                                         bool fullscreen,
                                         SetFullscreenCallback callback) {
  views::Widget* widget = GetWidget(id);
  if (!widget) {
    std::move(callback).Run(
        blink::mojom::CrosWindowManagementStatus::kWindowNoWidget);
    return;
  }
  widget->SetFullscreen(fullscreen);
  std::move(callback).Run(blink::mojom::CrosWindowManagementStatus::kSuccess);
}

void WindowManagementImpl::Maximize(const base::UnguessableToken& id,
                                    MaximizeCallback callback) {
  aura::Window* target = GetWindow(id);
  if (!target) {
    std::move(callback).Run(
        blink::mojom::CrosWindowManagementStatus::kWindowNotFound);
    return;
  }

  // Returns null when id points to non top level window or is null itself.
  WindowState* state = WindowState::Get(target);
  if (!state) {
    std::move(callback).Run(
        blink::mojom::CrosWindowManagementStatus::kWindowNoWindowState);
    return;
  }

  state->Maximize();
  std::move(callback).Run(blink::mojom::CrosWindowManagementStatus::kSuccess);
}

void WindowManagementImpl::Minimize(const base::UnguessableToken& id,
                                    MinimizeCallback callback) {
  aura::Window* target = GetWindow(id);
  if (!target) {
    std::move(callback).Run(
        blink::mojom::CrosWindowManagementStatus::kWindowNotFound);
    return;
  }

  // Returns null when id points to non top level window or is null itself.
  WindowState* state = WindowState::Get(target);
  if (!state) {
    std::move(callback).Run(
        blink::mojom::CrosWindowManagementStatus::kWindowNoWindowState);
    return;
  }

  state->Minimize();
  std::move(callback).Run(blink::mojom::CrosWindowManagementStatus::kSuccess);
}

void WindowManagementImpl::Focus(const base::UnguessableToken& id,
                                 FocusCallback callback) {
  aura::Window* target = GetWindow(id);
  if (!target) {
    std::move(callback).Run(
        blink::mojom::CrosWindowManagementStatus::kWindowNotFound);
    return;
  }
  target->Focus();
  std::move(callback).Run(blink::mojom::CrosWindowManagementStatus::kSuccess);
}

void WindowManagementImpl::Close(const base::UnguessableToken& id) {
  views::Widget* widget = GetWidget(id);
  if (widget) {
    widget->Close();
  }
}

aura::Window* WindowManagementImpl::GetWindow(
    const base::UnguessableToken& id) {
  aura::Window* target = nullptr;
  apps::AppServiceProxy* proxy = apps::AppServiceProxyFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context_));
  proxy->InstanceRegistry().ForOneInstance(
      id, [&target](const apps::InstanceUpdate& update) {
        target = update.Window()->GetToplevelWindow();
      });

  return target;
}

views::Widget* WindowManagementImpl::GetWidget(
    const base::UnguessableToken& id) {
  aura::Window* target = GetWindow(id);

  if (!target) {
    return nullptr;
  }

  views::Widget* widget = views::Widget::GetTopLevelWidgetForNativeView(target);
  return widget;
}

}  // namespace ash
