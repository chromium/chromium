// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_GLOBAL_STATE_HOLDER_KEYED_SERVICE_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_GLOBAL_STATE_HOLDER_KEYED_SERVICE_H_

#include "base/memory/scoped_refptr.h"
#include "components/keyed_service/core/keyed_service.h"

namespace optimization_guide {
class OptimizationGuideGlobalState;
}  // namespace optimization_guide

// A light-weight keyed service whose sole purpose is to hold onto a
// `OptimizationGuideGlobalState` reference so consumers of the global state
// can declare their dependency on this service to manage lifetime for
// `OptimizationGuideGlobalState`.
class OptimizationGuideGlobalStateHolderKeyedService : public KeyedService {
 public:
  OptimizationGuideGlobalStateHolderKeyedService();

  OptimizationGuideGlobalStateHolderKeyedService(
      const OptimizationGuideGlobalStateHolderKeyedService&) = delete;
  OptimizationGuideGlobalStateHolderKeyedService& operator=(
      const OptimizationGuideGlobalStateHolderKeyedService&) = delete;

  ~OptimizationGuideGlobalStateHolderKeyedService() override;

  optimization_guide::OptimizationGuideGlobalState& GetGlobalState();

 private:
  scoped_refptr<optimization_guide::OptimizationGuideGlobalState>
      optimization_guide_global_state_;
};

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_GLOBAL_STATE_HOLDER_KEYED_SERVICE_H_
