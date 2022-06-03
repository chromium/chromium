// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/installable/installed_webapp_geolocation_context.h"

#include <utility>

#include "chrome/browser/installable/installed_webapp_geolocation_bridge.h"

InstalledWebappGeolocationContext::InstalledWebappGeolocationContext() =
    default;

InstalledWebappGeolocationContext::~InstalledWebappGeolocationContext() =
    default;

void InstalledWebappGeolocationContext::BindGeolocation(
    mojo::PendingReceiver<device::mojom::Geolocation> receiver,
    const GURL& requesting_origin) {
  impls_.push_back(std::make_unique<InstalledWebappGeolocationBridge>(
      std::move(receiver), requesting_origin, this));
  if (geoposition_override_)
    impls_.back()->SetOverride(*geoposition_override_);
  else
    impls_.back()->StartListeningForUpdates();
}

void InstalledWebappGeolocationContext::OnConnectionError(
    InstalledWebappGeolocationBridge* impl) {
  for (auto it = impls_.begin(); it != impls_.end(); ++it) {
    if (impl == it->get()) {
      impls_.erase(it);
      return;
    }
  }
}

void InstalledWebappGeolocationContext::SetOverride(
    device::mojom::GeopositionPtr geoposition) {
  geoposition_override_ = std::move(geoposition);
  for (auto& impl : impls_) {
    impl->SetOverride(*geoposition_override_);
  }
}

void InstalledWebappGeolocationContext::ClearOverride() {
  geoposition_override_.reset();
  for (auto& impl : impls_) {
    impl->ClearOverride();
  }
}
