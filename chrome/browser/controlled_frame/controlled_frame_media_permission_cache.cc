// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/controlled_frame/controlled_frame_media_permission_cache.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"

namespace controlled_frame {

ControlledFrameMediaPermissionCache::ControlledFrameMediaPermissionCache(
    Profile* profile) {
  auto* hcsm = HostContentSettingsMapFactory::GetForProfile(profile);
  if (hcsm) {
    hcsm_observation_.Observe(hcsm);
  }
}

ControlledFrameMediaPermissionCache::~ControlledFrameMediaPermissionCache() =
    default;

void ControlledFrameMediaPermissionCache::Shutdown() {
  hcsm_observation_.Reset();
}

void ControlledFrameMediaPermissionCache::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  // We only cache Camera and Microphone permissions. Ignore all other content
  // setting changes (like Cookies or Geolocation) to prevent spurious cache
  // invalidations that would force the user to re-grant permissions.
  if (content_type_set.Contains(ContentSettingsType::MEDIASTREAM_CAMERA) ||
      content_type_set.Contains(ContentSettingsType::MEDIASTREAM_MIC)) {
    requests_.clear();
  }
}

void ControlledFrameMediaPermissionCache::AddPermission(
    const url::Origin& embedder_origin,
    const url::Origin& requesting_origin) {
  requests_[embedder_origin].insert(requesting_origin);
}

bool ControlledFrameMediaPermissionCache::HasPermission(
    const url::Origin& embedder_origin,
    const url::Origin& requesting_origin) const {
  auto it = requests_.find(embedder_origin);
  if (it == requests_.end()) {
    return false;
  }
  return it->second.contains(requesting_origin);
}

}  // namespace controlled_frame
