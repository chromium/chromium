// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ws/window_service_owner.h"

#include <cstdint>

#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/non_client_frame_controller.h"
#include "ash/ws/window_service_delegate_impl.h"
#include "base/lazy_instance.h"
#include "base/unguessable_token.h"
#include "services/content/public/cpp/buildflags.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/ws/public/cpp/host/gpu_interface_provider.h"
#include "services/ws/window_service.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/wm/core/focus_controller.h"

#if BUILDFLAG(ENABLE_REMOTE_NAVIGABLE_CONTENTS_VIEW)
#include "services/content/public/cpp/navigable_contents_view.h"
#include "services/ws/remote_view_host/server_remote_view_host.h"
#endif  // BUILDFLAG(ENABLE_REMOTE_NAVIGABLE_CONTENTS_VIEW)

namespace ash {

#if BUILDFLAG(ENABLE_REMOTE_NAVIGABLE_CONTENTS_VIEW)

namespace {

class ServerRemoteContentViewManager
    : public content::NavigableContentsView::RemoteViewManager {
 public:
  explicit ServerRemoteContentViewManager(ws::WindowService* window_service)
      : window_service_(window_service) {}
  ~ServerRemoteContentViewManager() override = default;

  // content::NavigableContentsView::RemoteViewManager:
  std::unique_ptr<views::NativeViewHost> CreateRemoteViewHost() override {
    return std::make_unique<ws::ServerRemoteViewHost>(window_service_);
  }

  void EmbedUsingToken(views::NativeViewHost* view_host,
                       const base::UnguessableToken& token) override {
    constexpr uint32_t kEmbedFlags =
        ws::mojom::kEmbedFlagEmbedderControlsVisibility;
    static_cast<ws::ServerRemoteViewHost*>(view_host)->EmbedUsingToken(
        token, kEmbedFlags, base::DoNothing());
  }

 private:
  ws::WindowService* const window_service_;

  DISALLOW_COPY_AND_ASSIGN(ServerRemoteContentViewManager);
};

}  // namespace

#endif  // BUILDFLAG(ENABLE_REMOTE_NAVIGABLE_CONTENTS_VIEW)

WindowServiceOwner::WindowServiceOwner(
    std::unique_ptr<ws::GpuInterfaceProvider> gpu_interface_provider)
    : window_service_delegate_(std::make_unique<WindowServiceDelegateImpl>()),
      owned_window_service_(
          std::make_unique<ws::WindowService>(window_service_delegate_.get(),
                                              std::move(gpu_interface_provider),
                                              Shell::Get()->focus_controller(),
                                              !::features::IsMultiProcessMash(),
                                              Shell::Get()->aura_env())),
      window_service_(owned_window_service_.get()) {
  window_service_->SetFrameDecorationValues(
      NonClientFrameController::GetPreferredClientAreaInsets(),
      NonClientFrameController::GetMaxTitleBarButtonWidth());
  window_service_->SetDisplayForNewWindows(
      display::Screen::GetScreen()->GetDisplayForNewWindows().id());
  RegisterWindowProperties(window_service_->property_converter());

#if BUILDFLAG(ENABLE_REMOTE_NAVIGABLE_CONTENTS_VIEW)
  content::NavigableContentsView::SetRemoteViewManager(
      std::make_unique<ServerRemoteContentViewManager>(window_service_));
#endif  // BUILDFLAG(ENABLE_REMOTE_NAVIGABLE_CONTENTS_VIEW)
}

WindowServiceOwner::~WindowServiceOwner() = default;

void WindowServiceOwner::BindWindowService(
    service_manager::mojom::ServiceRequest request) {
  // This should only be called once. If called more than once it means the
  // WindowService lost its connection to the service_manager, which triggered
  // a new WindowService to be created. That should never happen.
  DCHECK(!service_context_);

  service_context_ = std::make_unique<service_manager::ServiceContext>(
      std::move(owned_window_service_), std::move(request));
}

}  // namespace ash
