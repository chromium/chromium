// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Commands to pass from the ChromeVox background page context
 * to the ChromeVox Panel.
 */

/**
 * Create one command to pass to the ChromeVox Panel.
 */
export class PanelCommand {
  /**
   * @param {PanelCommandType} type The type of command.
   * @param {string|{groups:Array}=} opt_data
   *     Optional data associated with the command.
   */
  constructor(type, opt_data) {
    this.type = type;
    this.data = opt_data;
  }

  /**
   * @return {Window}
   */
  getPanelWindow() {
    const views = chrome.extension.getViews();
    for (let i = 0; i < views.length; i++) {
      if (views[i].location.href.indexOf('panel/panel.html') > 0) {
        return views[i];
      }
    }
    throw new Error('Could not find the panel window');
  }

  waitForPanel() {
    return new Promise(resolve => {
      const panelWindow = this.getPanelWindow();
      if (panelWindow.document.readyState === 'complete') {
        // The panel may already have loaded. In this case, resolve() and
        // do not wait for a load event that has already fired.
        resolve();
      }
      panelWindow.addEventListener('load', () => {
        resolve();
      });
    });
  }

  /**
   * Send this command to the ChromeVox Panel window.
   * @return {!Promise}
   */
  async send() {
    // Do not send commands to the ChromeVox Panel window until it has finished
    // loading and is ready to receive commands.
    await this.waitForPanel();
    const panelWindow = this.getPanelWindow();
    panelWindow.postMessage(JSON.stringify(this), window.location.origin);
  }
}


/**
 * Possible panel commands.
 * @enum {string}
 */
export const PanelCommandType = {
  CLEAR_SPEECH: 'clear_speech',
  ADD_NORMAL_SPEECH: 'add_normal_speech',
  ADD_ANNOTATION_SPEECH: 'add_annotation_speech',
  CLOSE_CHROMEVOX: 'close_chromevox',
  UPDATE_BRAILLE: 'update_braille',
  OPEN_MENUS: 'open_menus',
  OPEN_MENUS_MOST_RECENT: 'open_menus_most_recent',
  SEARCH: 'search',
  TUTORIAL: 'tutorial',
  ENABLE_TEST_HOOKS: 'enable_test_hooks',
};
