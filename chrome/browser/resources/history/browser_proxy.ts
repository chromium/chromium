// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PageHandlerRemote, TimePeriodOption} from 'chrome://resources/cr_components/history/history.mojom-webui.js';
import {PageCallbackRouter, PageHandler} from 'chrome://resources/cr_components/history/history.mojom-webui.js';

export type RemoveVisitsRequest = Array<{
  url: string,
  timestamps: number[],
}>;

export interface BrowserProxy {
  handler: PageHandlerRemote;
  callbackRouter: PageCallbackRouter;
  // TODO(crbug.com/495832119): Change all the methods that use `chrome.send` to
  // use mojo instead, or move them into a separate class, then auto-generate
  // this interface (see crrev.com/c/7877444 for an example).
  recordHistogram(histogram: string, value: number, max: number): void;
  recordBooleanHistogram(histogram: string, value: boolean): void;
  recordAction(action: string): void;
  recordTime(histogram: string, time: number): void;
  recordLongTime(histogram: string, time: number): void;
  getTimePeriodOptions(): Promise<{options: TimePeriodOption[]}>;
}

export class BrowserProxyImpl implements BrowserProxy {
  handler: PageHandlerRemote;
  callbackRouter: PageCallbackRouter;

  constructor() {
    this.handler = PageHandler.getRemote();
    this.callbackRouter = new PageCallbackRouter();
    this.handler.setPage(this.callbackRouter.$.bindNewPipeAndPassRemote());
  }

  recordHistogram(histogram: string, value: number, max: number) {
    chrome.send('metricsHandler:recordInHistogram', [histogram, value, max]);
  }

  recordBooleanHistogram(histogram: string, value: boolean): void {
    chrome.metricsPrivate.recordBoolean(histogram, value);
  }

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
    chrome.metricsPrivate.recordLongTime(histogram, time);
  }

  getTimePeriodOptions() {
    return this.handler.getTimePeriodOptions();
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxyImpl());
  }

  static setInstance(obj: BrowserProxy) {
    instance = obj;
  }
}

let instance: BrowserProxy|null = null;
