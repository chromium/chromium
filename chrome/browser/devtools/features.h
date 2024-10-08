// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_FEATURES_H_
#define CHROME_BROWSER_DEVTOOLS_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace features {

BASE_DECLARE_FEATURE(kDevToolsConsoleInsights);
extern const base::FeatureParam<std::string> kDevToolsConsoleInsightsModelId;
extern const base::FeatureParam<double> kDevToolsConsoleInsightsTemperature;
extern const base::FeatureParam<bool> kDevToolsConsoleInsightsOptIn;

BASE_DECLARE_FEATURE(kDevToolsFreestylerDogfood);
extern const base::FeatureParam<std::string> kDevToolsFreestylerDogfoodModelId;
extern const base::FeatureParam<double> kDevToolsFreestylerDogfoodTemperature;

enum class DevToolsFreestylerUserTier {
  // Users who are internal testers or validators.
  // AIDA does not log these users in product usage metrics.
  // In future, the data from these users will be excluded from training data
  // when logging is enabled.
  kTesters,
  // Users who are early adopters.
  kBeta,
  // Users in the general public.
  kPublic
};

enum class DevToolsFreestylerExecutionMode {
  // Allows running all scripts.
  kAllScripts,
  // Only allow running side-effect free scripts.
  kSideEffectFreeScriptsOnly,
  // Disallow all scripts.
  kNoScripts
};

extern const base::FeatureParam<DevToolsFreestylerUserTier>
    kDevToolsFreestylerDogfoodUserTier;

BASE_DECLARE_FEATURE(kDevToolsFreestyler);
extern const base::FeatureParam<std::string> kDevToolsFreestylerModelId;
extern const base::FeatureParam<double> kDevToolsFreestylerTemperature;
extern const base::FeatureParam<DevToolsFreestylerUserTier>
    kDevToolsFreestylerUserTier;
extern const base::FeatureParam<DevToolsFreestylerExecutionMode>
    kDevToolsFreestylerExecutionMode;

BASE_DECLARE_FEATURE(kDevToolsExplainThisResourceDogfood);
extern const base::FeatureParam<std::string>
    kDevToolsExplainThisResourceDogfoodModelId;
extern const base::FeatureParam<double>
    kDevToolsExplainThisResourceDogfoodTemperature;
extern const base::FeatureParam<DevToolsFreestylerUserTier>
    kDevToolsExplainThisResourceDogfoodUserTier;

BASE_DECLARE_FEATURE(kDevToolsAiAssistancePerformanceAgentDogfood);
extern const base::FeatureParam<std::string>
    kDevToolsAiAssistancePerformanceAgentDogfoodModelId;
extern const base::FeatureParam<double>
    kDevToolsAiAssistancePerformanceAgentDogfoodTemperature;
extern const base::FeatureParam<DevToolsFreestylerUserTier>
    kDevToolsAiAssistancePerformanceAgentDogfoodUserTier;

BASE_DECLARE_FEATURE(kDevToolsAiAssistanceFileAgentDogfood);
extern const base::FeatureParam<std::string>
    kDevToolsAiAssistanceFileAgentDogfoodModelId;
extern const base::FeatureParam<double>
    kDevToolsAiAssistanceFileAgentDogfoodTemperature;
extern const base::FeatureParam<DevToolsFreestylerUserTier>
    kDevToolsAiAssistanceFileAgentDogfoodUserTier;

BASE_DECLARE_FEATURE(kDevToolsSharedProcessInfobar);
BASE_DECLARE_FEATURE(kDevToolsVeLogging);
extern const base::FeatureParam<bool> kDevToolsVeLoggingTesting;

}  // namespace features

#endif  // CHROME_BROWSER_DEVTOOLS_FEATURES_H_
