// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/default_context.h"

#include <lib/sys/cpp/component_context.h>

#include "base/fuchsia/file_utils.h"
#include "base/logging.h"
#include "base/no_destructor.h"

namespace base {
namespace fuchsia {

sys::ComponentContext* ComponentContextForCurrentProcess() {
  static base::NoDestructor<std::unique_ptr<sys::ComponentContext>> value(
      std::make_unique<sys::ComponentContext>(
          sys::ServiceDirectory::CreateFromNamespace()));
  return value.get()->get();
}

}  // namespace fuchsia
}  // namespace base
