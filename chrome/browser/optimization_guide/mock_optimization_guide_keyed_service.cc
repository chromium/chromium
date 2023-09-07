// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"

MockOptimizationGuideKeyedService::MockOptimizationGuideKeyedService(
    content::BrowserContext* browser_context)
    : OptimizationGuideKeyedService(browser_context) {}

MockOptimizationGuideKeyedService::~MockOptimizationGuideKeyedService() =
    default;
