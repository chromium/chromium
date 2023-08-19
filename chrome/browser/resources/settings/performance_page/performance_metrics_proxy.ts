// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// This must be kept in sync with BatterySaverModeState in
// components/performance_manager/public/user_tuning/prefs.h
export enum BatterySaverModeState {
  DISABLED = 0,
  ENABLED_BELOW_THRESHOLD = 1,
  ENABLED_ON_BATTERY = 2,
  ENABLED = 3,

  // Must be last.
  COUNT = 4,
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
export enum HighEfficiencyModeExceptionListAction {
  ADD_MANUAL = 0,
  EDIT = 1,
  REMOVE = 2,
  ADD_FROM_CURRENT = 3,

  // Must be last.
  COUNT = 4,
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// This must be kept in sync with HighEfficiencyModeState in
// components/performance_manager/public/user_tuning/prefs.h
export enum HighEfficiencyModeState {
  DISABLED = 0,
  ENABLED = 1,
  ENABLED_ON_TIMER = 2,

  // Must be last.
  COUNT = 3,
}

export interface PerformanceMetricsProxy {
  recordBatterySaverModeChanged(state: BatterySaverModeState): void;
  recordHighEfficiencyModeChanged(state: HighEfficiencyModeState): void;
  recordExceptionListAction(action: HighEfficiencyModeExceptionListAction):
      void;
}

export class PerformanceMetricsProxyImpl implements PerformanceMetricsProxy {
  recordBatterySaverModeChanged(state: BatterySaverModeState) {
    chrome.metricsPrivate.recordEnumerationValue(
        'PerformanceControls.BatterySaver.SettingsChangeMode', state,
        BatterySaverModeState.COUNT);
  }

  recordHighEfficiencyModeChanged(state: HighEfficiencyModeState): void {
    chrome.metricsPrivate.recordEnumerationValue(
        'PerformanceControls.HighEfficiency.SettingsChangeMode2', state,
        HighEfficiencyModeState.COUNT);
  }

  recordExceptionListAction(action: HighEfficiencyModeExceptionListAction) {
    chrome.metricsPrivate.recordEnumerationValue(
        'PerformanceControls.HighEfficiency.SettingsChangeExceptionList',
        action, HighEfficiencyModeExceptionListAction.COUNT);
  }

  static getInstance(): PerformanceMetricsProxy {
    return instance || (instance = new PerformanceMetricsProxyImpl());
  }

  static setInstance(obj: PerformanceMetricsProxy) {
    instance = obj;
  }
}

let instance: PerformanceMetricsProxy|null = null;
