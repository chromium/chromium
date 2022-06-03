// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/test_component_controller.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/run_loop.h"

namespace base {

TestComponentController::TestComponentController() = default;

TestComponentController::TestComponentController(TestComponentController&&) =
    default;

TestComponentController& TestComponentController::operator=(
    TestComponentController&&) = default;

TestComponentController::~TestComponentController() {
  KillAndRunUntilDisconnect();
}

void TestComponentController::KillAndRunUntilDisconnect() {
  if (!ptr_)
    return;

  base::RunLoop loop;
  ptr_.set_error_handler([&loop](zx_status_t status) {
    loop.Quit();
    ZX_CHECK(status == ZX_ERR_PEER_CLOSED, status);
  });
  ptr_->Kill();
  loop.Run();

  CHECK(!ptr_);
}

}  // namespace base
