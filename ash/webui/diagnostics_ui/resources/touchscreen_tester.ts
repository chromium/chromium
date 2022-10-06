// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './touchscreen_tester.html.js';

const TouchscreenTesterElementBase = I18nMixin(PolymerElement);

export class TouchscreenTesterElement extends TouchscreenTesterElementBase {
  static get is() {
    return 'touchscreen-tester';
  }

  static get template() {
    return getTemplate();
  }

  getDialog(dialogId: string): CrDialogElement {
    const dialog = this.shadowRoot!.getElementById(dialogId);
    assert(dialog);
    return dialog as CrDialogElement;
  }

  /**
   * Shows the tester's dialog.
   */
  async showTester(): Promise<void> {
    const introDialog = this.getDialog('intro-dialog');
    await introDialog.requestFullscreen();
    introDialog.showModal();

    this.closeDialogWhenExitFullscreen();
  }

  /**
   * When user presses 'Ecs' key, the tester will only exit the fullscreen
   * mode. However, we want the tester to close when user has exited the
   * fullscreen mode. Add a event listener to listen to the
   * 'fullscreenchange' event to handle this case.
   */
  private closeDialogWhenExitFullscreen(): void {
    this.shadowRoot!.addEventListener('fullscreenchange', (e: Event) => {
      e.preventDefault();
      if (!document.fullscreenElement) {
        this.getDialog('intro-dialog').close();
        this.getDialog('canvas-dialog').close();
      }
    });
  }

  /**
   * Handle when get start button is clicked.
   */
  private onStartClick(): void {
    this.getDialog('intro-dialog').close();
    this.getDialog('canvas-dialog').showModal();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'touchscreen-tester': TouchscreenTesterElement;
  }
}

customElements.define(TouchscreenTesterElement.is, TouchscreenTesterElement);
