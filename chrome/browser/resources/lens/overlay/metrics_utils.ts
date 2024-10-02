// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxyImpl} from './browser_proxy.js';
import {UserAction} from './lens.mojom-webui.js';
import type {SemanticEvent} from './lens.mojom-webui.js';

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
