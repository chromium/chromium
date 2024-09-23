// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The implementation of Desk API
 */

import {DeskApi, LaunchOptions, RemoveDeskOptions, WindowProperties} from './types.js';


/**
 * Provides the implementation for Desk API.
 */
export class DeskApiImpl implements DeskApi {
  launchDesk(
      options: LaunchOptions, callback: chrome.wmDesksPrivate.DeskIdCallback) {
    if (typeof chrome.wmDesksPrivate.launchDesk !== 'function') {
      throw new Error('launchDesk is not supported in this version.');
    }
    chrome.wmDesksPrivate.launchDesk(options, callback);
  }
  removeDesk(
      deskUuid: string, options: RemoveDeskOptions,
      callback: chrome.wmDesksPrivate.VoidCallback) {
    if (typeof chrome.wmDesksPrivate.removeDesk !== 'function') {
      throw new Error('removeDesk is not supported in this version.');
    }
    chrome.wmDesksPrivate.removeDesk(deskUuid, options, callback);
  }
  setWindowProperties(
      windowId: number, windowProperties: WindowProperties,
      callback: chrome.wmDesksPrivate.VoidCallback) {
    if (typeof chrome.wmDesksPrivate.setWindowProperties !== 'function') {
      throw new Error('setWindowProperties is not supported in this version.');
    }
    chrome.wmDesksPrivate.setWindowProperties(
        windowId, windowProperties, callback);
  }
  getActiveDesk(callback: chrome.wmDesksPrivate.DeskIdCallback) {
    if (typeof chrome.wmDesksPrivate.getActiveDesk !== 'function') {
      throw new Error('getActiveDesk is not supported in this version.');
    }
    chrome.wmDesksPrivate.getActiveDesk(callback);
  }

  switchDesk(deskId: string, callback: chrome.wmDesksPrivate.VoidCallback) {
    if (typeof chrome.wmDesksPrivate.switchDesk !== 'function') {
      throw new Error('switchDesk is not supported in this version.');
    }
    chrome.wmDesksPrivate.switchDesk(deskId, callback);
  }

  getDeskById(
      deskId: string, callback: chrome.wmDesksPrivate.GetDeskByIdCallback) {
    if (typeof chrome.wmDesksPrivate.getDeskByID !== 'function') {
      throw new Error('getDeskByID is not supported in this version.');
    }
    chrome.wmDesksPrivate.getDeskByID(deskId, callback);
  }

  addDeskAddedListener(callback: chrome.wmDesksPrivate.DeskAddCallback) {
    if (!chrome.wmDesksPrivate.OnDeskAdded) {
      throw new Error('OnDeskAdded is not supported in this version.');
    }
    chrome.wmDesksPrivate.OnDeskAdded.addListener(callback);
  }


  addDeskRemovedListener(callback: chrome.wmDesksPrivate.DeskIdCallback) {
    if (!chrome.wmDesksPrivate.OnDeskRemoved) {
      throw new Error('OnDeskRemoved is not supported in this version.');
    }
    chrome.wmDesksPrivate.OnDeskRemoved.addListener(callback);
  }

  addDeskSwitchedListener(callback: chrome.wmDesksPrivate.DeskSwitchCallback) {
    if (!chrome.wmDesksPrivate.OnDeskSwitched) {
      throw new Error('OnDeskSwitched is not supported in this version.');
    }
    chrome.wmDesksPrivate.OnDeskSwitched.addListener(callback);
  }
}
