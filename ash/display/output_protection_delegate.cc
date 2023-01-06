// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/output_protection_delegate.h"

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/shell.h"
#include "base/functional/callback_helpers.h"
#include "ui/display/display.h"
#include "ui/display/manager/content_protection_manager.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"

namespace ash {

namespace {

display::ContentProtectionManager* manager() {
  return Shell::Get()->display_configurator()->content_protection_manager();
}

void MaybeSetCaptureModeWindowProtection(aura::Window* window,
                                         uint32_t protection_mask) {
  // `OutputProtectionDelegate` is not owned by ash. It is created by
  // `OutputProtectionImpl` which exists in Chrome, and can invoke the delegate
  // even after `Shell` has been destroyed. See b/256706119.
  if (!Shell::HasInstance())
    return;

  CaptureModeController::Get()->SetWindowProtectionMask(window,
                                                        protection_mask);
}

}  // namespace

struct OutputProtectionDelegate::ClientIdHolder {
  ClientIdHolder() : id(manager()->RegisterClient()) {}
  ~ClientIdHolder() { manager()->UnregisterClient(id); }

  const display::ContentProtectionManager::ClientId id;
};

OutputProtectionDelegate::OutputProtectionDelegate(aura::Window* window)
    : window_(window),
      display_id_(
          display::Screen::GetScreen()->GetDisplayNearestWindow(window).id()) {
  // TODO(domlaskowski): OutputProtectionImpl passes null if the RenderFrameHost
  // no longer exists. Investigate removing this check in crbug.com/997270.
  if (!window_)
    return;

  window_->AddObserver(this);
}

OutputProtectionDelegate::~OutputProtectionDelegate() {
  if (!window_)
    return;

  window_->RemoveObserver(this);
  MaybeSetCaptureModeWindowProtection(window_,
                                      display::CONTENT_PROTECTION_METHOD_NONE);
}

void OutputProtectionDelegate::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  // Switching the primary display (either by user or by going into docked
  // mode), as well as changing mirror mode may change the display on which
  // the window resides without actually changing the window hierarchy (i.e.
  // the root window is still the same). Hence we need to watch out for these
  // situations and update |display_id_| if needed.
  if (!(changed_metrics &
        (display::DisplayObserver::DISPLAY_METRIC_PRIMARY |
         display::DisplayObserver::DISPLAY_METRIC_MIRROR_STATE))) {
    return;
  }

  OnWindowMayHaveMovedToAnotherDisplayOrWindow();
}

void OutputProtectionDelegate::OnWindowHierarchyChanged(
    const aura::WindowObserver::HierarchyChangeParams& params) {
  OnWindowMayHaveMovedToAnotherDisplayOrWindow();
}

void OutputProtectionDelegate::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(window, window_);
  display_observer_.reset();
  window_->RemoveObserver(this);
  MaybeSetCaptureModeWindowProtection(window_,
                                      display::CONTENT_PROTECTION_METHOD_NONE);
  window_ = nullptr;
}

void OutputProtectionDelegate::QueryStatus(QueryStatusCallback callback) {
  if (!RegisterClientIfNecessary()) {
    std::move(callback).Run(/*success=*/false,
                            display::DISPLAY_CONNECTION_TYPE_NONE,
                            display::CONTENT_PROTECTION_METHOD_NONE);
    return;
  }

  manager()->QueryContentProtection(client_->id, display_id_,
                                    std::move(callback));
}

void OutputProtectionDelegate::SetProtection(uint32_t protection_mask,
                                             SetProtectionCallback callback) {
  protection_mask_ = protection_mask;

  // Capture mode screen recording doesn't rely on display protection, and hence
  // must be informed with the new window's protection.
  if (window_)
    MaybeSetCaptureModeWindowProtection(window_, protection_mask);

  if (!RegisterClientIfNecessary()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  manager()->ApplyContentProtection(client_->id, display_id_, protection_mask,
                                    std::move(callback));
}

void OutputProtectionDelegate::OnWindowMayHaveMovedToAnotherDisplayOrWindow() {
  DCHECK(window_);

  // The window may have moved to a display that is currently being recorded, or
  // to be hosted by a browser window that is being recorded when a tab becomes
  // active, so we need to refresh Capture Mode's content protection.
  CaptureModeController::Get()->RefreshContentProtection();

  const int64_t new_display_id =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window_).id();
  if (display_id_ == new_display_id)
    return;

  if (protection_mask_ != display::CONTENT_PROTECTION_METHOD_NONE) {
    DCHECK(client_);
    manager()->ApplyContentProtection(client_->id, new_display_id,
                                      protection_mask_, base::DoNothing());
    manager()->ApplyContentProtection(client_->id, display_id_,
                                      display::CONTENT_PROTECTION_METHOD_NONE,
                                      base::DoNothing());
  }

  display_id_ = new_display_id;
}

bool OutputProtectionDelegate::RegisterClientIfNecessary() {
  if (!window_)
    return false;

  if (!client_)
    client_ = std::make_unique<ClientIdHolder>();

  return true;
}

}  // namespace ash
