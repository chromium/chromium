// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTROLLED_FRAME_CONTROLLED_FRAME_MEDIA_PERMISSION_CACHE_H_
#define CHROME_BROWSER_CONTROLLED_FRAME_CONTROLLED_FRAME_MEDIA_PERMISSION_CACHE_H_

#include <map>
#include <set>

#include "base/scoped_observation.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/origin.h"

class HostContentSettingsMap;

class Profile;

namespace controlled_frame {

// Caches Controlled Frame media permission requests per profile.
// The cache is cleared when the profile is destroyed or when camera/microphone
// content settings are changed or cleared (e.g. by Clear Browsing Data).
class ControlledFrameMediaPermissionCache : public KeyedService,
                                            public content_settings::Observer {
 public:
  explicit ControlledFrameMediaPermissionCache(Profile* profile);
  ~ControlledFrameMediaPermissionCache() override;

  ControlledFrameMediaPermissionCache(
      const ControlledFrameMediaPermissionCache&) = delete;
  ControlledFrameMediaPermissionCache& operator=(
      const ControlledFrameMediaPermissionCache&) = delete;

  // KeyedService:
  void Shutdown() override;

  // content_settings::Observer:
  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsTypeSet content_type_set) override;

  // Permission cache operations.
  void AddPermission(const url::Origin& embedder_origin,
                     const url::Origin& requesting_origin);
  bool HasPermission(const url::Origin& embedder_origin,
                     const url::Origin& requesting_origin) const;

 private:
  std::map<url::Origin, std::set<url::Origin>> requests_;
  base::ScopedObservation<HostContentSettingsMap, content_settings::Observer>
      hcsm_observation_{this};
};

}  // namespace controlled_frame

#endif  // CHROME_BROWSER_CONTROLLED_FRAME_CONTROLLED_FRAME_MEDIA_PERMISSION_CACHE_H_
