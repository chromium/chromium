// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxyImpl} from './browser_proxy.js';
import {UserAction} from './lens.mojom-webui.js';
import type {SemanticEvent} from './lens.mojom-webui.js';


// LINT.IfChange(ContextMenuOption)
// The possible context menu options that can appear in the Lens overlay.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
export enum ContextMenuOption {
  COPY_TEXT = 0,
  TRANSLATE_TEXT = 1,
  SELECT_TEXT_IN_REGION = 2,
  TRANSLATE_TEXT_IN_REGION = 3,
  COPY_AS_IMAGE = 4,
  SAVE_AS_IMAGE = 5,
  COPY_TEXT_IN_REGION = 6,
  // Must be last.
  COUNT = 7,
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/lens/enums.xml:LensOverlayContextMenuOption)

// The possible events for the selection overlay close button.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(LensOverlaySelectionCloseButtonEvent)
export enum LensOverlaySelectionCloseButtonEvent {
  SHOWN = 0,
  USED = 1,
  // Must be last.
  COUNT = 2,
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/lens/enums.xml:LensOverlaySelectionCloseButtonEvent)

export function recordContextMenuOptionShown(
    invocationSource: string, contextMenuOption: ContextMenuOption) {
  chrome.metricsPrivate.recordEnumerationValue(
      `Lens.Overlay.ContextMenuOption.Shown`, contextMenuOption,
      ContextMenuOption.COUNT);
  chrome.metricsPrivate.recordEnumerationValue(
      `Lens.Overlay.ByInvocationSource.${
          invocationSource}.ContextMenuOption.Shown`,
      contextMenuOption, ContextMenuOption.COUNT);
}

export function recordLensOverlayInteraction(
    invocationSource: string, interaction: UserAction) {
  chrome.metricsPrivate.recordEnumerationValue(
      'Lens.Overlay.Overlay.UserAction', interaction, UserAction.MAX_VALUE + 1);
  chrome.metricsPrivate.recordEnumerationValue(
      `Lens.Overlay.Overlay.ByInvocationSource.${invocationSource}.UserAction`,
      interaction, UserAction.MAX_VALUE + 1);
  BrowserProxyImpl.getInstance()
      .handler.recordUkmAndTaskCompletionForLensOverlayInteraction(interaction);
}

export function recordLensOverlaySemanticEvent(semanticEvent: SemanticEvent) {
  BrowserProxyImpl.getInstance().handler.recordLensOverlaySemanticEvent(
      semanticEvent);
}

/** Records |durationMs| in the |metricName| histogram. */
export function recordTimeToWebUIReady(durationMs: number) {
  chrome.metricsPrivate.recordValue(
      {
        metricName: 'Lens.Overlay.TimeToWebUIReady',
        type: chrome.metricsPrivate.MetricTypeType.HISTOGRAM_LOG,
        min: 1,
        max: 50000,  // 50 seconds.
        buckets: 100,
      },
      Math.floor(durationMs));
}

/**
 * Records |averageFps| in the Lens.Overlay.Performance.AverageFPS histogram.
 */
export function recordAverageFps(averageFps: number) {
  chrome.metricsPrivate.recordValue(
      {
        metricName: 'Lens.Overlay.Performance.AverageFPS',
        type: chrome.metricsPrivate.MetricTypeType.HISTOGRAM_LOG,
        min: 1,
        max: 200,
        buckets: 50,
      },
      Math.floor(averageFps));
}

export function recordLensOverlaySelectionCloseButtonShown(
    invocationSource: string) {
  chrome.metricsPrivate.recordEnumerationValue(
      `Lens.Overlay.ByInvocationSource.${
          invocationSource}.SelectionState.CloseButtonEvent`,
      LensOverlaySelectionCloseButtonEvent.SHOWN,
      LensOverlaySelectionCloseButtonEvent.COUNT);
  chrome.metricsPrivate.recordEnumerationValue(
      `Lens.Overlay.SelectionState.CloseButtonEvent`,
      LensOverlaySelectionCloseButtonEvent.SHOWN,
      LensOverlaySelectionCloseButtonEvent.COUNT);
}

export function recordLensOverlaySelectionCloseButtonUsed(
    invocationSource: string) {
  chrome.metricsPrivate.recordEnumerationValue(
      `Lens.Overlay.ByInvocationSource.${
          invocationSource}.SelectionState.CloseButtonEvent`,
      LensOverlaySelectionCloseButtonEvent.USED,
      LensOverlaySelectionCloseButtonEvent.COUNT);
  chrome.metricsPrivate.recordEnumerationValue(
      `Lens.Overlay.SelectionState.CloseButtonEvent`,
      LensOverlaySelectionCloseButtonEvent.USED,
      LensOverlaySelectionCloseButtonEvent.COUNT);
}
