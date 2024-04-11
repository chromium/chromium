// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SeaPenTemplateId} from './sea_pen_generated.mojom-webui.js';
import {isPersonalizationApp} from './sea_pen_utils.js';

const enum HistogramName {
  SEA_PEN_TEMPLATE_SUBPAGE = 'Ash.SeaPen.Template',
  SEA_PEN_THUMBNAIL_CLICKED = 'Ash.SeaPen.ThumbnailClicked',
  SEA_PEN_CREATE_BUTTON = 'Ash.SeaPen.CreateButton',
}

// Numerical values are used for metrics; do not change or reuse values.
export enum RecentImageActionMenuItem {
  CREATE_MORE = 0,
  DELETE,
  ABOUT,
  MAX_VALUE = ABOUT,
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

export function logRecentImageActionMenuItemClick(
    menuItem: RecentImageActionMenuItem) {
  const appName = isPersonalizationApp() ? 'Wallpaper' : 'VcBackground';
  chrome.metricsPrivate.recordEnumerationValue(
      `Ash.SeaPen.${appName}.RecentImage.ActionMenu`, menuItem,
      RecentImageActionMenuItem.MAX_VALUE + 1);
}

export function logSeaPenImageSet(source: 'Create'|'Recent') {
  const appName = isPersonalizationApp() ? 'Wallpaper' : 'VcBackground';
  chrome.metricsPrivate.recordBoolean(
      `Ash.SeaPen.${appName}.${source}.ImageSet`, true);
}

export function logSeaPenVisited() {
  const appName = isPersonalizationApp() ? 'Wallpaper' : 'VcBackground';
  chrome.metricsPrivate.recordBoolean(`Ash.SeaPen.${appName}.Visited`, true);
}

export function logSeaPenThumbnailClicked(templateId: SeaPenTemplateId) {
  chrome.metricsPrivate.recordEnumerationValue(
      HistogramName.SEA_PEN_THUMBNAIL_CLICKED, templateId,
      SeaPenTemplateId.MAX_VALUE + 1);
}
