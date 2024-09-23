// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/web_view_chooser_context.h"

#include "base/containers/map_util.h"
#include "base/feature_list.h"
#include "chrome/browser/hid/hid_chooser_context.h"
#include "extensions/common/extension_features.h"

WebViewChooserContext::WebViewChooserContext(HidChooserContext* chooser_context)
    : chooser_context_(chooser_context) {
  permission_observation_.Observe(chooser_context_);
}

WebViewChooserContext::~WebViewChooserContext() = default;

void WebViewChooserContext::GrantDevicePermission(
    const url::Origin& origin,
    const url::Origin& embedding_origin,
    const device::mojom::HidDeviceInfo& device) {
  CHECK(base::FeatureList::IsEnabled(
      extensions_features::kEnableWebHidInWebView));
  CHECK(chooser_context_->HasDevicePermission(embedding_origin, device));

  device_access_[embedding_origin][device.guid].insert(origin);
  chooser_context_->PermissionForWebViewChanged();
}

bool WebViewChooserContext::HasDevicePermission(
    const url::Origin& origin,
    const url::Origin& embedding_origin,
    const device::mojom::HidDeviceInfo& device) const {
  auto* origins_per_device = base::FindOrNull(device_access_, embedding_origin);
  if (!origins_per_device) {
    return false;
  }
  auto* origins = base::FindOrNull(*origins_per_device, device.guid);
  if (!origins) {
    return false;
  }
  return origins->contains(origin);
}

void WebViewChooserContext::RevokeDevicePermission(
    const url::Origin& origin,
    const url::Origin& embedding_origin,
    const device::mojom::HidDeviceInfo& device) {
  auto it = device_access_.find(embedding_origin);
  if (it == device_access_.end()) {
    return;
  }
  auto device_it = it->second.find(device.guid);
  if (device_it == it->second.end()) {
    return;
  }

  bool revoked_permission = device_it->second.erase(origin) > 0;

  if (device_it->second.empty()) {
    it->second.erase(device_it);
  }
  if (it->second.empty()) {
    device_access_.erase(it);
  }

  if (revoked_permission) {
    chooser_context_->PermissionForWebViewRevoked(origin);
  }
}

void WebViewChooserContext::OnPermissionRevoked(const url::Origin& origin) {
  auto* origins_per_device = base::FindOrNull(device_access_, origin);
  if (!origins_per_device) {
    return;
  }

  // If permission was revoked for the embedding origin, permission for web view
  // must be revoked as well.
  for (auto it = origins_per_device->begin();
       it != origins_per_device->end();) {
    auto* device = chooser_context_->GetDeviceInfo(it->first);
    if (!device || chooser_context_->HasDevicePermission(origin, *device)) {
      ++it;
      continue;
    }

    std::set<url::Origin> revoked_guest_origins;
    revoked_guest_origins.swap(it->second);
    for (const auto& guest_origin : revoked_guest_origins) {
      chooser_context_->PermissionForWebViewRevoked(guest_origin);
    }
    it = origins_per_device->erase(it);
  }
}

void WebViewChooserContext::OnHidChooserContextShutdown() {
  permission_observation_.Reset();
}
