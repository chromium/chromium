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

BASE_DECLARE_FEATURE(kDevToolsAiCodeCompletion);
extern const base::FeatureParam<std::string> kDevToolsAiCodeCompletionModelId;
extern const base::FeatureParam<double> kDevToolsAiCodeCompletionTemperature;
extern const base::FeatureParam<DevToolsFreestylerUserTier>
    kDevToolsAiCodeCompletionUserTier;

BASE_DECLARE_FEATURE(kDevToolsAiCodeGeneration);
extern const base::FeatureParam<std::string> kDevToolsAiCodeGenerationModelId;
extern const base::FeatureParam<double> kDevToolsAiCodeGenerationTemperature;
extern const base::FeatureParam<DevToolsFreestylerUserTier>
    kDevToolsAiCodeGenerationUserTier;

BASE_DECLARE_FEATURE(kDevToolsSharedProcessInfobar);

BASE_DECLARE_FEATURE(kDevToolsAnimationStylesInStylesTab);

BASE_DECLARE_FEATURE(kDevToolsWellKnown);

BASE_DECLARE_FEATURE(kDevToolsAiGeneratedTimelineLabels);

BASE_DECLARE_FEATURE(kDevToolsNewPermissionDialog);

BASE_DECLARE_FEATURE(kDevToolsVerticalDrawer);

BASE_DECLARE_FEATURE(kDevToolsAiSubmenuPrompts);
BASE_DECLARE_FEATURE(kDevToolsAiDebugWithAi);

BASE_DECLARE_FEATURE(kDevToolsGreenDevUi);

BASE_DECLARE_FEATURE(kDevToolsGlobalAiButton);
extern const base::FeatureParam<bool> kDevToolsGlobalAiButtonPromotionEnabled;

BASE_DECLARE_FEATURE(kDevToolsGdpProfiles);
extern const base::FeatureParam<bool> kDevToolsGdpProfilesBadgesEnabled;
extern const base::FeatureParam<bool> kDevToolsGdpProfilesStarterBadgeEnabled;

BASE_DECLARE_FEATURE(kDevToolsLiveEdit);

BASE_DECLARE_FEATURE(kDevToolsIndividualRequestThrottling);

BASE_DECLARE_FEATURE(kDevToolsAiPromptApi);
extern const base::FeatureParam<bool> kDevToolsAiPromptApiAllowWithoutGpu;

BASE_DECLARE_FEATURE(kDevToolsStartingStyleDebugging);

BASE_DECLARE_FEATURE(kDevToolsEnableDurableMessages);

BASE_DECLARE_FEATURE(kDevToolsAcceptDebuggingConnections);

BASE_DECLARE_FEATURE(kDevToolsShowPolicyDialog);

BASE_DECLARE_FEATURE(kDevToolsAiAssistanceContextSelectionAgent);

}  // namespace features

#endif  // CHROME_BROWSER_DEVTOOLS_FEATURES_H_
