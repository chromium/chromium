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
  private static panelIsInitialized_: Promise<void>|null = null;

  type: PanelCommandType;
  data?: string|Object;

  constructor(type: PanelCommandType, data?: string|Object) {
    this.type = type;
    this.data = data;

    if (!PanelCommand.panelIsInitialized_) {
      PanelCommand.panelIsInitialized_ = new Promise(resolve => {
        this.waitForPanel(resolve);
      });
    }
  }

  waitForPanel(resolve: () => void) {
    chrome.runtime.sendMessage(
        undefined, {command: PanelCommandType.IS_PANEL_INITIALIZED}, undefined,
        (initialized: any) => {
          // Panel is not yet initialized
          if (chrome.runtime.lastError) {
            setTimeout(() => this.waitForPanel(resolve), 500);
            return;
          }

          // Panel is initialized.
          if (initialized as boolean) {
            resolve();
          }
        });
  }

  /** Send this command to the ChromeVox Panel window. */
  async send(): Promise<void> {
    // Do not send commands to the ChromeVox Panel window until it has finished
    // loading and is ready to receive commands.
    await PanelCommand.panelIsInitialized_;

    chrome.runtime.sendMessage(undefined, {type: this.type, data: this.data});
  }
}

/**
 * The types of commands that can be sent between the panel popup and the
 * ChromeVox service worker.
 */
export enum PanelCommandType {
  CLEAR_SPEECH = 'clear_speech',
  ADD_NORMAL_SPEECH = 'add_normal_speech',
  ADD_ANNOTATION_SPEECH = 'add_annotation_speech',
  BRAILLE_ROUTE = 'braille_route',
  CLOSE_CHROMEVOX = 'close_chromevox',
  UPDATE_BRAILLE = 'update_braille',
  OPEN_MENUS = 'open_menus',
  OPEN_MENUS_MOST_RECENT = 'open_menus_most_recent',
  SEARCH = 'search',
  TUTORIAL = 'tutorial',
  IS_PANEL_INITIALIZED = 'IsPanelInitialized',
}

/**
 * The types of commands that can be sent between the panel popup and the
 * ChromeVox service worker for testing purposes.
 */
export enum TestPanelCommandType {
  BRAILLE_PAN_RIGHT = 'braille_pan_right',
  BRAILLE_PAN_LEFT = 'braille_pan_left',
  DISABLE_ERROR_MSG = 'disable_error_msg',
  FIRE_MOCK_EVENT = 'fire_mock_event',
  FIRE_MOCK_QUERY = 'fire_mock_query',
  GET_ACTIVE_MENU_DATA = 'get_active_menu_data',
  GET_ACTIVE_SEARCH_MENU_DATA = 'get_active_search_menu_data'
}

TestImportManager.exportForTesting(
    PanelCommand, ['PanelCommandType', PanelCommandType],
    ['TestPanelCommandType', TestPanelCommandType]);
