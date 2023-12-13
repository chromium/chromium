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
  MAX_VALUE,
}

export function recordCustomizeChromeAction(action: CustomizeChromeAction) {
  chrome.metricsPrivate.recordEnumerationValue(
      'NewTabPage.CustomizeChromeSidePanelAction', action,
      CustomizeChromeAction.MAX_VALUE);
}
