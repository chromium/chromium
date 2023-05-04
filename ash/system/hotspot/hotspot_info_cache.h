// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOTSPOT_HOTSPOT_INFO_CACHE_H_
#define ASH_SYSTEM_HOTSPOT_HOTSPOT_INFO_CACHE_H_

#include "ash/ash_export.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class PrefRegistrySimple;

namespace ash {

// Queries the Hotspot info on startup, then monitors it for changes.
// Used by code that needs to query the hotspot state synchronously (e.g. the
// quick settings bubble).
class ASH_EXPORT HotspotInfoCache
    : public hotspot_config::mojom::CrosHotspotConfigObserver {
 public:
  HotspotInfoCache();
  HotspotInfoCache(const HotspotInfoCache&) = delete;
  HotspotInfoCache& operator=(const HotspotInfoCache&) = delete;
  ~HotspotInfoCache() override;

  // Registers the profile prefs for whether hotspot has been used.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Returns the cached hotspot info.
  hotspot_config::mojom::HotspotInfoPtr GetHotspotInfo();

  // Returns whether the hotspot has been successfully used previously.
  bool HasHotspotUsedBefore();

 private:
  // Binds to the mojo interface for hotspot config.
  void BindToCrosHotspotConfig();

  // hotspot_config::mojom::CrosHotspotConfigObserver:
  void OnHotspotInfoChanged() override;

  void OnGetHotspotInfo(hotspot_config::mojom::HotspotInfoPtr hotspot_info);
  void SetHasHotspotUsedBefore();

  mojo::Remote<hotspot_config::mojom::CrosHotspotConfig>
      remote_cros_hotspot_config_;
  mojo::Receiver<hotspot_config::mojom::CrosHotspotConfigObserver>
      hotspot_config_observer_receiver_{this};

  hotspot_config::mojom::HotspotInfoPtr hotspot_info_;

  base::WeakPtrFactory<HotspotInfoCache> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOTSPOT_HOTSPOT_INFO_CACHE_H_
