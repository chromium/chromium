// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_sharing_manager_impl.h"

#include "chrome/browser/glic/host/context/glic_tab_data.h"

namespace glic {

GlicSharingManagerImpl::GlicSharingManagerImpl(
    Profile* profile,
    GlicWindowController& window_controller,
    Host* host,
    GlicMetrics* metrics)
    : focused_tab_manager_(profile, window_controller, host, metrics) {}

GlicSharingManagerImpl::~GlicSharingManagerImpl() = default;

FocusedTabData GlicSharingManagerImpl::GetFocusedTabData() {
  return focused_tab_manager_.GetFocusedTabData();
}

base::CallbackListSubscription
GlicSharingManagerImpl::AddFocusedTabChangedCallback(
    FocusedTabChangedCallback callback) {
  return focused_tab_manager_.AddFocusedTabChangedCallback(std::move(callback));
}

base::CallbackListSubscription
GlicSharingManagerImpl::AddFocusedTabDataChangedCallback(
    FocusedTabDataChangedCallback callback) {
  return focused_tab_manager_.AddFocusedTabDataChangedCallback(
      std::move(callback));
}

void GlicSharingManagerImpl::GetContextFromFocusedTab(
    const mojom::GetTabContextOptions& options,
    base::OnceCallback<void(glic::mojom::GetContextResultPtr)> callback) {
  focused_tab_manager_.GetContextFromFocusedTab(options, std::move(callback));
}

}  // namespace glic
