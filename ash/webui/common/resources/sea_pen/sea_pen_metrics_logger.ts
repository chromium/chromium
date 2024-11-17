// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import {FreeformTab, QUERY, Query} from './constants.js';
import {SeaPenTemplateId} from './sea_pen_generated.mojom-webui.js';
import {SeaPenPaths} from './sea_pen_router_element.js';
import {SeaPenSamplePromptId} from './sea_pen_untranslated_constants.js';
import {isPersonalizationApp} from './sea_pen_utils.js';

const WALLPAPER_FREEFORM = 999;
const VC_BACKGROUND_FREEFORM = 1000;

const enum HistogramName {
  SEA_PEN_TEMPLATE_SUBPAGE = 'Ash.SeaPen.Template',
  SEA_PEN_THUMBNAIL_CLICKED = 'Ash.SeaPen.ThumbnailClicked',
  SEA_PEN_CREATE_BUTTON = 'Ash.SeaPen.CreateButton',
  SEA_PEN_WORD_COUNT = 'Ash.SeaPen.WordCount',
  SEA_PEN_SUGGESTION_CLICKED = `Ash.SeaPen.Freeform.Suggestion.Clicked`,
  SEA_PEN_SUGGESTION_SHUFFLE_CLICKED =
      `Ash.SeaPen.Freeform.Suggestion.Shuffle.Clicked`,
  SEA_PEN_SAMPLE_PROMPT_CLICKED =
      `Ash.SeaPen.Freeform.SamplePrompt.SampleClicked`,
  SEA_PEN_SAMPLE_PROMPT_SHUFFLE_CLICKED =
      `Ash.SeaPen.Freeform.SamplePrompt.Shuffle.Clicked`,
  SEA_PEN_FREEFORM_TAB_CLICKED = `Ash.SeaPen.Freeform.Tab.Clicked`,
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

export function logSeaPenVisited(path: SeaPenPaths) {
  const appName = isPersonalizationApp() ? 'Wallpaper' : 'VcBackground';
  const histogramName = path === SeaPenPaths.FREEFORM ?
      `Ash.SeaPen.Freeform.${appName}.Visited` :
      `Ash.SeaPen.${appName}.Visited`;
  chrome.metricsPrivate.recordBoolean(histogramName, true);
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

export function logSuggestionClicked() {
  chrome.metricsPrivate.recordBoolean(
      HistogramName.SEA_PEN_SUGGESTION_CLICKED, true);
}

export function logSuggestionShuffleClicked() {
  chrome.metricsPrivate.recordBoolean(
      HistogramName.SEA_PEN_SUGGESTION_SHUFFLE_CLICKED, true);
}

export function logSamplePromptClicked(id: SeaPenSamplePromptId) {
  chrome.metricsPrivate.recordEnumerationValue(
      HistogramName.SEA_PEN_SAMPLE_PROMPT_CLICKED, id,
      SeaPenSamplePromptId.MAX_VALUE);
}

export function logSamplePromptShuffleClicked() {
  chrome.metricsPrivate.recordBoolean(
      HistogramName.SEA_PEN_SAMPLE_PROMPT_SHUFFLE_CLICKED, true);
}

export function logSeaPenFreeformTabClicked(freeformTab: FreeformTab) {
  let enumValue;
  switch (freeformTab) {
    case FreeformTab.RESULTS:
      enumValue = 0;
      break;
    case FreeformTab.SAMPLE_PROMPTS:
      enumValue = 1;
      break;
  }
  chrome.metricsPrivate.recordEnumerationValue(
      HistogramName.SEA_PEN_FREEFORM_TAB_CLICKED, enumValue, /*enumSize=*/ 2);
}
