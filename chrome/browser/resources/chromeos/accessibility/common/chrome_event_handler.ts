// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ChromeEvent} from '/tools/typescript/definitions/chrome_event.js';

type GenericListener<T extends any[]> = (...args: T) => void;

/**
 * This class wraps ChromeEvent listeners, adding some convenience
 * functions.
 */
export class ChromeEventHandler<T extends any[]> {
  private listening_ = false;

  constructor(private chromeEvent_: ChromeEvent<GenericListener<T>>,
      private callback_: GenericListener<T>) {}

  /** Starts listening to events. */
  start(): void {
    if (this.listening_) {
      return;
    }

    this.listening_ = true;
    this.chromeEvent_.addListener(this.callback_);
  }

  /** Stops listening or handling future events. */
  stop(): void {
    this.listening_ = false;
    this.chromeEvent_.removeListener(this.callback_);
  }
}
