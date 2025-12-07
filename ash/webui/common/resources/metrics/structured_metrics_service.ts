// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Event} from './event.mojom-webui.js';
import type {StructuredMetricsServiceInterface} from './structured_metrics_service.mojom-webui.js';
import {StructuredMetricsService} from './structured_metrics_service.mojom-webui.js';

let structuredMetricsService: StructuredMetricsServiceInterface|null = null;

function getStructuredMetricsRecorder(): StructuredMetricsServiceInterface {
  if (structuredMetricsService) {
    return structuredMetricsService;
  }
  structuredMetricsService = StructuredMetricsService.getRemote();
  return structuredMetricsService;
}

export function record(event: Event) {
  getStructuredMetricsRecorder().record([event]);
}
