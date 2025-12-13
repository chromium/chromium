// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"

#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"

MockOptimizationGuideKeyedService::MockOptimizationGuideKeyedService()
    : OptimizationGuideKeyedService(nullptr) {}

MockOptimizationGuideKeyedService::~MockOptimizationGuideKeyedService() =
    default;

void MockOptimizationGuideKeyedService::Shutdown() {}
