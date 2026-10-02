// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/web_view_chooser_context.h"

#include <set>

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
  if (!chooser_context_->HasDevicePermission(embedding_origin, device)) {
    return false;
  }

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

std::vector<url::Origin> WebViewChooserContext::RevokeEphemeralPermissions(
    const ContentSettingsPattern& primary_pattern,
    bool unconditional) {
  auto should_revoke = [&](const url::Origin& origin) {
    return primary_pattern.Matches(origin.GetURL()) &&
           (unconditional ||
            !chooser_context_->CanRequestObjectPermission(origin));
  };

  std::set<url::Origin> revoked_guest_origins;
  std::erase_if(device_access_, [&](auto& embedder_entry) {
    auto& [embedding_origin, origins_per_device] = embedder_entry;
    if (should_revoke(embedding_origin)) {
      for (const auto& [device_guid, guest_origins] : origins_per_device) {
        revoked_guest_origins.insert(guest_origins.begin(),
                                     guest_origins.end());
      }
      return true;
    }
    std::erase_if(origins_per_device, [&](auto& device_entry) {
      std::erase_if(device_entry.second, [&](const url::Origin& guest_origin) {
        if (!should_revoke(guest_origin)) {
          return false;
        }
        revoked_guest_origins.insert(guest_origin);
        return true;
      });
      return device_entry.second.empty();
    });
    return origins_per_device.empty();
  });

  return std::vector<url::Origin>(revoked_guest_origins.begin(),
                                  revoked_guest_origins.end());
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
