// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';

/**
 * These values are persisted to logs and should not be renumbered or re-used.
 * See tools/metrics/histograms/enums.xml.
 * @enum {number}
 */
export const CloseTabAction = {
  CLOSE_BUTTON: 0,
  SWIPED_TO_CLOSE: 1,
};

/**
 * Must be kept in sync with TabNetworkState from
 * //chrome/browser/ui/tabs/tab_network_state.h.
 * @enum {number}
 */
export const TabNetworkState = {
  NONE: 0,
  WAITING: 1,
  LOADING: 2,
  ERROR: 3,
};

/**
 * Must be kept in sync with TabAlertState from
 * //chrome/browser/ui/tabs/tab_utils.h
 * @enum {string}
 */
export const TabAlertState = {
  AUDIO_MUTING: 'audio-muting',
  AUDIO_PLAYING: 'audio-playing',
  BLUETOOTH_CONNECTED: 'bluetooth-connected',
  DESKTOP_CAPTURING: 'desktop-capturing',
  HID_CONNECTED: 'hid-connected',
  MEDIA_RECORDING: 'media-recording',
  PIP_PLAYING: 'pip-playing',
  SERIAL_CONNECTED: 'serial-connected',
  TAB_CAPTURING: 'tab-capturing',
  USB_CONNECTED: 'usb-connected',
  VR_PRESENTING_IN_HEADSET: 'vr-presenting',
};

/**
 * @typedef {{
 *    active: boolean,
 *    alertStates: !Array<!TabAlertState>,
 *    blocked: boolean,
 *    crashed: boolean,
 *    favIconUrl: (string|undefined),
 *    groupId: (string|undefined),
 *    id: number,
 *    index: number,
 *    isDefaultFavicon: boolean,
 *    networkState: !TabNetworkState,
 *    pinned: boolean,
 *    shouldHideThrobber: boolean,
 *    showIcon: boolean,
 *    title: string,
 *    url: string,
 * }}
 */
export let TabData;

/** @typedef {!Tab} */
export let ExtensionsApiTab;

/**
 * @typedef {{
 *   color: string,
 *   textColor: string,
 *   title: string,
 * }}
 */
export let TabGroupVisualData;

/** @interface */
export class TabsApiProxy {
  /**
   * @param {number} tabId
   * @return {!Promise<!ExtensionsApiTab>}
   */
  activateTab(tabId) {}

  /**
   * @return {!Promise<!Object<!TabGroupVisualData>>} Object of group IDs as
   *     strings mapped to their visual data.
   */
  getGroupVisualData() {}

  /**
   * @return {!Promise<!Array<!TabData>>}
   */
  getTabs() {}

  /**
   * @param {number} tabId
   * @param {!CloseTabAction} closeTabAction
   */
  closeTab(tabId, closeTabAction) {}

  /**
   * @param {number} tabId
   * @param {string} groupId
   */
  groupTab(tabId, groupId) {}

  /**
   * @param {string} groupId
   * @param {number} newIndex
   */
  moveGroup(groupId, newIndex) {}

  /**
   * @param {number} tabId
   * @param {number} newIndex
   */
  moveTab(tabId, newIndex) {}

  /**
   * @param {number} tabId
   * @param {boolean} thumbnailTracked
   */
  setThumbnailTracked(tabId, thumbnailTracked) {}

  /** @param {number} tabId */
  ungroupTab(tabId) {}
}

/** @implements {TabsApiProxy} */
export class TabsApiProxyImpl {
  /** @override */
  activateTab(tabId) {
    return new Promise(resolve => {
      chrome.tabs.update(tabId, {active: true}, resolve);
    });
  }

  /** @override */
  getGroupVisualData() {
    return sendWithPromise('getGroupVisualData');
  }

  /** @override */
  getTabs() {
    return sendWithPromise('getTabs');
  }

  /** @override */
  closeTab(tabId, closeTabAction) {
    chrome.send(
        'closeTab', [tabId, closeTabAction === CloseTabAction.SWIPED_TO_CLOSE]);
    chrome.metricsPrivate.recordEnumerationValue(
        'WebUITabStrip.CloseTabAction', closeTabAction,
        Object.keys(CloseTabAction).length);
  }

  /** @override */
  groupTab(tabId, groupId) {
    chrome.send('groupTab', [tabId, groupId]);
  }

  /** @override */
  moveGroup(groupId, newIndex) {
    chrome.send('moveGroup', [groupId, newIndex]);
  }

  /** @override */
  moveTab(tabId, newIndex) {
    chrome.send('moveTab', [tabId, newIndex]);
  }

  /** @override */
  setThumbnailTracked(tabId, thumbnailTracked) {
    chrome.send('setThumbnailTracked', [tabId, thumbnailTracked]);
  }

  /** @override */
  ungroupTab(tabId) {
    chrome.send('ungroupTab', [tabId]);
  }
}

addSingletonGetter(TabsApiProxyImpl);
