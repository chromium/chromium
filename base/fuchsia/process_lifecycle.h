// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FUCHSIA_PROCESS_LIFECYCLE_H_
#define BASE_FUCHSIA_PROCESS_LIFECYCLE_H_

#include <fuchsia/process/lifecycle/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>

#include "base/base_export.h"
#include "base/functional/callback.h"

namespace base {

// Registers a fuchsia.process.lifecycle.Lifecycle protocol implementation to
// receive graceful termination requests from the Component Framework v2
// ELF executable runner.
//
// The implementation consumes the PA_LIFECYCLE handle, which the ELF runner
// will provide only if the Component manifest contains a lifecycle/stop_event
// registration.
class BASE_EXPORT ProcessLifecycle final
    : public fuchsia::process::lifecycle::Lifecycle {
 public:
  explicit ProcessLifecycle(base::OnceClosure on_stop);
  ~ProcessLifecycle() override;

  ProcessLifecycle(const ProcessLifecycle&) = delete;
  ProcessLifecycle& operator=(const ProcessLifecycle&) = delete;

  // fuchsia::process::lifecycle::Lifecycle implementation.
  void Stop() override;

 private:
  base::OnceClosure on_stop_;

  fidl::Binding<fuchsia::process::lifecycle::Lifecycle> binding_;
};

}  // namespace base

#endif  // BASE_FUCHSIA_PROCESS_LIFECYCLE_H_
