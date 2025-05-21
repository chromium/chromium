// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"

namespace features {

// Let the DevTools front-end query an AIDA endpoint for explanations and
// insights regarding console (error) messages.
BASE_FEATURE(kDevToolsConsoleInsights,
             "DevToolsConsoleInsights",
             base::FEATURE_ENABLED_BY_DEFAULT);
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
BASE_FEATURE(kDevToolsFreestyler,
             "DevToolsFreestyler",
             base::FEATURE_ENABLED_BY_DEFAULT);
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
    &kDevToolsFreestyler, "multimodal_upload_input", /*default_value=*/false};
const base::FeatureParam<bool> kDevToolsFreestylerFunctionCalling{
    &kDevToolsFreestyler, "function_calling", /*default_value=*/false};

// Whether the DevTools AI Assistance Network Agent is enabled.
BASE_FEATURE(kDevToolsAiAssistanceNetworkAgent,
             "DevToolsAiAssistanceNetworkAgent",
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
             "DevToolsAiAssistancePerformanceAgent",
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
BASE_FEATURE(kDevToolsAiAssistanceFileAgent,
             "DevToolsAiAssistanceFileAgent",
             base::FEATURE_ENABLED_BY_DEFAULT);
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

// Whether an infobar is shown when the process is shared.
BASE_FEATURE(kDevToolsSharedProcessInfobar,
             "DevToolsSharedProcessInfobar",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Let DevTools front-end log extensive VisualElements-style UMA metrics for
// impressions and interactions.
BASE_FEATURE(kDevToolsVeLogging,
             "DevToolsVeLogging",
             base::FEATURE_ENABLED_BY_DEFAULT);
// Run VE logging in a test mode
const base::FeatureParam<bool> kDevToolsVeLoggingTesting{
    &kDevToolsVeLogging, "testing", /*default_value=*/false};

// Whether showing animation styles in the styles tab is enabled.
BASE_FEATURE(kDevToolsAnimationStylesInStylesTab,
             "DevToolsAnimationStylesInStylesTab",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether DevTools will attempt to automatically connect Workspace folders.
// See http://go/chrome-devtools:automatic-workspace-folders-design for details.
BASE_FEATURE(kDevToolsAutomaticFileSystems,
             "DevToolsAutomaticFileSystems",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether DevTools will attempt to load project settings from a well-known
// URI. See https://goo.gle/devtools-json-design for additional details.
// This is enabled by default starting with M-136.
BASE_FEATURE(kDevToolsWellKnown,
             "DevToolsWellKnown",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether DevTools will offer the new CSS value tracing UI.
BASE_FEATURE(kDevToolsCssValueTracing,
             "DevToolsCssValueTracing",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether the DevTools AI generated annotation labels in timeline are enabled.
BASE_FEATURE(kDevToolsAiGeneratedTimelineLabels,
             "DevToolsAiGeneratedTimelineLabels",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether the DevTools AI generated annotation labels in timeline are enabled.
BASE_FEATURE(kDevToolsNewPermissionDialog,
             "DevToolsNewPermissionDialog",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// If enabled, DevTools does not accept remote debugging connections unless
// using a non-default user data dir via the --user-data-dir switch.
BASE_FEATURE(kDevToolsDebuggingRestrictions,
             "DevToolsDebuggingRestrictions",
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

// Whether DevTools drawer can be toggled to vertical orientation.
BASE_FEATURE(kDevToolsVerticalDrawer,
             "DevToolsVerticalDrawer",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
