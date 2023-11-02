// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OOM_INTERVENTION_OOM_INTERVENTION_CONFIG_H_
#define CHROME_BROWSER_ANDROID_OOM_INTERVENTION_OOM_INTERVENTION_CONFIG_H_

#include "third_party/blink/public/mojom/oom_intervention/oom_intervention.mojom.h"

// Holds the configurations provided by field trials for OOM intervention.
class OomInterventionConfig {
 public:
  static const OomInterventionConfig* GetInstance();

  // True when field trials enables intervention and config is valid.
  bool is_intervention_enabled() const { return is_intervention_enabled_; }

  // True when browser swap monitor is enabled.
  bool is_swap_monitor_enabled() const { return is_swap_monitor_enabled_; }

  // True if Android memory pressure signals should be monitored.
  bool use_components_callback() const { return use_components_callback_; }

  // True if on detection of near OOM condition the renderer JS should be
  // paused.
  bool is_renderer_pause_enabled() const { return is_renderer_pause_enabled_; }

  // True if on detection of near OOM condition the ad iframes should be
  // navigated.
  bool is_navigate_ads_enabled() const { return is_navigate_ads_enabled_; }

  // True if on detection of near OOM condition V8 memory should be purged.
  bool is_purge_v8_memory_enabled() const {
    return is_purge_v8_memory_enabled_;
  }

  // True if detection should be enabled on renderers.
  bool should_detect_in_renderer() const { return should_detect_in_renderer_; }

  // The threshold for swap size in the system to start monitoring.
  uint64_t swapfree_threshold() const { return swapfree_threshold_; }

  // The arguments for detecting near OOM situation in renderer.
  blink::mojom::DetectionArgsPtr GetRendererOomDetectionArgs() const;

 private:
  OomInterventionConfig();
  ~OomInterventionConfig();

  bool is_intervention_enabled_ = false;

  bool is_swap_monitor_enabled_ = false;
  bool use_components_callback_ = false;

  bool is_renderer_pause_enabled_ = false;
  bool is_navigate_ads_enabled_ = false;
  bool is_purge_v8_memory_enabled_ = false;
  bool should_detect_in_renderer_ = false;

  uint64_t swapfree_threshold_ = 0;

  blink::mojom::DetectionArgsPtr renderer_detection_args_;
};

#endif  // CHROME_BROWSER_ANDROID_OOM_INTERVENTION_OOM_INTERVENTION_CONFIG_H_
