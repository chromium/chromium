// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/process_lifecycle.h"

#include <zircon/processargs.h>

#include "base/check.h"
#include "base/fuchsia/fuchsia_logging.h"

namespace base {

ProcessLifecycle::ProcessLifecycle(base::OnceClosure on_stop)
    : on_stop_(std::move(on_stop)), binding_(this) {
  // Sanity-check that an instance was not already created.
  static bool was_created = false;
  DCHECK(!was_created);
  was_created = true;

  // Under Components Framework v2 the ELF runner provides PA_LIFECYCLE.
  zx::channel lifecycle_request(zx_take_startup_handle(PA_LIFECYCLE));
  CHECK(lifecycle_request.is_valid());
  zx_status_t status = binding_.Bind(std::move(lifecycle_request));
  ZX_CHECK(status == ZX_OK, status) << "Bind Lifecycle";
}

ProcessLifecycle::~ProcessLifecycle() = default;

void ProcessLifecycle::Stop() {
  if (on_stop_) {
    std::move(on_stop_).Run();
  }
}

}  // namespace base
