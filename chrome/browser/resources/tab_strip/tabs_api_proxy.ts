// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Tab, TabGroupVisualData} from './tab_strip.mojom-webui.js';
import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './tab_strip.mojom-webui.js';

/**
 * These values are persisted to logs and should not be renumbered or re-used.
 * See tools/metrics/histograms/enums.xml.
 */
export enum CloseTabAction {
  CLOSE_BUTTON = 0,
  SWIPED_TO_CLOSE = 1,
}

export interface TabsApiProxy {
  activateTab(tabId: number): void;

  /**
   * @return Object of group IDs as strings mapped to their visual data.
   */
  getGroupVisualData(): Promise<{data: {[id: string]: TabGroupVisualData}}>;

  getTabs(): Promise<{tabs: Tab[]}>;

  closeTab(tabId: number, closeTabAction: CloseTabAction): void;

  groupTab(tabId: number, groupId: string): void;

  moveGroup(groupId: string, newIndex: number): void;

  moveTab(tabId: number, newIndex: number): void;

  setThumbnailTracked(tabId: number, thumbnailTracked: boolean): void;

  ungroupTab(tabId: number): void;

  isVisible(): boolean;

  /**
   * @return Object with CSS variables as keys and pixel lengths as values
   */
  getLayout(): Promise<{layout: {[key: string]: string}}>;

  showEditDialogForGroup(
      groupId: string, locationX: number, locationY: number, width: number,
      height: number): void;

  showTabContextMenu(tabId: number, locationX: number, locationY: number): void;

  showBackgroundContextMenu(locationX: number, locationY: number): void;

  closeContainer(): void;

  /** @param durationMs Activation duration time in ms. */
  reportTabActivationDuration(durationMs: number): void;

  /**
   * @param tabCount Number of tabs.
   * @param durationMs Activation duration time in ms.
   */
  reportTabDataReceivedDuration(tabCount: number, durationMs: number): void;

  /**
   * @param tabCount Number of tabs.
   * @param durationMs Creation duration time in ms.
   */
  reportTabCreationDuration(tabCount: number, durationMs: number): void;

  getCallbackRouter(): PageCallbackRouter;
}

export class TabsApiProxyImpl implements TabsApiProxy {
  callbackRouter: PageCallbackRouter = new PageCallbackRouter();
  handler: PageHandlerRemote = new PageHandlerRemote();

  constructor() {
    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  activateTab(tabId: number) {
    this.handler.activateTab(tabId);
  }

  getGroupVisualData() {
    return this.handler.getGroupVisualData();
  }

  getTabs() {
    return this.handler.getTabs();
  }

  closeTab(tabId: number, closeTabAction: CloseTabAction) {
    this.handler.closeTab(
        tabId, closeTabAction === CloseTabAction.SWIPED_TO_CLOSE);
    chrome.metricsPrivate.recordEnumerationValue(
        'WebUITabStrip.CloseTabAction', closeTabAction,
        Object.keys(CloseTabAction).length);
  }

  groupTab(tabId: number, groupId: string) {
    this.handler.groupTab(tabId, groupId);
  }

  moveGroup(groupId: string, newIndex: number) {
    this.handler.moveGroup(groupId, newIndex);
  }

  moveTab(tabId: number, newIndex: number) {
    this.handler.moveTab(tabId, newIndex);
  }

  setThumbnailTracked(tabId: number, thumbnailTracked: boolean) {
    this.handler.setThumbnailTracked(tabId, thumbnailTracked);
  }

  ungroupTab(tabId: number) {
    this.handler.ungroupTab(tabId);
  }

  isVisible() {
    // TODO(crbug.com/40781526): Move this call out of tabs_api_proxy
    // since it's not related to tabs API.
    return document.visibilityState === 'visible';
  }

  getLayout() {
    return this.handler.getLayout();
  }

  showEditDialogForGroup(
      groupId: string, locationX: number, locationY: number, width: number,
      height: number) {
    this.handler.showEditDialogForGroup(
        groupId, locationX, locationY, width, height);
  }

  showTabContextMenu(tabId: number, locationX: number, locationY: number) {
    this.handler.showTabContextMenu(tabId, locationX, locationY);
  }

  showBackgroundContextMenu(locationX: number, locationY: number) {
    this.handler.showBackgroundContextMenu(locationX, locationY);
  }

  closeContainer() {
    this.handler.closeContainer();
  }

  reportTabActivationDuration(durationMs: number) {
    this.handler.reportTabActivationDuration(durationMs);
  }

  reportTabDataReceivedDuration(tabCount: number, durationMs: number) {
    this.handler.reportTabDataReceivedDuration(tabCount, durationMs);
  }

  reportTabCreationDuration(tabCount: number, durationMs: number) {
    this.handler.reportTabCreationDuration(tabCount, durationMs);
  }

  getCallbackRouter() {
    return this.callbackRouter;
  }

  static getInstance(): TabsApiProxy {
    return instance || (instance = new TabsApiProxyImpl());
  }

  static setInstance(obj: TabsApiProxy) {
    instance = obj;
  }
}

let instance: TabsApiProxy|null = null;
