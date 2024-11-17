// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Customize Chrome actions. This enum must match the numbering for
 * NTPCustomizeChromeSidePanelAction in enums.xml. These values are persisted
 * to logs. Entries should not be renumbered, removed or reused.
 *
 * MAX_VALUE should always be at the end to help get the current number of
 * buckets.
 */
export enum CustomizeChromeAction {
  EDIT_THEME_CLICKED,
  CATEGORIES_DEFAULT_CHROME_SELECTED,
  CATEGORIES_UPLOAD_IMAGE_SELECTED,
  CATEGORIES_WALLPAPER_SEARCH_SELECTED,
  WALLPAPER_SEARCH_PROMPT_SUBMITTED,
  WALLPAPER_SEARCH_RESULT_IMAGE_SELECTED,
  WALLPAPER_SEARCH_HISTORY_IMAGE_SELECTED,
  CATEGORIES_FIRST_PARTY_COLLECTION_SELECTED,
  FIRST_PARTY_COLLECTION_THEME_SELECTED,
  WALLPAPER_SEARCH_THUMBS_UP_SELECTED,
  WALLPAPER_SEARCH_THUMBS_DOWN_SELECTED,
  WALLPAPER_SEARCH_SUBJECT_DESCRIPTOR_UPDATED,
  WALLPAPER_SEARCH_STYLE_DESCRIPTOR_UPDATED,
  WALLPAPER_SEARCH_MOOD_DESCRIPTOR_UPDATED,
  WALLPAPER_SEARCH_COLOR_DESCRIPTOR_UPDATED,
  WALLPAPER_SEARCH_INSPIRATION_THEME_SELECTED,
  SET_CLASSIC_CHROME_THEME_CLICKED,
  SHOW_SHORTCUTS_TOGGLE_CLICKED,
  SHOW_CARDS_TOGGLE_CLICKED,
  WALLPAPER_SEARCH_APPEARANCE_BUTTON_CLICKED,
  DEFAULT_COLOR_CLICKED,
  CHROME_COLOR_CLICKED,
  CUSTOM_COLOR_CLICKED,
  MAX_VALUE = CUSTOM_COLOR_CLICKED,
}

export function recordCustomizeChromeAction(action: CustomizeChromeAction) {
  chrome.metricsPrivate.recordEnumerationValue(
      'NewTabPage.CustomizeChromeSidePanelAction', action,
      CustomizeChromeAction.MAX_VALUE + 1);
}

/**
 * Customize Chrome impressions. This enum must match the numbering for
 * NTPCustomizeChromeSidePanelImpression in enums.xml. These values are
 * persisted to logs. Entries should not be renumbered, removed or reused.
 *
 * MAX_VALUE should always be at the end to help get the current number of
 * buckets.
 */
export enum CustomizeChromeImpression {
  EXTENSIONS_CARD_SECTION_DISPLAYED,
  MAX_VALUE = EXTENSIONS_CARD_SECTION_DISPLAYED,
}

export function recordCustomizeChromeImpression(
    action: CustomizeChromeImpression) {
  chrome.metricsPrivate.recordEnumerationValue(
      'NewTabPage.CustomizeChromeSidePanelImpression', action,
      CustomizeChromeImpression.MAX_VALUE + 1);
}

/**
 * Types of images that are shown on the NTP (and therefore also appear in
 * Customize Chrome). This enum must match the numbering for NtpImageType in
 * enums.xml. These values are persisted to logs. Entries should not be
 * renumbered, removed or reused.
 *
 * MAX_VALUE should always be at the end to help get the current number of
 * buckets.
 */
export enum NtpImageType {
  BACKGROUND_IMAGE,
  COLLECTIONS,
  COLLECTION_IMAGES,
  MAX_VALUE = COLLECTION_IMAGES,
}

export function recordCustomizeChromeImageError(imageType: NtpImageType) {
  chrome.metricsPrivate.recordEnumerationValue(
      'NewTabPage.BackgroundService.Images.Headers.ErrorDetected', imageType,
      NtpImageType.MAX_VALUE + 1);
}
