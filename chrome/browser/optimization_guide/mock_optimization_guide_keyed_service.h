// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_MOCK_OPTIMIZATION_GUIDE_KEYED_SERVICE_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_MOCK_OPTIMIZATION_GUIDE_KEYED_SERVICE_H_

#include "base/test/gmock_callback_support.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockOptimizationGuideKeyedService : public OptimizationGuideKeyedService {
 public:
  explicit MockOptimizationGuideKeyedService(
      content::BrowserContext* browser_context);
  ~MockOptimizationGuideKeyedService() override;

  MOCK_METHOD(void,
              RegisterOptimizationTypes,
              (const std::vector<optimization_guide::proto::OptimizationType>&),
              (override));
  MOCK_METHOD(void,
              CanApplyOptimization,
              (const GURL&,
               optimization_guide::proto::OptimizationType,
               optimization_guide::OptimizationGuideDecisionCallback),
              (override));
};

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_MOCK_OPTIMIZATION_GUIDE_KEYED_SERVICE_H_
