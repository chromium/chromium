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

// Exporting pref names so that they can be referenced by tests.
export enum SettingsAiPageFeaturePrefName {
  MAIN = 'optimization_guide.model_execution_main_toggle_setting_state',
  HISTORY_SEARCH = 'optimization_guide.history_search_setting_state',
  COMPOSE = 'optimization_guide.compose_setting_state',
  TAB_ORGANIZATION = 'optimization_guide.tab_organization_setting_state',
  WALLPAPER_SEARCH = 'optimization_guide.wallpaper_search_setting_state',
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
}
