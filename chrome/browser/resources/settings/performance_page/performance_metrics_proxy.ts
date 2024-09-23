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
// This must be kept in sync with MemorySaverModeAggressiveness in
// components/performance_manager/public/user_tuning/prefs.h
export enum MemorySaverModeAggressiveness {
  CONSERVATIVE = 0,
  MEDIUM = 1,
  AGGRESSIVE = 2,

  // Must be last.
  COUNT = 3,
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
export enum MemorySaverModeExceptionListAction {
  ADD_MANUAL = 0,
  EDIT = 1,
  REMOVE = 2,
  ADD_FROM_CURRENT = 3,

  // Must be last.
  COUNT = 4,
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// This must be kept in sync with MemorySaverModeState in
// components/performance_manager/public/user_tuning/prefs.h
export enum MemorySaverModeState {
  DISABLED = 0,
  DEPRECATED = 1,
  ENABLED = 2,

  // Must be last.
  COUNT = 3,
}

export interface PerformanceMetricsProxy {
  recordBatterySaverModeChanged(state: BatterySaverModeState): void;
  recordMemorySaverModeChanged(state: MemorySaverModeState): void;
  recordMemorySaverModeAggressivenessChanged(
      aggressiveness: MemorySaverModeAggressiveness): void;
  recordDiscardRingTreatmentEnabledChanged(enabled: boolean): void;
  recordExceptionListAction(action: MemorySaverModeExceptionListAction): void;
  recordPerformanceInterventionToggleButtonChanged(enabled: boolean): void;
}

export class PerformanceMetricsProxyImpl implements PerformanceMetricsProxy {
  recordBatterySaverModeChanged(state: BatterySaverModeState) {
    chrome.metricsPrivate.recordEnumerationValue(
        'PerformanceControls.BatterySaver.SettingsChangeMode', state,
        BatterySaverModeState.COUNT);
  }

  recordMemorySaverModeChanged(state: MemorySaverModeState): void {
    chrome.metricsPrivate.recordEnumerationValue(
        'PerformanceControls.MemorySaver.SettingsChangeMode', state,
        MemorySaverModeState.COUNT);
  }

  recordMemorySaverModeAggressivenessChanged(
      aggressiveness: MemorySaverModeAggressiveness): void {
    chrome.metricsPrivate.recordEnumerationValue(
        'PerformanceControls.MemorySaver.SettingsChangeAggressiveness',
        aggressiveness, MemorySaverModeAggressiveness.COUNT);
  }

  recordDiscardRingTreatmentEnabledChanged(enabled: boolean): void {
    chrome.metricsPrivate.recordBoolean(
        'PerformanceControls.MemorySaver.DiscardRingTreatment', enabled);
  }

  recordExceptionListAction(action: MemorySaverModeExceptionListAction) {
    chrome.metricsPrivate.recordEnumerationValue(
        'PerformanceControls.MemorySaver.SettingsChangeExceptionList', action,
        MemorySaverModeExceptionListAction.COUNT);
  }

  recordPerformanceInterventionToggleButtonChanged(enabled: boolean): void {
    chrome.metricsPrivate.recordBoolean(
        'PerformanceControls.Intervention.SettingsChangeNotification', enabled);
  }

  static getInstance(): PerformanceMetricsProxy {
    return instance || (instance = new PerformanceMetricsProxyImpl());
  }

  static setInstance(obj: PerformanceMetricsProxy) {
    instance = obj;
  }
}

let instance: PerformanceMetricsProxy|null = null;
