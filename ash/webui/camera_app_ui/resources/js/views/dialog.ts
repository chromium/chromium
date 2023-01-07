// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertString} from '../assert.js';
import * as dom from '../dom.js';
import {ViewName} from '../type.js';

import {DialogEnterOptions, View} from './view.js';

/**
 * Creates the Dialog view controller.
 */
export class Dialog extends View {
  private readonly positiveButton: HTMLButtonElement;

  private negativeButton: HTMLButtonElement|null;

  private messageHolder: HTMLElement;

  /**
   * @param name View name of the dialog.
   */
  constructor(name: ViewName) {
    super(
        name,
        {dismissByEsc: true, defaultFocusSelector: '.dialog-positive-button'});

    this.positiveButton =
        dom.getFrom(this.root, '.dialog-positive-button', HTMLButtonElement);

    this.negativeButton = (() => {
      const btn = dom.getAllFrom(
          this.root, '.dialog-negative-button', HTMLButtonElement)[0];
      return btn || null;
    })();

    this.messageHolder =
        dom.getFrom(this.root, '.dialog-msg-holder', HTMLElement);

    this.positiveButton.addEventListener(
        'click', () => this.leave({kind: 'CLOSED', val: true}));
    if (this.negativeButton !== null) {
      this.negativeButton.addEventListener('click', () => this.leave());
    }
  }

  override entering({message, cancellable = false}: DialogEnterOptions = {}):
      void {
    if (message !== undefined) {
      this.messageHolder.textContent = assertString(message);
    }
    if (this.negativeButton !== null) {
      this.negativeButton.hidden = !cancellable;
    }
  }
}
