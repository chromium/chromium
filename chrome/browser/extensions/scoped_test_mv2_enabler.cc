// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/scoped_test_mv2_enabler.h"

#include "chrome/browser/extensions/manifest_v2_experiment_manager.h"

namespace extensions {

ScopedTestMV2Enabler::ScopedTestMV2Enabler()
    : enable_mv2_extensions_(
          ManifestV2ExperimentManager::AllowMV2ExtensionsForTesting(
              PassKey())) {}

ScopedTestMV2Enabler::~ScopedTestMV2Enabler() = default;

}  // namespace extensions
