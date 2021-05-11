// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/process_context.h"

#include <lib/sys/cpp/component_context.h>
#include <utility>

#include "base/fuchsia/process_context.h"
#include "base/no_destructor.h"

namespace base {

namespace {

std::unique_ptr<sys::ComponentContext>* ProcessComponentContextPtr() {
  static base::NoDestructor<std::unique_ptr<sys::ComponentContext>> value(
      std::make_unique<sys::ComponentContext>(
          sys::ServiceDirectory::CreateFromNamespace()));
  return value.get();
}

}  // namespace

sys::ComponentContext* ComponentContextForProcess() {
  return ProcessComponentContextPtr()->get();
}

std::unique_ptr<sys::ComponentContext> ReplaceComponentContextForProcessForTest(
    std::unique_ptr<sys::ComponentContext> context) {
  std::swap(*ProcessComponentContextPtr(), context);
  return context;
}

}  // namespace base
