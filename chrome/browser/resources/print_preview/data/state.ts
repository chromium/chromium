// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

export enum State {
  NOT_READY = 0,
  READY = 1,
  PRINT_PENDING = 2,
  HIDDEN = 3,
  PRINTING = 4,
  SYSTEM_DIALOG = 5,
  ERROR = 6,
  FATAL_ERROR = 7,
  CLOSING = 8,
}

/**
 * These values are persisted to logs. New entries should replace MAX_BUCKET but
 * existing entries should not be renumbered and numeric values should never be
 * reused.
 */
export enum Error {
  NONE = 0,
  INVALID_TICKET = 1,
  INVALID_PRINTER = 2,
  NO_DESTINATIONS = 3,
  PREVIEW_FAILED = 4,
  PRINT_FAILED = 5,
  MAX_BUCKET = 6,
}


export class PrintPreviewStateElement extends CrLitElement {
  static get is() {
    return 'print-preview-state';
  }

  static override get properties() {
    return {
      error: {
        type: Number,
        notify: true,
      },
    };
  }

  private state_: State = State.NOT_READY;
  accessor error: Error = Error.NONE;

  override connectedCallback() {
    super.connectedCallback();
    this.sendStateChanged_();
  }

  private sendStateChanged_() {
    this.dispatchEvent(
        new CustomEvent('state-changed', {detail: {value: this.state_}}));
  }

  transitTo(newState: State) {
    switch (newState) {
      case (State.NOT_READY):
        assert(
            this.state_ === State.NOT_READY || this.state_ === State.READY ||
            this.state_ === State.ERROR);
        break;
      case (State.READY):
        assert(
            this.state_ === State.ERROR || this.state_ === State.NOT_READY ||
            this.state_ === State.PRINTING);
        break;
      case (State.PRINT_PENDING):
        assert(this.state_ === State.READY);
        break;
      case (State.HIDDEN):
        assert(this.state_ === State.PRINT_PENDING);
        break;
      case (State.PRINTING):
        assert(
            this.state_ === State.READY || this.state_ === State.HIDDEN ||
            this.state_ === State.PRINT_PENDING);
        break;
      case (State.SYSTEM_DIALOG):
        assert(
            this.state_ !== State.HIDDEN && this.state_ !== State.PRINTING &&
            this.state_ !== State.CLOSING);
        break;
      case (State.ERROR):
        assert(
            this.state_ === State.ERROR || this.state_ === State.NOT_READY ||
            this.state_ === State.READY);
        break;
      case (State.CLOSING):
        assert(this.state_ !== State.HIDDEN);
        break;
    }

    const oldState = this.state_;
    this.state_ = newState;

    if (oldState !== newState) {
      // Fire a manual 'state-changed' event to ensure that all states changes
      // are reported, even if a state is changed twice in the same cycle, which
      // wouldn't be the case if CrLitElement's 'notify: true' was used.
      this.sendStateChanged_();
    }

    if (newState !== State.ERROR && newState !== State.FATAL_ERROR) {
      this.error = Error.NONE;
    }
  }
}

customElements.define(PrintPreviewStateElement.is, PrintPreviewStateElement);
