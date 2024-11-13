// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_SESSION_ARC_START_PARAMS_H_
#define ASH_COMPONENTS_ARC_SESSION_ARC_START_PARAMS_H_

#include <stdint.h>

namespace arc {

// Parameters to start request.
struct StartParams {
  enum class PlayStoreAutoUpdate {
    // Play Store auto-update is left unchanged.
    AUTO_UPDATE_DEFAULT = 0,
    // Play Store auto-update is forced to on.
    AUTO_UPDATE_ON,
    // Play Store auto-update is forced to off.
    AUTO_UPDATE_OFF,
  };

  enum class DalvikMemoryProfile {
    // Default dalvik memory profile suitable for all devices.
    DEFAULT = 0,
    // Dalvik memory profile suitable for 4G devices.
    M4G,
    // Dalvik memory profile suitable for 8G devices.
    M8G,
    // Dalvik memory profile suitable for 16G devices.
    M16G,
  };

  enum HostUreadaheadMode {
    // By default, ureadahead is in readahead mode.
    MODE_READAHEAD = 0,
    // Ureadahead is in generate mode.
    MODE_GENERATE = 1,
    // Ureadahead is in disabled mode.
    MODE_DISABLED = 2,
  };

  StartParams();

  StartParams(const StartParams&) = delete;
  StartParams& operator=(const StartParams&) = delete;

  StartParams(StartParams&& other);
  StartParams& operator=(StartParams&& other);

  ~StartParams();

  bool native_bridge_experiment = false;
  int lcd_density = -1;

  // Experiment flag for go/arc-file-picker.
  bool arc_file_picker_experiment = false;

  // Optional mode for play store auto-update.
  PlayStoreAutoUpdate play_store_auto_update =
      PlayStoreAutoUpdate::AUTO_UPDATE_DEFAULT;

  DalvikMemoryProfile dalvik_memory_profile = DalvikMemoryProfile::DEFAULT;

  HostUreadaheadMode host_ureadahead_mode = HostUreadaheadMode::MODE_READAHEAD;

  // Experiment flag for ARC Custom Tabs.
  bool arc_custom_tabs_experiment = false;

  // Flag to disable scheduling of media store periodic maintenance tasks.
  bool disable_media_store_maintenance = false;

  // Flag to disable Download provider in cache based tests in order to prevent
  // installing content, that is impossible to control in ARC and which causes
  // flakiness in tests.
  bool disable_download_provider = false;

  // Flag to indicate whether to use dev caches.
  bool use_dev_caches = false;

  // The number of logical CPU cores that are currently disabled on the host.
  uint32_t num_cores_disabled = 0;

  // Enables developer options used to generate Play Auto Install rosters.
  bool arc_generate_play_auto_install = false;

  // Flag to enable TTS caching.
  bool enable_tts_caching = false;

  // Flag to enable disable consumer auto update toggle as part of EU new deal.
  bool enable_consumer_auto_update_toggle = false;

  // Flag that indicates whether ARCVM uses virtio-blk for /data.
  bool use_virtio_blk_data = false;

  // Flag to enable Privacy Hub for chrome.
  bool enable_privacy_hub_for_chrome = false;

  // Flag to switch to KeyMint for T+.
  bool arc_switch_to_keymint = false;

  // Flag that indicates whether ARC is already signed in.
  bool arc_signed_in = false;

  // Flag that would enable ARC Attestation, and would make ARC
  // RemotelyProvisionedComponentDevice visible to Keystore.
  bool enable_arc_attestation = false;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_SESSION_ARC_START_PARAMS_H_
