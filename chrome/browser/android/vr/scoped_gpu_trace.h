// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SCOPED_GPU_TRACE_H_
#define CHROME_BROWSER_ANDROID_VR_SCOPED_GPU_TRACE_H_

#include <memory>

#include "base/time/time.h"

namespace gl {
class GLFenceAndroidNativeFenceSync;
}

namespace vr {

// Helper class to trace GPU work.
// It creates a fence in the constructor and extracts the time when the fence
// completed in the destructor. The duration is reported to trace in the "gpu"
// category. It assumes that fence has completed when destructor is triggered.
// If for some reason it failed to extract fence completion time, no trace event
// will be recorded. NB: This class is not thread safe due to static id
// generation. If you need to use it on different threads, consider a thread
// safe id generator.
class ScopedGpuTrace {
 public:
  explicit ScopedGpuTrace(const char* name);

  ScopedGpuTrace(const ScopedGpuTrace&) = delete;
  ScopedGpuTrace& operator=(const ScopedGpuTrace&) = delete;

  virtual ~ScopedGpuTrace();

  gl::GLFenceAndroidNativeFenceSync* fence() { return fence_.get(); }

 private:
  // Not thread safe.
  static uint32_t s_trace_id_;
  base::TimeTicks start_time_;
  std::unique_ptr<gl::GLFenceAndroidNativeFenceSync> fence_;
  const char* const name_;
  uint32_t trace_id_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_SCOPED_GPU_TRACE_H_
