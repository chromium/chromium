// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/hotspot/hotspot_info_cache.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/hotspot_config_service.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {

using hotspot_config::mojom::HotspotInfoPtr;
using hotspot_config::mojom::HotspotState;

HotspotInfoCache::HotspotInfoCache() {
  // Asynchronously bind to CrosHotspotConfig so that we don't attempt to bind
  // to it before it has initialized.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&HotspotInfoCache::BindToCrosHotspotConfig,
                                weak_ptr_factory_.GetWeakPtr()));
}

HotspotInfoCache::~HotspotInfoCache() = default;

// static
void HotspotInfoCache::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  // These prefs stores whether the hotspot has been used before.
  registry->RegisterBooleanPref(prefs::kHasHotspotUsedBefore,
                                /*default_value=*/false);
}

HotspotInfoPtr HotspotInfoCache::GetHotspotInfo() {
  return mojo::Clone(hotspot_info_);
}

bool HotspotInfoCache::HasHotspotUsedBefore() {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  if (!prefs) {
    // Return false because we don't want to show the hotspot if we're
    // unsure if it can be shown or not.
    return false;
  }
  return prefs->GetBoolean(prefs::kHasHotspotUsedBefore);
}

void HotspotInfoCache::SetHasHotspotUsedBefore() {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  if (!prefs) {
    return;
  }
  prefs->SetBoolean(prefs::kHasHotspotUsedBefore, true);
}

void HotspotInfoCache::BindToCrosHotspotConfig() {
  GetHotspotConfigService(
      remote_cros_hotspot_config_.BindNewPipeAndPassReceiver());
  // Observing CrosHotspotConfig and also does a fetch of the initial
  // properties.
  remote_cros_hotspot_config_->AddObserver(
      hotspot_config_observer_receiver_.BindNewPipeAndPassRemote());
}

void HotspotInfoCache::OnHotspotInfoChanged() {
  remote_cros_hotspot_config_->GetHotspotInfo(base::BindOnce(
      &HotspotInfoCache::OnGetHotspotInfo, weak_ptr_factory_.GetWeakPtr()));
}

void HotspotInfoCache::OnGetHotspotInfo(HotspotInfoPtr hotspot_info) {
  hotspot_info_ = std::move(hotspot_info);

  if (hotspot_info_->state == HotspotState::kEnabled &&
      !HasHotspotUsedBefore()) {
    SetHasHotspotUsedBefore();
  }
}

}  // namespace ash
