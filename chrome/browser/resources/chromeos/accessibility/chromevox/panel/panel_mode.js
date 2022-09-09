// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The modes that the Chromevox panel can be in.
 */

/**
 * @enum {string}
 */
export const PanelMode = {
  COLLAPSED: 'collapsed',
  FOCUSED: 'focused',
  FULLSCREEN_MENUS: 'menus',
  FULLSCREEN_TUTORIAL: 'tutorial',
  SEARCH: 'search',
};

/** @typedef {{title: string, location: (string|undefined)}} */
let PanelModeData;

/** @type {!Object<string, PanelModeData>} */
export const PanelModeInfo = {
  [PanelMode.COLLAPSED]: {title: 'panel_title', location: '#'},
  [PanelMode.FOCUSED]: {title: 'panel_title', location: '#focus'},
  [PanelMode.FULLSCREEN_MENUS]:
      {title: 'panel_menus_title', location: '#fullscreen'},
  [PanelMode.FULLSCREEN_TUTORIAL]:
      {title: 'panel_tutorial_title', location: '#fullscreen'},
  [PanelMode.SEARCH]: {title: 'panel_title', location: '#focus'},
};
