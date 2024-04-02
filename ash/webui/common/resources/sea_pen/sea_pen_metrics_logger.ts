// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SeaPenTemplateId} from './sea_pen_generated.mojom-webui.js';

const enum HistogramName {
  SEA_PEN_TEMPLATE_SUBPAGE = 'Ash.SeaPen.Template',
  SEA_PEN_CREATE_BUTTON = 'Ash.SeaPen.CreateButton',
}

export function logSeaPenTemplateFeedback(
    templateName: string, positiveFeedback: boolean) {
  chrome.metricsPrivate.recordBoolean(
      `Ash.SeaPen.${templateName}.UserFeedback`, positiveFeedback);
}

export function logGenerateSeaPenWallpaper(templateId: SeaPenTemplateId) {
  chrome.metricsPrivate.recordEnumerationValue(
      HistogramName.SEA_PEN_CREATE_BUTTON, templateId,
      SeaPenTemplateId.MAX_VALUE + 1);
}

export function logSeaPenTemplateSelect(templateId: SeaPenTemplateId) {
  chrome.metricsPrivate.recordEnumerationValue(
      HistogramName.SEA_PEN_TEMPLATE_SUBPAGE, templateId as SeaPenTemplateId,
      SeaPenTemplateId.MAX_VALUE + 1);
}
