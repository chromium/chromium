// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/test_crosapi_dependency_registry.h"

#include <memory>

#include "chrome/browser/ash/crosapi/crosapi_manager.h"

namespace crosapi {

std::unique_ptr<CrosapiManager> CreateCrosapiManagerWithTestRegistry() {
  TestCrosapiDependencyRegistry test_registry;
  return std::make_unique<CrosapiManager>(&test_registry);
}

TestCrosapiDependencyRegistry::TestCrosapiDependencyRegistry() = default;

TestCrosapiDependencyRegistry::~TestCrosapiDependencyRegistry() = default;

}  // namespace crosapi
