// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @enum {number} */
export const State = {
  NOT_READY: 0,
  READY: 1,
  HIDDEN: 2,
  PRINTING: 3,
  SYSTEM_DIALOG: 4,
  ERROR: 5,
  FATAL_ERROR: 6,
  CLOSING: 7,
};

/** @enum {number} */
export const Error = {
  NONE: 0,
  INVALID_TICKET: 1,
  INVALID_PRINTER: 2,
  UNSUPPORTED_PRINTER: 3,
  NO_DESTINATIONS: 4,
  NO_PLUGIN: 5,
  PREVIEW_FAILED: 6,
  PRINT_FAILED: 7,
  CLOUD_PRINT_ERROR: 8,
};

Polymer({
  is: 'print-preview-state',

  _template: null,

  properties: {
    /** @type {!State} */
    state: {
      type: Number,
      notify: true,
      value: State.NOT_READY,
    },

    /** @type {!Error} */
    error: {
      type: Number,
      notify: true,
      value: Error.NONE,
    },
  },

  /** @param {State} newState The state to transition to. */
  transitTo(newState) {
    switch (newState) {
      case (State.NOT_READY):
        assert(
            this.state === State.NOT_READY || this.state === State.READY ||
            this.state === State.ERROR);
        break;
      case (State.READY):
        assert(
            this.state === State.ERROR || this.state === State.NOT_READY ||
            this.state === State.PRINTING);
        break;
      case (State.HIDDEN):
        assert(this.state === State.READY);
        break;
      case (State.PRINTING):
        assert(this.state === State.READY || this.state === State.HIDDEN);
        break;
      case (State.SYSTEM_DIALOG):
        assert(
            this.state !== State.HIDDEN && this.state !== State.PRINTING &&
            this.state !== State.CLOSING);
        break;
      case (State.ERROR):
        assert(
            this.state === State.ERROR || this.state === State.NOT_READY ||
            this.state === State.READY);
        break;
      case (State.CLOSING):
        assert(this.state !== State.HIDDEN);
        break;
    }
    this.state = newState;
    if (newState !== State.ERROR && newState !== State.FATAL_ERROR) {
      this.error = Error.NONE;
    }
  },
});
