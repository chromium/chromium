// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import {QUERY, Query} from './constants.js';
import {SeaPenTemplateId} from './sea_pen_generated.mojom-webui.js';
import {isPersonalizationApp} from './sea_pen_utils.js';

const WALLPAPER_FREEFORM = 999;
const VC_BACKGROUND_FREEFORM = 1000;

const enum HistogramName {
  SEA_PEN_TEMPLATE_SUBPAGE = 'Ash.SeaPen.Template',
  SEA_PEN_THUMBNAIL_CLICKED = 'Ash.SeaPen.ThumbnailClicked',
  SEA_PEN_CREATE_BUTTON = 'Ash.SeaPen.CreateButton',
  SEA_PEN_WORD_COUNT = 'Ash.SeaPen.WordCount',
}

function getTemplateIdForMetrics(templateId: SeaPenTemplateId|Query): number {
  if (templateId === QUERY) {
    return isPersonalizationApp() ? WALLPAPER_FREEFORM : VC_BACKGROUND_FREEFORM;
  }
  return templateId as SeaPenTemplateId;
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

export function logGenerateSeaPenWallpaper(templateId: SeaPenTemplateId|Query) {
  const templateIdForMetrics = getTemplateIdForMetrics(templateId);
  assert(
      templateIdForMetrics <= VC_BACKGROUND_FREEFORM,
      `Template ID ${
          templateIdForMetrics} should not be greater than VC_BACKGROUND_FREEFORM.`);
  chrome.metricsPrivate.recordEnumerationValue(
      HistogramName.SEA_PEN_CREATE_BUTTON, templateIdForMetrics,
      VC_BACKGROUND_FREEFORM + 1);
}

export function logSeaPenTemplateSelect(templateId: SeaPenTemplateId|Query) {
  const templateIdForMetrics = getTemplateIdForMetrics(templateId);
  assert(
      templateIdForMetrics <= VC_BACKGROUND_FREEFORM,
      `Template ID ${
          templateIdForMetrics} should not be greater than VC_BACKGROUND_FREEFORM.`);
  chrome.metricsPrivate.recordEnumerationValue(
      HistogramName.SEA_PEN_TEMPLATE_SUBPAGE, templateIdForMetrics,
      VC_BACKGROUND_FREEFORM + 1);
}

export function logRecentImageActionMenuItemClick(
    isTextQuery: boolean, menuItem: RecentImageActionMenuItem) {
  const appName = isPersonalizationApp() ? 'Wallpaper' : 'VcBackground';
  const histogramName = isTextQuery ?
      `Ash.SeaPen.Freeform.${appName}.RecentImage.ActionMenu` :
      `Ash.SeaPen.${appName}.RecentImage.ActionMenu`;
  chrome.metricsPrivate.recordEnumerationValue(
      histogramName, menuItem, RecentImageActionMenuItem.MAX_VALUE + 1);
}

export function logSeaPenImageSet(
    isTextQuery: boolean, source: 'Create'|'Recent') {
  const appName = isPersonalizationApp() ? 'Wallpaper' : 'VcBackground';
  const histogramName = isTextQuery ?
      `Ash.SeaPen.Freeform.${appName}.${source}.ImageSet` :
      `Ash.SeaPen.${appName}.${source}.ImageSet`;
  chrome.metricsPrivate.recordBoolean(histogramName, true);
}

export function logSeaPenVisited() {
  const appName = isPersonalizationApp() ? 'Wallpaper' : 'VcBackground';
  chrome.metricsPrivate.recordBoolean(`Ash.SeaPen.${appName}.Visited`, true);
}

export function logSeaPenThumbnailClicked(templateId: SeaPenTemplateId|Query) {
  const templateIdForMetrics = getTemplateIdForMetrics(templateId);
  assert(
      templateIdForMetrics <= VC_BACKGROUND_FREEFORM,
      `Template ID ${
          templateIdForMetrics} should not be greater than VC_BACKGROUND_FREEFORM.`);
  chrome.metricsPrivate.recordEnumerationValue(
      HistogramName.SEA_PEN_THUMBNAIL_CLICKED, templateIdForMetrics,
      VC_BACKGROUND_FREEFORM + 1);
}

export function logNumWordsInTextQuery(wordCount: number) {
  chrome.metricsPrivate.recordCount(
      HistogramName.SEA_PEN_WORD_COUNT, wordCount);
}
