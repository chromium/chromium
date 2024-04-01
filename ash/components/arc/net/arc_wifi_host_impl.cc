// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/net/arc_wifi_host_impl.h"

#include "ash/components/arc/session/arc_bridge_service.h"

namespace arc {

// static
ArcWifiHostImpl* ArcWifiHostImpl::GetForBrowserContext(
    content::BrowserContext* context) {
  // TODO(b/329552433): Add implementation.
  return nullptr;
}

// static
ArcWifiHostImpl* ArcWifiHostImpl::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  // TODO(b/329552433): Add implementation.
  return nullptr;
}

ArcWifiHostImpl::ArcWifiHostImpl(content::BrowserContext* context,
                                 ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  // TODO(b/329552433): Add implementation.
}

ArcWifiHostImpl::~ArcWifiHostImpl() {
  // TODO(b/329552433): Add implementation.
}

// static
void ArcWifiHostImpl::EnsureFactoryBuilt() {
  // TODO(b/329552433): Add implementation.
}

void ArcWifiHostImpl::GetWifiEnabledState(
    GetWifiEnabledStateCallback callback) {
  // TODO(b/329552433): Add implementation.
}

void ArcWifiHostImpl::SetWifiEnabledState(
    bool is_enabled,
    SetWifiEnabledStateCallback callback) {
  // TODO(b/329552433): Add implementation.
}

}  // namespace arc
