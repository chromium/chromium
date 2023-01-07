// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Calculates the menu items for the tabs menu in the ChromeVox
 * panel.
 */

import {Msgs} from '../../common/msgs.js';
import {PanelTabMenuItemData} from '../../common/panel_menu_data.js';

export class PanelTabMenuBackground {
  /**
   * @param {number} windowId
   * @param {number} tabId
   * @return {!Promise}
   */
  static async focusTab(windowId, tabId) {
    await new Promise(
        resolve => chrome.windows.update(windowId, {focused: true}, resolve));
    await new Promise(
        resolve => chrome.tabs.update(tabId, {active: true}, resolve));
  }

  /** @return {!Promise<!Array<!PanelTabMenuItemData>>} */
  static async getTabMenuData() {
    const menuData = [];
    const lastFocusedWindow =
        await new Promise(resolve => chrome.windows.getLastFocused(resolve));
    const windows = await new Promise(
        resolve => chrome.windows.getAll({populate: true}, resolve));
    for (const w of windows) {
      const windowId = w.id;
      for (const tab of w.tabs) {
        const tabId = tab.id;
        let title = tab.title;
        if (tab.active && windowId === lastFocusedWindow.id) {
          title += ' ' + Msgs.getMsg('active_tab');
        }
        menuData.push({title, windowId, tabId});
      }
    }
    return menuData;
  }
}
