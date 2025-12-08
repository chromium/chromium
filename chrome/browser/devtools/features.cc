// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace features {

// Let the DevTools front-end query an AIDA endpoint for explanations and
// insights regarding console (error) messages.
BASE_FEATURE(kDevToolsConsoleInsights, base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<std::string> kDevToolsConsoleInsightsModelId{
    &kDevToolsConsoleInsights, "aida_model_id", /*default_value=*/""};
const base::FeatureParam<double> kDevToolsConsoleInsightsTemperature{
    &kDevToolsConsoleInsights, "aida_temperature", /*default_value=*/-1};
const base::FeatureParam<bool> kDevToolsConsoleInsightsOptIn{
    &kDevToolsConsoleInsights, "opt_in", /*default_value=*/false};

const base::FeatureParam<DevToolsFreestylerUserTier>::Option
    devtools_freestyler_user_tier_options[] = {
        {DevToolsFreestylerUserTier::kTesters, "TESTERS"},
        {DevToolsFreestylerUserTier::kBeta, "BETA"},
        {DevToolsFreestylerUserTier::kPublic, "PUBLIC"}};

const base::FeatureParam<DevToolsFreestylerExecutionMode>::Option
    devtools_freestyler_execution_mode_options[] = {
        {DevToolsFreestylerExecutionMode::kAllScripts, "ALL_SCRIPTS"},
        {DevToolsFreestylerExecutionMode::kSideEffectFreeScriptsOnly,
         "SIDE_EFFECT_FREE_SCRIPTS_ONLY"},
        {DevToolsFreestylerExecutionMode::kNoScripts, "NO_SCRIPTS"}};

// Whether the DevTools styling assistant is enabled.
BASE_FEATURE(kDevToolsFreestyler, base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<std::string> kDevToolsFreestylerModelId{
    &kDevToolsFreestyler, "aida_model_id", /*default_value=*/""};
const base::FeatureParam<double> kDevToolsFreestylerTemperature{
    &kDevToolsFreestyler, "aida_temperature", /*default_value=*/-1};
const base::FeatureParam<DevToolsFreestylerUserTier>
    kDevToolsFreestylerUserTier{
        &kDevToolsFreestyler, "user_tier",
        /*default_value=*/DevToolsFreestylerUserTier::kPublic,
        &devtools_freestyler_user_tier_options};
const base::FeatureParam<DevToolsFreestylerExecutionMode>
    kDevToolsFreestylerExecutionMode{
        &kDevToolsFreestyler, "execution_mode",
        /*default_value=*/DevToolsFreestylerExecutionMode::kAllScripts,
        &devtools_freestyler_execution_mode_options};
const base::FeatureParam<bool> kDevToolsFreestylerPatching{
    &kDevToolsFreestyler, "patching", /*default_value=*/true};
const base::FeatureParam<bool> kDevToolsFreestylerMultimodal{
    &kDevToolsFreestyler, "multimodal", /*default_value=*/true};
const base::FeatureParam<bool> kDevToolsFreestylerMultimodalUploadInput{
    &kDevToolsFreestyler, "multimodal_upload_input", /*default_value=*/true};
const base::FeatureParam<bool> kDevToolsFreestylerFunctionCalling{
    &kDevToolsFreestyler, "function_calling", /*default_value=*/true};

// Whether the DevTools AI Assistance Network Agent is enabled.
BASE_FEATURE(kDevToolsAiAssistanceNetworkAgent,
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<std::string> kDevToolsAiAssistanceNetworkAgentModelId{
    &kDevToolsAiAssistanceNetworkAgent, "aida_model_id",
    /*default_value=*/""};
const base::FeatureParam<double> kDevToolsAiAssistanceNetworkAgentTemperature{
    &kDevToolsAiAssistanceNetworkAgent, "aida_temperature",
    /*default_value=*/-1};
const base::FeatureParam<DevToolsFreestylerUserTier>
    kDevToolsAiAssistanceNetworkAgentUserTier{
        &kDevToolsAiAssistanceNetworkAgent, "user_tier",
        /*default_value=*/DevToolsFreestylerUserTier::kPublic,
        &devtools_freestyler_user_tier_options};

// Whether the DevTools AI Assistance Performance Agent is enabled.
BASE_FEATURE(kDevToolsAiAssistancePerformanceAgent,
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<std::string>
    kDevToolsAiAssistancePerformanceAgentModelId{
        &kDevToolsAiAssistancePerformanceAgent, "aida_model_id",
        /*default_value=*/""};
const base::FeatureParam<double>
    kDevToolsAiAssistancePerformanceAgentTemperature{
        &kDevToolsAiAssistancePerformanceAgent, "aida_temperature",
        /*default_value=*/-1};
const base::FeatureParam<DevToolsFreestylerUserTier>
    kDevToolsAiAssistancePerformanceAgentUserTier{
        &kDevToolsAiAssistancePerformanceAgent, "user_tier",
        /*default_value=*/DevToolsFreestylerUserTier::kPublic,
        &devtools_freestyler_user_tier_options};
const base::FeatureParam<bool>
    kDevToolsAiAssistancePerformanceAgentInsightsEnabled{
        &kDevToolsAiAssistancePerformanceAgent, "insights_enabled",
        /*default_value=*/true};

// Whether the DevTools AI Assistance File Agent is enabled.
BASE_FEATURE(kDevToolsAiAssistanceFileAgent, base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<std::string> kDevToolsAiAssistanceFileAgentModelId{
    &kDevToolsAiAssistanceFileAgent, "aida_model_id",
    /*default_value=*/""};
const base::FeatureParam<double> kDevToolsAiAssistanceFileAgentTemperature{
    &kDevToolsAiAssistanceFileAgent, "aida_temperature",
    /*default_value=*/-1};
const base::FeatureParam<DevToolsFreestylerUserTier>
    kDevToolsAiAssistanceFileAgentUserTier{
        &kDevToolsAiAssistanceFileAgent, "user_tier",
        /*default_value=*/DevToolsFreestylerUserTier::kPublic,
        &devtools_freestyler_user_tier_options};

// Whether the DevTools AI Code Completion is enabled.
BASE_FEATURE(kDevToolsAiCodeCompletion, base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<std::string> kDevToolsAiCodeCompletionModelId{
    &kDevToolsAiCodeCompletion, "aida_model_id",
    /*default_value=*/""};
const base::FeatureParam<double> kDevToolsAiCodeCompletionTemperature{
    &kDevToolsAiCodeCompletion, "aida_temperature",
    /*default_value=*/-1};
const base::FeatureParam<DevToolsFreestylerUserTier>
    kDevToolsAiCodeCompletionUserTier{
        &kDevToolsAiCodeCompletion, "user_tier",
        /*default_value=*/DevToolsFreestylerUserTier::kPublic,
        &devtools_freestyler_user_tier_options};

// Whether the DevTools AI Code Generation is enabled.
BASE_FEATURE(kDevToolsAiCodeGeneration, base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<std::string> kDevToolsAiCodeGenerationModelId{
    &kDevToolsAiCodeGeneration, "aida_model_id",
    /*default_value=*/""};
const base::FeatureParam<double> kDevToolsAiCodeGenerationTemperature{
    &kDevToolsAiCodeGeneration, "aida_temperature",
    /*default_value=*/-1};
const base::FeatureParam<DevToolsFreestylerUserTier>
    kDevToolsAiCodeGenerationUserTier{
        &kDevToolsAiCodeGeneration, "user_tier",
        /*default_value=*/DevToolsFreestylerUserTier::kPublic,
        &devtools_freestyler_user_tier_options};

// Whether an infobar is shown when the process is shared.
BASE_FEATURE(kDevToolsSharedProcessInfobar, base::FEATURE_DISABLED_BY_DEFAULT);

// Whether showing animation styles in the styles tab is enabled.
BASE_FEATURE(kDevToolsAnimationStylesInStylesTab,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether DevTools will attempt to load project settings from a well-known
// URI. See https://goo.gle/devtools-json-design for additional details.
// This is enabled by default starting with M-136.
BASE_FEATURE(kDevToolsWellKnown, base::FEATURE_ENABLED_BY_DEFAULT);

// Whether the DevTools AI generated annotation labels in timeline are enabled.
BASE_FEATURE(kDevToolsAiGeneratedTimelineLabels,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether the DevTools AI generated annotation labels in timeline are enabled.
BASE_FEATURE(kDevToolsNewPermissionDialog, base::FEATURE_ENABLED_BY_DEFAULT);

// Whether DevTools drawer can be toggled to vertical orientation.
BASE_FEATURE(kDevToolsVerticalDrawer, base::FEATURE_ENABLED_BY_DEFAULT);

// Whether DevTools shows submenu example prompts for the AI Assistance panel
// in context menus.
BASE_FEATURE(kDevToolsAiSubmenuPrompts, base::FEATURE_ENABLED_BY_DEFAULT);

// Whether DevTools shows 'Debug with AI' and new badges.
BASE_FEATURE(kDevToolsAiDebugWithAi, base::FEATURE_DISABLED_BY_DEFAULT);

// Turns on the GreenDev experimental UI.
BASE_FEATURE(kDevToolsGreenDevUi, base::FEATURE_DISABLED_BY_DEFAULT);

// Whether the global AI entrypoint is enabled.
BASE_FEATURE(kDevToolsGlobalAiButton, base::FEATURE_ENABLED_BY_DEFAULT);
// Whether the promotion animation is enabled.
const base::FeatureParam<bool> kDevToolsGlobalAiButtonPromotionEnabled{
    &kDevToolsGlobalAiButton, "promotion_enabled",
    /*default_value=*/false};

// Whether the Google Developer Program integration is enabled.
BASE_FEATURE(kDevToolsGdpProfiles, base::FEATURE_ENABLED_BY_DEFAULT);
// Whether the badges for the Google Developer Program is enabled. It's used
// as a kill-switch to disable granting badges in case something goes wrong and
// we start spamming users with badge notifications.
const base::FeatureParam<bool> kDevToolsGdpProfilesBadgesEnabled{
    &kDevToolsGdpProfiles, "badges_enabled",
    /*default_value=*/true};
// Whether the starter badge for the Google Developer Program is enabled.
const base::FeatureParam<bool> kDevToolsGdpProfilesStarterBadgeEnabled{
    &kDevToolsGdpProfiles, "starter_badge_enabled",
    /*default_value=*/true};

// Whether DevTools Live Edit (Debugger.setScriptSource usage in CDP) is
// enabled.
BASE_FEATURE(kDevToolsLiveEdit, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDevToolsIndividualRequestThrottling,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether the AI Prompt API (https://developer.chrome.com/docs/ai/prompt-api)
// is available in DevTools.
BASE_FEATURE(kDevToolsAiPromptApi, base::FEATURE_DISABLED_BY_DEFAULT);
// Whether the Prompt API is allowed to run on devices without a dedicated GPU.
const base::FeatureParam<bool> kDevToolsAiPromptApiAllowWithoutGpu{
    &kDevToolsAiPromptApi, "allow_without_gpu",
    /*default_value=*/false};

// Whether showing animation styles in the styles tab is enabled.
BASE_FEATURE(kDevToolsStartingStyleDebugging, base::FEATURE_ENABLED_BY_DEFAULT);

// Whether Network panel should use Durable Messages to preserve network bodies.
BASE_FEATURE(kDevToolsEnableDurableMessages, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, allows starting remote debugging in a running Chrome instance.
BASE_FEATURE(kDevToolsAcceptDebuggingConnections,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether the policy dialog should be shown instead of greying out the
// Developer Tools toggle.
// TODO(crbug.com/442892562): Remove this flag once the feature is launched.
BASE_FEATURE(kDevToolsShowPolicyDialog, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDevToolsAiAssistanceContextSelectionAgent,
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
