// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {assert} from 'chrome://resources/js/assert.js';

/**
 * Manages focus restoration for modal dialogs. After the final dialog in a
 * stack is closed, restores focus to the element which was focused when the
 * first dialog was opened.
 */
export class DialogFocusManager {
  private previousFocusElement_: HTMLElement|null = null;
  private dialogs_: Set<HTMLDialogElement|CrDialogElement> = new Set();

  showDialog(dialog: (HTMLDialogElement|CrDialogElement), showFn?: () => void) {
    if (!showFn) {
      showFn = function() {
        dialog.showModal();
      };
    }

    // Update the focus if there are no open dialogs or if this is the only
    // dialog and it's getting reshown.
    if (!this.dialogs_.size ||
        (this.dialogs_.has(dialog) && this.dialogs_.size === 1)) {
      this.updatePreviousFocus_();
    }

    if (!this.dialogs_.has(dialog)) {
      dialog.addEventListener('close', this.getCloseListener_(dialog));
      this.dialogs_.add(dialog);
    }

    showFn();
  }

  /**
   * @return True if the document currently has an open dialog.
   */
  hasOpenDialog(): boolean {
    return this.dialogs_.size > 0;
  }

  /**
   * Clears the stored focus element, so that focus does not restore when all
   * dialogs are closed.
   */
  clearFocus() {
    this.previousFocusElement_ = null;
  }

  private updatePreviousFocus_() {
    this.previousFocusElement_ = this.getFocusedElement_();
  }

  private getFocusedElement_(): HTMLElement {
    let focus = document.activeElement as HTMLElement;
    while (focus.shadowRoot && focus.shadowRoot!.activeElement) {
      focus = focus.shadowRoot!.activeElement as HTMLElement;
    }

    return focus;
  }

  private getCloseListener_(dialog: (HTMLDialogElement|CrDialogElement)):
      ((p1: Event) => void) {
    const closeListener = (_e: Event) => {
      // If the dialog is open, then it got reshown immediately and we
      // shouldn't clear it until it is closed again.
      if (dialog.open) {
        return;
      }

      assert(this.dialogs_.delete(dialog));
      // Focus the originally focused element if there are no more dialogs.
      if (!this.hasOpenDialog() && this.previousFocusElement_) {
        this.previousFocusElement_.focus();
      }

      dialog.removeEventListener('close', closeListener);
    };

    return closeListener;
  }

  static getInstance(): DialogFocusManager {
    return instance || (instance = new DialogFocusManager());
  }

  static setInstance(obj: DialogFocusManager|null) {
    instance = obj;
  }
}

let instance: DialogFocusManager|null = null;
