// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/session/arc_client_adapter.h"

#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/arc_container_client_adapter.h"
#include "ash/components/arc/session/arc_start_params.h"
#include "ash/components/arc/session/arc_vm_client_adapter.h"
#include "chromeos/ash/components/dbus/arc/arc.pb.h"

namespace arc {

namespace {

// Converts PlayStoreAutoUpdate into ArcMiniInstanceRequest's.
StartArcMiniInstanceRequest_PlayStoreAutoUpdate
ToArcMiniInstanceRequestPlayStoreAutoUpdate(
    StartParams::PlayStoreAutoUpdate update) {
  switch (update) {
    case StartParams::PlayStoreAutoUpdate::AUTO_UPDATE_DEFAULT:
      return StartArcMiniInstanceRequest_PlayStoreAutoUpdate_AUTO_UPDATE_DEFAULT;
    case StartParams::PlayStoreAutoUpdate::AUTO_UPDATE_ON:
      return StartArcMiniInstanceRequest_PlayStoreAutoUpdate_AUTO_UPDATE_ON;
    case StartParams::PlayStoreAutoUpdate::AUTO_UPDATE_OFF:
      return StartArcMiniInstanceRequest_PlayStoreAutoUpdate_AUTO_UPDATE_OFF;
  }
}

// Converts DalvikMemoryProfile into ArcMiniInstanceRequest's.
StartArcMiniInstanceRequest_DalvikMemoryProfile
ToArcMiniInstanceRequestDalvikMemoryProfile(
    StartParams::DalvikMemoryProfile dalvik_memory_profile) {
  switch (dalvik_memory_profile) {
    case StartParams::DalvikMemoryProfile::DEFAULT:
      return StartArcMiniInstanceRequest_DalvikMemoryProfile_MEMORY_PROFILE_DEFAULT;
    case StartParams::DalvikMemoryProfile::M4G:
      return StartArcMiniInstanceRequest_DalvikMemoryProfile_MEMORY_PROFILE_4G;
    case StartParams::DalvikMemoryProfile::M8G:
      return StartArcMiniInstanceRequest_DalvikMemoryProfile_MEMORY_PROFILE_8G;
    case StartParams::DalvikMemoryProfile::M16G:
      return StartArcMiniInstanceRequest_DalvikMemoryProfile_MEMORY_PROFILE_16G;
  }
}

StartArcMiniInstanceRequest_HostUreadaheadMode
ToArcMiniInstanceRequestHostUreadaheadMode(
    StartParams::HostUreadaheadMode host_ureadahead_mode) {
  switch (host_ureadahead_mode) {
    case StartParams::HostUreadaheadMode::MODE_READAHEAD:
      return StartArcMiniInstanceRequest_HostUreadaheadMode_MODE_DEFAULT;
    case StartParams::HostUreadaheadMode::MODE_DISABLED:
      return StartArcMiniInstanceRequest_HostUreadaheadMode_MODE_DISABLED;
    case StartParams::HostUreadaheadMode::MODE_GENERATE:
      return StartArcMiniInstanceRequest_HostUreadaheadMode_MODE_GENERATE;
  }
}

}  // namespace

ArcClientAdapter::ArcClientAdapter() = default;
ArcClientAdapter::~ArcClientAdapter() = default;

void ArcClientAdapter::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ArcClientAdapter::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

// static
std::unique_ptr<ArcClientAdapter> ArcClientAdapter::Create() {
  return IsArcVmEnabled() ? CreateArcVmClientAdapter()
                          : CreateArcContainerClientAdapter();
}

StartArcMiniInstanceRequest
ArcClientAdapter::ConvertStartParamsToStartArcMiniInstanceRequest(
    const StartParams& params) {
  StartArcMiniInstanceRequest request;
  request.set_native_bridge_experiment(params.native_bridge_experiment);
  request.set_lcd_density(params.lcd_density);
  request.set_arc_file_picker_experiment(params.arc_file_picker_experiment);
  request.set_play_store_auto_update(
      ToArcMiniInstanceRequestPlayStoreAutoUpdate(
          params.play_store_auto_update));
  request.set_dalvik_memory_profile(ToArcMiniInstanceRequestDalvikMemoryProfile(
      params.dalvik_memory_profile));
  request.set_arc_custom_tabs_experiment(params.arc_custom_tabs_experiment);
  request.set_disable_media_store_maintenance(
      params.disable_media_store_maintenance);
  request.set_disable_download_provider(params.disable_download_provider);
  request.set_host_ureadahead_mode(
      ToArcMiniInstanceRequestHostUreadaheadMode(params.host_ureadahead_mode));
  request.set_use_dev_caches(params.use_dev_caches);
  request.set_arc_signed_in(params.arc_signed_in);
  request.set_arc_generate_pai(params.arc_generate_play_auto_install);
  request.set_enable_consumer_auto_update_toggle(
      params.enable_consumer_auto_update_toggle);
  request.set_enable_tts_caching(params.enable_tts_caching);
  request.set_enable_privacy_hub_for_chrome(
      params.enable_privacy_hub_for_chrome);
  return request;
}

}  // namespace arc
