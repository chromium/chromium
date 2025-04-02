// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Commands to pass from the ChromeVox background page context
 * to the ChromeVox Panel.
 */
import {TestImportManager} from '/common/testing/test_import_manager.js';

/**
 * Create one command to pass to the ChromeVox Panel.
 */
export class PanelCommand {
  type: PanelCommandType;
  data?: string|Object;

  constructor(type: PanelCommandType, data?: string|Object) {
    this.type = type;
    this.data = data;
  }

  getPanelWindow(): Window {
    const views = chrome.extension.getViews();
    for (let i = 0; i < views.length; i++) {
      if (views[i]['location'].href.indexOf('panel/panel.html') > 0) {
        return views[i] as Window;
      }
    }
    throw new Error('Could not find the panel window');
  }

  waitForPanel(): Promise<void> {
    return new Promise<void>(resolve => {
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

  /** Send this command to the ChromeVox Panel window. */
  async send(): Promise<void> {
    // Do not send commands to the ChromeVox Panel window until it has finished
    // loading and is ready to receive commands.
    await this.waitForPanel();
    const panelWindow = this.getPanelWindow();
    panelWindow.postMessage(JSON.stringify(this), window.location.origin);
  }
}

export enum PanelCommandType {
  CLEAR_SPEECH = 'clear_speech',
  ADD_NORMAL_SPEECH = 'add_normal_speech',
  ADD_ANNOTATION_SPEECH = 'add_annotation_speech',
  CLOSE_CHROMEVOX = 'close_chromevox',
  UPDATE_BRAILLE = 'update_braille',
  OPEN_MENUS = 'open_menus',
  OPEN_MENUS_MOST_RECENT = 'open_menus_most_recent',
  SEARCH = 'search',
  TUTORIAL = 'tutorial',
  ENABLE_TEST_HOOKS = 'enable_test_hooks',
}

TestImportManager.exportForTesting(
    PanelCommand, ['PanelCommandType', PanelCommandType]);
