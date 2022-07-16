// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote, Tab, TabGroupVisualData} from './tab_strip.mojom-webui.js';

/**
 * These values are persisted to logs and should not be renumbered or re-used.
 * See tools/metrics/histograms/enums.xml.
 * @enum {number}
 */
export const CloseTabAction = {
  CLOSE_BUTTON: 0,
  SWIPED_TO_CLOSE: 1,
};

/** @typedef {!Tab} */
export let ExtensionsApiTab;

/** @interface */
export class TabsApiProxy {
  /**
   * @param {number} tabId
   * @return {!Promise<!ExtensionsApiTab>}
   */
  activateTab(tabId) {}

  /**
   * @return {!Promise<{data: !Object<!TabGroupVisualData>}>} Object of group
   *     IDs as strings mapped to their visual data.
   */
  getGroupVisualData() {}

  /**
   * @return {!Promise<{tabs: !Array<!Tab>}>}
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


  /** @return {boolean} */
  isVisible() {}

  /**
   * @return {!Promise<{colors: !Object<string, string>}>} Object with CSS
   *     variables as keys and rgba strings as values
   */
  getColors() {}

  /**
   * @return {!Promise<{layout: !Object<string, string>}>} Object with CSS
   *     variables as keys and pixel lengths as values
   */
  getLayout() {}

  observeThemeChanges() {}

  /**
   * @param {string} groupId
   * @param {number} locationX
   * @param {number} locationY
   * @param {number} width
   * @param {number} height
   */
  showEditDialogForGroup(groupId, locationX, locationY, width, height) {}

  /**
   * @param {number} tabId
   * @param {number} locationX
   * @param {number} locationY
   */
  showTabContextMenu(tabId, locationX, locationY) {}

  /**
   * @param {number} locationX
   * @param {number} locationY
   */
  showBackgroundContextMenu(locationX, locationY) {}

  closeContainer() {}

  /** @param {number} durationMs Activation duration time in ms. */
  reportTabActivationDuration(durationMs) {}

  /**
   * @param {number} tabCount Number of tabs.
   * @param {number} durationMs Activation duration time in ms.
   */
  reportTabDataReceivedDuration(tabCount, durationMs) {}

  /**
   * @param {number} tabCount Number of tabs.
   * @param {number} durationMs Creation duration time in ms.
   */
  reportTabCreationDuration(tabCount, durationMs) {}

  /** @return {!PageCallbackRouter} */
  getCallbackRouter() {}
}

/** @implements {TabsApiProxy} */
export class TabsApiProxyImpl {
  constructor() {
    /** @type {!PageCallbackRouter} */
    this.callbackRouter = new PageCallbackRouter();

    /** @type {!PageHandlerRemote} */
    this.handler = new PageHandlerRemote();

    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  /** @override */
  activateTab(tabId) {
    return new Promise(resolve => {
      chrome.tabs.update(tabId, {active: true}, resolve);
    });
  }

  /** @override */
  getGroupVisualData() {
    return this.handler.getGroupVisualData();
  }

  /** @override */
  getTabs() {
    return this.handler.getTabs();
  }

  /** @override */
  closeTab(tabId, closeTabAction) {
    this.handler.closeTab(
        tabId, closeTabAction === CloseTabAction.SWIPED_TO_CLOSE);
    chrome.metricsPrivate.recordEnumerationValue(
        'WebUITabStrip.CloseTabAction', closeTabAction,
        Object.keys(CloseTabAction).length);
  }

  /** @override */
  groupTab(tabId, groupId) {
    this.handler.groupTab(tabId, groupId);
  }

  /** @override */
  moveGroup(groupId, newIndex) {
    this.handler.moveGroup(groupId, newIndex);
  }

  /** @override */
  moveTab(tabId, newIndex) {
    this.handler.moveTab(tabId, newIndex);
  }

  /** @override */
  setThumbnailTracked(tabId, thumbnailTracked) {
    this.handler.setThumbnailTracked(tabId, thumbnailTracked);
  }

  /** @override */
  ungroupTab(tabId) {
    this.handler.ungroupTab(tabId);
  }

  /** @override */
  isVisible() {
    // TODO(crbug.com/1234500): Move this call out of tabs_api_proxy
    // since it's not related to tabs API.
    return document.visibilityState === 'visible';
  }

  /** @override */
  getColors() {
    return this.handler.getThemeColors();
  }

  /** @override */
  getLayout() {
    return this.handler.getLayout();
  }

  /** @override */
  observeThemeChanges() {
    // TODO(crbug.com/1234500): Migrate to mojo as well.
    chrome.send('observeThemeChanges');
  }

  /** @override */
  showEditDialogForGroup(groupId, locationX, locationY, width, height) {
    this.handler.showEditDialogForGroup(
        groupId, locationX, locationY, width, height);
  }

  /** @override */
  showTabContextMenu(tabId, locationX, locationY) {
    this.handler.showTabContextMenu(tabId, locationX, locationY);
  }

  /** @override */
  showBackgroundContextMenu(locationX, locationY) {
    this.handler.showBackgroundContextMenu(locationX, locationY);
  }

  /** @override */
  closeContainer() {
    this.handler.closeContainer();
  }

  /** @override */
  reportTabActivationDuration(durationMs) {
    this.handler.reportTabActivationDuration(durationMs);
  }

  /** @override */
  reportTabDataReceivedDuration(tabCount, durationMs) {
    this.handler.reportTabDataReceivedDuration(tabCount, durationMs);
  }

  /** @override */
  reportTabCreationDuration(tabCount, durationMs) {
    this.handler.reportTabCreationDuration(tabCount, durationMs);
  }

  /** @override */
  getCallbackRouter() {
    return this.callbackRouter;
  }
}

addSingletonGetter(TabsApiProxyImpl);
