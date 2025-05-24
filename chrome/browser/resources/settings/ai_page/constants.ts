// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These values must stay in sync with
// optimization_guide::prefs::FeatureOptInState in
// components/optimization_guide/core/optimization_guide_prefs.h.
export enum FeatureOptInState {
  NOT_INITIALIZED = 0,
  ENABLED = 1,
  DISABLED = 2,
}

// LINT.IfChange(ModelExecutionEnterprisePolicyValue)
export enum ModelExecutionEnterprisePolicyValue {
  ALLOW = 0,
  ALLOW_WITHOUT_LOGGING = 1,
  DISABLE = 2,
}
// LINT.ThenChange(/components/optimization_guide/core/model_execution/model_execution_prefs.h:ModelExecutionEnterprisePolicyValue)


// Exporting pref names so that they can be referenced by tests.
export enum SettingsAiPageFeaturePrefName {
  HISTORY_SEARCH = 'optimization_guide.history_search_setting_state',
  COMPOSE = 'optimization_guide.compose_setting_state',
  TAB_ORGANIZATION = 'optimization_guide.tab_organization_setting_state',
}

// Exporting enterprise pref names so that they can be referenced by tests.
export enum AiEnterpriseFeaturePrefName {
  HISTORY_SEARCH =
      'optimization_guide.model_execution.history_search_enterprise_policy_allowed',
  COMPOSE =
      'optimization_guide.model_execution.compose_enterprise_policy_allowed',
  TAB_ORGANIZATION =
      'optimization_guide.model_execution.tab_organization_enterprise_policy_allowed',
  COMPARE =
      'optimization_guide.model_execution.tab_compare_settings_enterprise_policy',
  AUTOFILL_AI =
      'optimization_guide.model_execution.autofill_prediction_improvements_enterprise_policy_allowed',
}

export enum AiPageActions {
  HISTORY_SEARCH_ENABLED = 'Settings.AiPage.HistorySearch.Enabled',
  HISTORY_SEARCH_DISABLED = 'Settings.AiPage.HistorySearch.Disabled',
  HISTORY_SEARCH_FEATURE_LINK_CLICKED =
      'Settings.AiPage.HistorySearch.FeatureLinkClicked',
  HISTORY_SEARCH_LEARN_MORE_CLICKED =
      'Settings.AiPage.HistorySearch.LearnMoreClicked',
  COMPARE_FEATURE_LINK_CLICKED = 'Settings.AiPage.Compare.FeatureLinkClicked',
  COMPARE_LEARN_MORE_CLICKED = 'Settings.AiPage.Compare.LearnMoreClicked',
  COMPOSE_LEARN_MORE_CLICKED = 'Settings.AiPage.Compose.LearnMoreClicked',
  COMPOSE_PROACTIVE_NUDGE_ENABLED =
      'Settings.AiPage.Compose.ProactiveNudgeEnabled',
  COMPOSE_PROACTIVE_NUDGE_DISABLED =
      'Settings.AiPage.Compose.ProactiveNudgeDisabled',
  TAB_ORGANIZATION_LEARN_MORE_CLICKED =
      'Settings.AiPage.TabOrganization.LearnMoreClicked',
  GLIC_COLLAPSED_LEARN_MORE_CLICKED =
      'Settings.AiPage.GlicCollapsed.LearnMoreClicked',
  GLIC_SHORTCUTS_LEARN_MORE_CLICKED =
      'Settings.AiPage.GlicShortcuts.LearnMoreClicked',
  GLIC_SHORTCUTS_LAUNCHER_TOGGLE_LEARN_MORE_CLICKED =
      'Settings.AiPage.GlicShortcuts.LauncherToggleLearnMoreClicked',
  GLIC_SHORTCUTS_LOCATION_TOGGLE_LEARN_MORE_CLICKED =
      'Settings.AiPage.GlicShortcuts.LocationToggleLearnMoreClicked',
  GLIC_SHORTCUTS_TAB_ACCESS_TOGGLE_LEARN_MORE_CLICKED =
      'Settings.AiPage.GlicShortcuts.TabAccessToggleLearnMoreClicked',
}
