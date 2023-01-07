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
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
export enum HighEfficiencyModeExceptionListAction {
  ADD = 0,
  EDIT = 1,
  REMOVE = 2,
}

function getNumericEnumLength(e: Object) {
  // numeric enums have reverse mapping when compiled to js, so must divide by 2
  return Object.keys(e).length / 2;
}

export interface PerformanceMetricsProxy {
  recordBatterySaverModeChanged(state: BatterySaverModeState): void;
  recordHighEfficiencyModeChanged(enabled: boolean): void;
  recordExceptionListAction(action: HighEfficiencyModeExceptionListAction):
      void;
}

export class PerformanceMetricsProxyImpl implements PerformanceMetricsProxy {
  recordBatterySaverModeChanged(state: BatterySaverModeState) {
    chrome.metricsPrivate.recordEnumerationValue(
        'PerformanceControls.BatterySaver.SettingsChangeMode', state,
        getNumericEnumLength(BatterySaverModeState));
  }

  recordHighEfficiencyModeChanged(enabled: boolean): void {
    chrome.metricsPrivate.recordBoolean(
        'PerformanceControls.HighEfficiency.SettingsChangeMode', enabled);
  }

  recordExceptionListAction(action: HighEfficiencyModeExceptionListAction) {
    chrome.metricsPrivate.recordEnumerationValue(
        'PerformanceControls.HighEfficiency.SettingsChangeExceptionList',
        action, getNumericEnumLength(HighEfficiencyModeExceptionListAction));
  }

  static getInstance(): PerformanceMetricsProxy {
    return instance || (instance = new PerformanceMetricsProxyImpl());
  }

  static setInstance(obj: PerformanceMetricsProxy) {
    instance = obj;
  }
}

let instance: PerformanceMetricsProxy|null = null;
