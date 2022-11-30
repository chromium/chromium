// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_JANK_INJECTOR_H_
#define CC_METRICS_JANK_INJECTOR_H_

#include "base/task/single_thread_task_runner.h"
#include "cc/cc_export.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"

class GURL;

namespace cc {

class CC_EXPORT ScopedJankInjectionEnabler {
 public:
  ScopedJankInjectionEnabler();
  ~ScopedJankInjectionEnabler();

  ScopedJankInjectionEnabler(const ScopedJankInjectionEnabler&) = delete;
  ScopedJankInjectionEnabler& operator=(const ScopedJankInjectionEnabler&) =
      delete;
};

class CC_EXPORT JankInjector {
 public:
  struct CC_EXPORT JankConfig {
    uint32_t target_dropped_frames_percent = 10;

    // How many consecutive frames to drop (when a frame is dropped).
    uint32_t dropped_frame_cluster_size = 1;
  };

  // Jank injection.
  JankInjector();
  ~JankInjector();

  JankInjector(const JankInjector&) = delete;
  JankInjector& operator=(const JankInjector&) = delete;

  static bool IsEnabled(const GURL& url);

  void ScheduleJankIfNeeded(const viz::BeginFrameArgs& args,
                            base::SingleThreadTaskRunner* task_runner);

  const JankConfig& config() const { return config_; }

 private:
  bool ShouldJankCurrentFrame(const viz::BeginFrameArgs& args) const;
  void ScheduleJank(const viz::BeginFrameArgs& args,
                    base::SingleThreadTaskRunner* task_runner);
  void SignalJank();

  JankConfig config_;

  uint64_t total_frames_ = 0;
  uint64_t janked_frames_ = 0;
  bool did_jank_last_time_ = false;
};

}  // namespace cc

#endif  // CC_METRICS_JANK_INJECTOR_H_
