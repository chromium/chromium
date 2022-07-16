// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/process_lifecycle.h"

#include <lib/sys/cpp/component_context.h>
#include <lib/sys/cpp/outgoing_directory.h>
#include <zircon/processargs.h>

#include "base/check.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"

namespace base {

ProcessLifecycle::ProcessLifecycle(base::OnceClosure on_stop)
    : on_stop_(std::move(on_stop)) {
  // Sanity-check that an instance was not already created.
  static bool was_created = false;
  DCHECK(!was_created);
  was_created = true;

  // Under Components Framework v2 the ELF runner provides PA_LIFECYCLE.
  zx::channel lifecycle_request(zx_take_startup_handle(PA_LIFECYCLE));
  if (lifecycle_request.is_valid()) {
    bindings_.AddBinding(
        this, fidl::InterfaceRequest<fuchsia::process::lifecycle::Lifecycle>(
                  std::move(lifecycle_request)));
  } else {
    // TODO(crbug.com/1250747): Remove this fallback once everything uses CFv2.
    // Under Component Framework v1 there is no PA_LIFECYCLE handle provided by
    // the runner, and the Component must instead publish the Lifecycle service.
    LOG(WARNING) << "Publishing Lifecycle service for CFv1.";
    zx_status_t status =
        base::ComponentContextForProcess()->outgoing()->AddPublicService(
            bindings_.GetHandler(this), "fuchsia.process.lifecycle.Lifecycle");
    ZX_CHECK(status == ZX_OK, status);
  }
}

ProcessLifecycle::~ProcessLifecycle() = default;

void ProcessLifecycle::Stop() {
  if (on_stop_)
    std::move(on_stop_).Run();
}

}  // namespace base
