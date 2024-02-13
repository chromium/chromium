// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import type {PageHandlerInterface} from './access_code_cast.mojom-webui.js';
import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './access_code_cast.mojom-webui.js';

declare const chrome: {
  send(message: string, params?: any[]): void,
  getVariableValue(variable: string): string,
};

const HISTOGRAM_ACCESS_CODE_INPUT_TIME =
    'AccessCodeCast.Ui.AccessCodeInputTime';
const HISTOGRAM_CAST_ATTEMPT_LENGTH = 'AccessCodeCast.Ui.CastAttemptLength';
const HISTOGRAM_DIALOG_CLOSE_REASON = 'AccessCodeCast.Ui.DialogCloseReason';

/**
 * This enum is tied directly to a UMA enum defined in
 * //tools/metrics/histograms/enums.xml, and should always reflect it (do not
 * change one without changing the other).
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 */
export enum DialogCloseReason {
  LOST_FOCUS = 0,
  CANCEL_BUTTON = 1,
  CAST_SUCCESS = 2,
  // Leave this at the end.
  COUNT = 3
}

export class BrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: PageHandlerInterface;

  constructor(omitHandler?: boolean) {
    if (omitHandler) {
      return;
    }

    this.callbackRouter = new PageCallbackRouter();
    this.handler = new PageHandlerRemote();

    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        (this.handler as PageHandlerRemote).$.bindNewPipeAndPassReceiver());
  }

  closeDialog() {
    chrome.send('dialogClose');
  }

  getDialogArgs() {
    return JSON.parse(chrome.getVariableValue('dialogArguments'));
  }

  isDialog() {
    return chrome.getVariableValue('dialogArguments').length > 0;
  }

  isBarcodeApiAvailable() {
    return ('BarcodeDetector' in window);
  }

  async isQrScanningAvailable() {
    return loadTimeData.getBoolean('qrScannerEnabled')
        && this.isBarcodeApiAvailable()
        && (await this.isCameraAvailable());
  }

  async isCameraAvailable() {
    const devices = await navigator.mediaDevices.enumerateDevices();
    for (const device of devices) {
      if (device.kind === 'videoinput') {
        return true;
      }
    }
    return false;
  }

  static recordAccessCodeEntryTime(time: number) {
    if (time < 0) {
      return;
    }

    chrome.send('metricsHandler:recordMediumTime', [
      HISTOGRAM_ACCESS_CODE_INPUT_TIME,
      time,
    ]);
  }

  static recordCastAttemptLength(time: number) {
    if (time < 0) {
      return;
    }

    chrome.send('metricsHandler:recordMediumTime', [
      HISTOGRAM_CAST_ATTEMPT_LENGTH,
      time,
    ]);
  }

  static recordDialogCloseReason(reason: DialogCloseReason) {
    chrome.send('metricsHandler:recordInHistogram', [
      HISTOGRAM_DIALOG_CLOSE_REASON,
      reason,
      DialogCloseReason.COUNT,
    ]);
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxy());
  }

  static setInstance(obj: BrowserProxy) {
    instance = obj;
  }
}

let instance: BrowserProxy|null = null;
