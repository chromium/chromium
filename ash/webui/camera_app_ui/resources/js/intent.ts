// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as metrics from './metrics.js';
import {ChromeHelper} from './mojo/chrome_helper.js';
import {Mode} from './type.js';

/**
 * Thrown when fails to parse intent url.
 */
export class ParseError extends Error {
  /**
   * @param url Intent url.
   */
  constructor(url: URL) {
    super(`Failed to parse intent url ${url}`);

    this.name = this.constructor.name;
  }
}

/**
 * Intent from ARC++.
 */
export class Intent {
  /**
   * Flag for avoiding intent being resolved by foreground and background
   * twice.
   */
  private doneInternal = false;

  /**
   * @param url URL passed along with app launch event.
   * @param intentId Id of the intent.
   * @param mode Capture mode of intent.
   * @param shouldHandleResult Whether the intent should return with the
   *     captured result.
   * @param shouldDownScale Whether the captured image should be down-scaled.
   * @param isSecure If the intent is launched when the device is under secure
   *     mode.
   */
  private constructor(
      readonly url: URL,
      readonly intentId: number,
      readonly mode: Mode,
      readonly shouldHandleResult: boolean,
      readonly shouldDownScale: boolean,
      readonly isSecure: boolean,
  ) {}

  /**
   * Whether intent has been finished or canceled.
   */
  get done(): boolean {
    return this.doneInternal;
  }

  /**
   * Notifies ARC++ to finish the intent.
   */
  async finish(): Promise<void> {
    if (this.doneInternal) {
      return;
    }
    this.doneInternal = true;
    await ChromeHelper.getInstance().finish(this.intentId);
    this.logResult(metrics.IntentResultType.CONFIRMED);
  }

  /**
   * Notifies ARC++ to append data to the intent result.
   *
   * @param data The data to be appended to intent result.
   */
  async appendData(data: Uint8Array): Promise<void> {
    if (this.doneInternal) {
      return;
    }
    await ChromeHelper.getInstance().appendData(this.intentId, data);
  }

  /**
   * Notifies ARC++ to clear appended intent result data.
   */
  async clearData(): Promise<void> {
    if (this.doneInternal) {
      return;
    }
    await ChromeHelper.getInstance().clearData(this.intentId);
  }

  /**
   * Logs the intent result to metrics.
   */
  logResult(result: metrics.IntentResultType): void {
    metrics.sendIntentEvent({
      intent: this,
      result,
    });
  }

  /**
   * @param url URL passed along with app launch event.
   * @param mode Mode for the intent.
   */
  static create(url: URL, mode: Mode): Intent {
    const params = url.searchParams;
    function getBool(key: string) {
      return params.get(key) === '1';
    }
    const param = params.get('intentId');
    if (param === null) {
      throw new ParseError(url);
    }
    const intentId = Number(param);
    if (!Number.isInteger(intentId)) {
      throw new ParseError(url);
    }

    return new Intent(
        url, intentId, mode, getBool('shouldHandleResult'),
        getBool('shouldDownScale'), getBool('isSecure'));
  }
}
