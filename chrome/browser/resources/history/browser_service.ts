// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PageHandlerRemote} from 'chrome://resources/cr_components/history/history.mojom-webui.js';
import {PageCallbackRouter, PageHandler} from 'chrome://resources/cr_components/history/history.mojom-webui.js';
import {sendWithPromise} from 'chrome://resources/js/cr.js';

import type {ForeignSession} from './externs.js';
import type {HistoryIdentityState} from './externs.js';

export type RemoveVisitsRequest = Array<{
  url: string,
  timestamps: number[],
}>;

/**
 * @fileoverview Defines a singleton object, history.BrowserService, which
 * provides access to chrome.send APIs.
 */

export interface BrowserService {
  handler: PageHandlerRemote;
  callbackRouter: PageCallbackRouter;
  getForeignSessions(): Promise<ForeignSession[]>;
  openForeignSessionAllTabs(sessionTag: string): void;
  openForeignSessionTab(sessionTag: string, tabId: number, e: MouseEvent): void;
  deleteForeignSession(sessionTag: string): void;
  recordHistogram(histogram: string, value: number, max: number): void;
  recordBooleanHistogram(histogram: string, value: boolean): void;
  recordAction(action: string): void;
  recordTime(histogram: string, time: number): void;
  recordLongTime(histogram: string, time: number): void;
  recordSigninPendingOffered(): void;
  navigateToUrl(url: string, target: string, e: MouseEvent): void;
  otherDevicesInitialized(): void;
  getInitialIdentityState(): Promise<HistoryIdentityState>;
  startTurnOnSyncFlow(): void;
}

export class BrowserServiceImpl implements BrowserService {
  handler: PageHandlerRemote;
  callbackRouter: PageCallbackRouter;

  constructor(handler: PageHandlerRemote, callbackRouter: PageCallbackRouter) {
    this.handler = handler;
    this.callbackRouter = callbackRouter;
  }

  static getInstance(): BrowserService {
    if (instance) {
      return instance;
    }

    const handler = PageHandler.getRemote();
    const callbackRouter = new PageCallbackRouter();
    handler.setPage(callbackRouter.$.bindNewPipeAndPassRemote());
    return instance = new BrowserServiceImpl(handler, callbackRouter);
  }

  static setInstance(obj: BrowserService) {
    instance = obj;
  }

  getForeignSessions() {
    return sendWithPromise('getForeignSessions');
  }

  openForeignSessionAllTabs(sessionTag: string) {
    chrome.send('openForeignSessionAllTabs', [sessionTag]);
  }

  openForeignSessionTab(sessionTag: string, tabId: number, e: MouseEvent) {
    chrome.send('openForeignSessionTab', [
      sessionTag,
      String(tabId),
      e.button || 0,
      e.altKey,
      e.ctrlKey,
      e.metaKey,
      e.shiftKey,
    ]);
  }

  deleteForeignSession(sessionTag: string) {
    chrome.send('deleteForeignSession', [sessionTag]);
  }

  recordHistogram(histogram: string, value: number, max: number) {
    chrome.send('metricsHandler:recordInHistogram', [histogram, value, max]);
  }

  recordBooleanHistogram(histogram: string, value: boolean): void {
    chrome.metricsPrivate.recordBoolean(histogram, value);
  }

  /**
   * Record an action in UMA.
   * @param action The name of the action to be logged.
   */
  recordAction(action: string) {
    if (action.indexOf('_') === -1) {
      action = `HistoryPage_${action}`;
    }
    chrome.send('metricsHandler:recordAction', [action]);
  }

  recordTime(histogram: string, time: number) {
    chrome.send('metricsHandler:recordTime', [histogram, time]);
  }

  recordLongTime(histogram: string, time: number) {
    // It's a bit odd that this is the only one to use chrome.metricsPrivate,
    // but that's because the other code predates chrome.metricsPrivate.
    // In any case, the MetricsHandler doesn't support long time histograms.
    chrome.metricsPrivate.recordLongTime(histogram, time);
  }

  recordSigninPendingOffered() {
    chrome.send('recordSigninPendingOffered');
  }

  navigateToUrl(url: string, target: string, e: MouseEvent) {
    chrome.send(
        'navigateToUrl',
        [url, target, e.button, e.altKey, e.ctrlKey, e.metaKey, e.shiftKey]);
  }

  otherDevicesInitialized() {
    chrome.send('otherDevicesInitialized');
  }

  getInitialIdentityState(): Promise<HistoryIdentityState> {
    return sendWithPromise('getInitialIdentityState');
  }

  startTurnOnSyncFlow() {
    chrome.send('startTurnOnSyncFlow');
  }
}

let instance: BrowserService|null = null;
