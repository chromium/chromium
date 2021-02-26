// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @enum {number} */
export const BackgroundSelectionType = {
  NO_SELECTION: 0,
  NO_BACKGROUND: 1,
  IMAGE: 2,
  DAILY_REFRESH: 3,
};

/**
 * A user can make three types of background selections: no background, image
 * or daily refresh for a selected collection. The selection is tracked an
 * object of this type.
 * @typedef {{
 *   type: !BackgroundSelectionType,
 *   image: (!newTabPage.mojom.CollectionImage|undefined),
 *   dailyRefreshCollectionId: (string|undefined),
 * }}
 */
export let BackgroundSelection;

/** @enum {string} */
export const CustomizeDialogPage = {
  BACKGROUNDS: 'backgrounds',
  SHORTCUTS: 'shortcuts',
  MODULES: 'modules',
  THEMES: 'themes',
};
