// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_SCROLL_JANK_UKM_REPORTER_H_
#define CC_METRICS_SCROLL_JANK_UKM_REPORTER_H_

#include <optional>
#include "base/memory/raw_ptr.h"
#include "cc/cc_export.h"

// TODO(b/294040250): Add metrics for ScrollPredictor histogram.

namespace cc {
class UkmManager;

class CC_EXPORT ScrollJankUkmReporter {
 public:
  ScrollJankUkmReporter();
  ~ScrollJankUkmReporter();

  ScrollJankUkmReporter(const ScrollJankUkmReporter&) = delete;

  void IncrementFrameCount();
  void IncrementDelayedFrameCount();
  void AddVsyncs(int vsyncs);
  void AddMissedVsyncs(int missed_vsyncs);
  void IncrementPredictorJankyFrames();

  void EmitScrollJankUkm();

  void set_max_missed_vsyncs(int max_missed_vsyncs) {
    max_missed_vsyncs_ = max_missed_vsyncs;
  }

  void set_ukm_manager(UkmManager* manager) { ukm_manager_ = manager; }

 private:
  int num_frames_ = 0;
  int num_vsyncs_ = 0;
  int num_missed_vsyncs_ = 0;
  int max_missed_vsyncs_ = 0;
  int num_delayed_frames_ = 0;
  int predictor_jank_frames_ = 0;

  // This is pointing to the LayerTreeHostImpl::ukm_manager_, which is
  // initialized right after the LayerTreeHostImpl is created. So when this
  // pointer is initialized, there should be no trackers yet. Moreover, the
  // LayerTreeHostImpl::ukm_manager_ lives as long as the LayerTreeHostImpl, so
  // this pointer should never be null as long as LayerTreeHostImpl is alive.
  raw_ptr<UkmManager> ukm_manager_ = nullptr;
};

}  // namespace cc

#endif  // CC_METRICS_SCROLL_JANK_UKM_REPORTER_H_
