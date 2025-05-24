// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_FEATURES_H_
#define CHROME_BROWSER_DEVTOOLS_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace features {

BASE_DECLARE_FEATURE(kDevToolsConsoleInsights);
extern const base::FeatureParam<std::string> kDevToolsConsoleInsightsModelId;
extern const base::FeatureParam<double> kDevToolsConsoleInsightsTemperature;
extern const base::FeatureParam<bool> kDevToolsConsoleInsightsOptIn;


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

BASE_DECLARE_FEATURE(kDevToolsFreestyler);
extern const base::FeatureParam<std::string> kDevToolsFreestylerModelId;
extern const base::FeatureParam<double> kDevToolsFreestylerTemperature;
extern const base::FeatureParam<DevToolsFreestylerUserTier>
    kDevToolsFreestylerUserTier;
extern const base::FeatureParam<DevToolsFreestylerExecutionMode>
    kDevToolsFreestylerExecutionMode;
extern const base::FeatureParam<bool> kDevToolsFreestylerPatching;
extern const base::FeatureParam<bool> kDevToolsFreestylerMultimodal;
extern const base::FeatureParam<bool> kDevToolsFreestylerMultimodalUploadInput;
extern const base::FeatureParam<bool> kDevToolsFreestylerFunctionCalling;

BASE_DECLARE_FEATURE(kDevToolsAiAssistanceNetworkAgent);
extern const base::FeatureParam<std::string>
    kDevToolsAiAssistanceNetworkAgentModelId;
extern const base::FeatureParam<double>
    kDevToolsAiAssistanceNetworkAgentTemperature;
extern const base::FeatureParam<DevToolsFreestylerUserTier>
    kDevToolsAiAssistanceNetworkAgentUserTier;

BASE_DECLARE_FEATURE(kDevToolsAiAssistancePerformanceAgent);
extern const base::FeatureParam<std::string>
    kDevToolsAiAssistancePerformanceAgentModelId;
extern const base::FeatureParam<double>
    kDevToolsAiAssistancePerformanceAgentTemperature;
extern const base::FeatureParam<DevToolsFreestylerUserTier>
    kDevToolsAiAssistancePerformanceAgentUserTier;
extern const base::FeatureParam<bool>
    kDevToolsAiAssistancePerformanceAgentInsightsEnabled;

BASE_DECLARE_FEATURE(kDevToolsAiAssistanceFileAgent);
extern const base::FeatureParam<std::string>
    kDevToolsAiAssistanceFileAgentModelId;
extern const base::FeatureParam<double>
    kDevToolsAiAssistanceFileAgentTemperature;
extern const base::FeatureParam<DevToolsFreestylerUserTier>
    kDevToolsAiAssistanceFileAgentUserTier;

BASE_DECLARE_FEATURE(kDevToolsSharedProcessInfobar);
BASE_DECLARE_FEATURE(kDevToolsVeLogging);
extern const base::FeatureParam<bool> kDevToolsVeLoggingTesting;

BASE_DECLARE_FEATURE(kDevToolsAnimationStylesInStylesTab);

BASE_DECLARE_FEATURE(kDevToolsAutomaticFileSystems);

BASE_DECLARE_FEATURE(kDevToolsWellKnown);

BASE_DECLARE_FEATURE(kDevToolsCssValueTracing);

BASE_DECLARE_FEATURE(kDevToolsAiGeneratedTimelineLabels);

BASE_DECLARE_FEATURE(kDevToolsNewPermissionDialog);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
BASE_DECLARE_FEATURE(kDevToolsDebuggingRestrictions);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

BASE_DECLARE_FEATURE(kDevToolsVerticalDrawer);

}  // namespace features

#endif  // CHROME_BROWSER_DEVTOOLS_FEATURES_H_
