// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/output_protection_delegate.h"

#include "ash/shell.h"
#include "base/bind_helpers.h"
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
  display::Screen::GetScreen()->AddObserver(this);
}

OutputProtectionDelegate::~OutputProtectionDelegate() {
  if (!window_)
    return;

  display::Screen::GetScreen()->RemoveObserver(this);
  window_->RemoveObserver(this);
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

  OnWindowMayHaveMovedToAnotherDisplay();
}

void OutputProtectionDelegate::OnWindowHierarchyChanged(
    const aura::WindowObserver::HierarchyChangeParams& params) {
  OnWindowMayHaveMovedToAnotherDisplay();
}

void OutputProtectionDelegate::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(window, window_);
  display::Screen::GetScreen()->RemoveObserver(this);
  window_->RemoveObserver(this);
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
  if (!RegisterClientIfNecessary()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  manager()->ApplyContentProtection(client_->id, display_id_, protection_mask,
                                    std::move(callback));
  protection_mask_ = protection_mask;
}

void OutputProtectionDelegate::OnWindowMayHaveMovedToAnotherDisplay() {
  DCHECK(window_);
  int64_t new_display_id =
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
