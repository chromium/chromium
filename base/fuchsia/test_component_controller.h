// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FUCHSIA_TEST_COMPONENT_CONTROLLER_H_
#define BASE_FUCHSIA_TEST_COMPONENT_CONTROLLER_H_

#include <fuchsia/sys/cpp/fidl.h>

#include "base/base_export.h"

namespace base {

// fuchsia.sys.ComponentController that requests the component to teardown
// gracefully, and waits for it to do so, when destroyed.
class BASE_EXPORT TestComponentController {
 public:
  TestComponentController();
  ~TestComponentController();

  TestComponentController(TestComponentController&&);
  TestComponentController& operator=(TestComponentController&&);

  explicit operator bool() const { return ptr_.is_bound(); }

  ::fuchsia::sys::ComponentControllerPtr& ptr() { return ptr_; }

  void KillAndRunUntilDisconnect();

 private:
  ::fuchsia::sys::ComponentControllerPtr ptr_;
};

}  // namespace base

#endif  // BASE_FUCHSIA_TEST_COMPONENT_CONTROLLER_H_
