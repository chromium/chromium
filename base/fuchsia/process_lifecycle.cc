// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/process_lifecycle.h"

#include <lib/async/default.h>
#include <zircon/process.h>
#include <zircon/processargs.h>

#include "base/check.h"
#include "base/fuchsia/fuchsia_logging.h"

namespace base {

ProcessLifecycle::ProcessLifecycle(base::OnceClosure on_stop)
    : on_stop_(std::move(on_stop)) {
  // Sanity-check that an instance was not already created.
  static bool was_created = false;
  DCHECK(!was_created);
  was_created = true;

  // Under Components Framework v2 the ELF runner provides PA_LIFECYCLE.
  zx::channel lifecycle_server_channel(zx_take_startup_handle(PA_LIFECYCLE));
  CHECK(lifecycle_server_channel.is_valid());
  fidl::ServerEnd<fuchsia_process_lifecycle::Lifecycle> lifecycle_server_end(
      std::move(lifecycle_server_channel));
  binding_.emplace(async_get_default_dispatcher(),
                   std::move(lifecycle_server_end), this,
                   fidl::kIgnoreBindingClosure);
}

ProcessLifecycle::~ProcessLifecycle() = default;

void ProcessLifecycle::Stop(ProcessLifecycle::StopCompleter::Sync& completer) {
  if (on_stop_) {
    std::move(on_stop_).Run();
  }
}

}  // namespace base
