// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ForeignSessionPageHandlerRemote} from 'chrome://resources/cr_components/history/foreign_sessions.mojom-webui.js';
import {ForeignSessionPageCallbackRouter, ForeignSessionPageHandler} from 'chrome://resources/cr_components/history/foreign_sessions.mojom-webui.js';
import type {PageHandlerRemote} from 'chrome://resources/cr_components/history/history.mojom-webui.js';
import {PageCallbackRouter, PageHandler} from 'chrome://resources/cr_components/history/history.mojom-webui.js';
import {sendWithPromise} from 'chrome://resources/js/cr.js';
import type {ClickModifiers} from 'chrome://resources/mojo/ui/base/mojom/window_open_disposition.mojom-webui.js';

import type {ForeignSession, HistoryIdentityState} from './externs.js';

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
  foreignSessionHandler: ForeignSessionPageHandlerRemote;
  foreignSessionCallbackRouter: ForeignSessionPageCallbackRouter;
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
  foreignSessionHandler: ForeignSessionPageHandlerRemote;
  foreignSessionCallbackRouter: ForeignSessionPageCallbackRouter;

  constructor(
      handler: PageHandlerRemote, callbackRouter: PageCallbackRouter,
      foreignSessionHandler: ForeignSessionPageHandlerRemote,
      foreignSessionCallbackRouter: ForeignSessionPageCallbackRouter) {
    this.handler = handler;
    this.callbackRouter = callbackRouter;
    this.foreignSessionHandler = foreignSessionHandler;
    this.foreignSessionCallbackRouter = foreignSessionCallbackRouter;
  }

  static getInstance(): BrowserService {
    if (instance) {
      return instance;
    }

    const handler = PageHandler.getRemote();
    const callbackRouter = new PageCallbackRouter();
    handler.setPage(callbackRouter.$.bindNewPipeAndPassRemote());

    const foreignSessionHandler = ForeignSessionPageHandler.getRemote();
    const foreignSessionCallbackRouter = new ForeignSessionPageCallbackRouter();
    foreignSessionHandler.setPage(
        foreignSessionCallbackRouter.$.bindNewPipeAndPassRemote());

    return instance = new BrowserServiceImpl(
               handler, callbackRouter, foreignSessionHandler,
               foreignSessionCallbackRouter);
  }

  static setInstance(obj: BrowserService) {
    instance = obj;
  }

  getForeignSessions() {
    return this.foreignSessionHandler.getForeignSessions().then(
        (result: {sessions: ForeignSession[]}) => result.sessions);
  }

  openForeignSessionAllTabs(sessionTag: string) {
    this.foreignSessionHandler.openForeignSessionAllTabs(sessionTag);
  }

  openForeignSessionTab(sessionTag: string, tabId: number, e: MouseEvent) {
    const modifiers: ClickModifiers = {
      middleButton: e.button === 1,
      altKey: e.altKey,
      ctrlKey: e.ctrlKey,
      metaKey: e.metaKey,
      shiftKey: e.shiftKey,
    };
    this.foreignSessionHandler.openForeignSessionTab(
        sessionTag, tabId, modifiers);
  }

  deleteForeignSession(sessionTag: string) {
    this.foreignSessionHandler.deleteForeignSession(sessionTag);
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
