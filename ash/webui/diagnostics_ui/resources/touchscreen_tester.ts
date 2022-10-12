// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CanvasDrawingProvider} from './drawing_provider.js';
import {getTemplate} from './touchscreen_tester.html.js';

// To ensure the tester works when the user rotates their screen, we
// need to set both the canvas width and height to be the larger number.
// Rather than looking for the correct display and find their size
// from backend, we take a simpler approach to set it as a very large
// number. The number is based on largest known supported resolution.
const SCREEN_MAX_LENGTH = 9999;

const TouchscreenTesterElementBase = I18nMixin(PolymerElement);

export class TouchscreenTesterElement extends TouchscreenTesterElementBase {
  static get is() {
    return 'touchscreen-tester';
  }

  static get template() {
    return getTemplate();
  }

  // Drawing provider.
  private drawingProvider: CanvasDrawingProvider;

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

    this.setupCanvas();
  }

  /**
   * Set up canvas width, height and drawing context.
   */
  private setupCanvas(): void {
    const canvas =
        this.shadowRoot!.querySelector('canvas') as HTMLCanvasElement;
    assert(canvas);

    canvas.width = SCREEN_MAX_LENGTH;
    canvas.height = SCREEN_MAX_LENGTH;

    // CSS in .html file does not have access to this element,
    // therefore adjust it here to make the canvas cover the whole screen.
    const topContainer =
        this.getDialog('canvas-dialog')!.shadowRoot!.querySelector(
            '.top-container') as HTMLElement;
    topContainer!.style.display = 'none';

    const ctx = canvas.getContext('2d');
    assert(ctx);
    this.drawingProvider = new CanvasDrawingProvider(ctx);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'touchscreen-tester': TouchscreenTesterElement;
  }
}

customElements.define(TouchscreenTesterElement.is, TouchscreenTesterElement);
