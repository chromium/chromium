// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FUCHSIA_PROCESS_LIFECYCLE_H_
#define BASE_FUCHSIA_PROCESS_LIFECYCLE_H_

#include <fuchsia/process/lifecycle/cpp/fidl.h>
#include <lib/fidl/cpp/binding_set.h>

#include "base/base_export.h"
#include "base/callback.h"

namespace base {

// Publishes a fuchsia.process.lifecycle.Lifecycle protocol implementation to
// receive graceful termination requests from the ELF runner.
//
// The implementation supports both Components Framework v1 and v2.
// Under CFv2 the PA_LIFECYCLE handle will be bound to the implementation.
// The service will be published to the process' outgoing directory to suppport
// CFv1.
//
// Note that the calling component must opt-in to receiving Lifecycle events.
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

  fidl::BindingSet<fuchsia::process::lifecycle::Lifecycle> bindings_;
};

}  // namespace base

#endif  // BASE_FUCHSIA_PROCESS_LIFECYCLE_H_
