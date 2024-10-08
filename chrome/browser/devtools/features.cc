// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

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

// Whether the DevTools styling assistant dogfood is enabled.
BASE_FEATURE(kDevToolsFreestylerDogfood,
             "DevToolsFreestylerDogfood",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<std::string> kDevToolsFreestylerDogfoodModelId{
    &kDevToolsFreestylerDogfood, "aida_model_id", /*default_value=*/""};
const base::FeatureParam<double> kDevToolsFreestylerDogfoodTemperature{
    &kDevToolsFreestylerDogfood, "aida_temperature", /*default_value=*/-1};
const base::FeatureParam<DevToolsFreestylerUserTier>
    kDevToolsFreestylerDogfoodUserTier{
        &kDevToolsFreestylerDogfood, "user_tier",
        /*default_value=*/DevToolsFreestylerUserTier::kBeta,
        &devtools_freestyler_user_tier_options};

// Whether the DevTools styling assistant is enabled.
BASE_FEATURE(kDevToolsFreestyler,
             "DevToolsFreestyler",
             base::FEATURE_DISABLED_BY_DEFAULT);
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

// Whether the DevTools resource explainer assistant is enabled.
BASE_FEATURE(kDevToolsExplainThisResourceDogfood,
             "DevToolsExplainThisResourceDogfood",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<std::string>
    kDevToolsExplainThisResourceDogfoodModelId{
        &kDevToolsExplainThisResourceDogfood, "aida_model_id",
        /*default_value=*/""};
const base::FeatureParam<double> kDevToolsExplainThisResourceDogfoodTemperature{
    &kDevToolsExplainThisResourceDogfood, "aida_temperature",
    /*default_value=*/-1};
const base::FeatureParam<DevToolsFreestylerUserTier>
    kDevToolsExplainThisResourceDogfoodUserTier{
        &kDevToolsExplainThisResourceDogfood, "user_tier",
        /*default_value=*/DevToolsFreestylerUserTier::kBeta,
        &devtools_freestyler_user_tier_options};

// Whether the DevTools AI Assistance Performance Agent is enabled.
BASE_FEATURE(kDevToolsAiAssistancePerformanceAgentDogfood,
             "DevToolAiAssistancePerformanceAgentDogfood",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<std::string>
    kDevToolsAiAssistancePerformanceAgentDogfoodModelId{
        &kDevToolsAiAssistancePerformanceAgentDogfood, "aida_model_id",
        /*default_value=*/""};
const base::FeatureParam<double>
    kDevToolsAiAssistancePerformanceAgentDogfoodTemperature{
        &kDevToolsAiAssistancePerformanceAgentDogfood, "aida_temperature",
        /*default_value=*/-1};
const base::FeatureParam<DevToolsFreestylerUserTier>
    kDevToolsAiAssistancePerformanceAgentDogfoodUserTier{
        &kDevToolsAiAssistancePerformanceAgentDogfood, "user_tier",
        /*default_value=*/DevToolsFreestylerUserTier::kBeta,
        &devtools_freestyler_user_tier_options};

// Whether the DevTools AI Assistance File Agent is enabled.
BASE_FEATURE(kDevToolsAiAssistanceFileAgentDogfood,
             "DevToolsAiAssistanceFileAgentDogfood",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<std::string>
    kDevToolsAiAssistanceFileAgentDogfoodModelId{
        &kDevToolsAiAssistanceFileAgentDogfood, "aida_model_id",
        /*default_value=*/""};
const base::FeatureParam<double>
    kDevToolsAiAssistanceFileAgentDogfoodTemperature{
        &kDevToolsAiAssistanceFileAgentDogfood, "aida_temperature",
        /*default_value=*/-1};
const base::FeatureParam<DevToolsFreestylerUserTier>
    kDevToolsAiAssistanceFileAgentDogfoodUserTier{
        &kDevToolsAiAssistanceFileAgentDogfood, "user_tier",
        /*default_value=*/DevToolsFreestylerUserTier::kBeta,
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

}  // namespace features
