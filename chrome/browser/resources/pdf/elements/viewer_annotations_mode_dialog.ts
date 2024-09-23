// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './viewer_annotations_mode_dialog.html.js';

export interface ViewerAnnotationsModeDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

export class ViewerAnnotationsModeDialogElement extends PolymerElement {
  static get is() {
    return 'viewer-annotations-mode-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      rotated: Boolean,
      twoUpViewEnabled: Boolean,
    };
  }

  rotated: boolean;
  twoUpViewEnabled: boolean;

  /** @return Whether the dialog is open. */
  isOpen(): boolean {
    return this.$.dialog.hasAttribute('open');
  }

  /** @return Whether the dialog was confirmed */
  wasConfirmed(): boolean {
    return this.$.dialog.getNative().returnValue === 'success';
  }

  private getBodyMessage_(): string {
    if (this.rotated && this.twoUpViewEnabled) {
      return loadTimeData.getString('annotationResetRotateAndTwoPageView');
    }
    if (this.rotated) {
      return loadTimeData.getString('annotationResetRotate');
    }
    return loadTimeData.getString('annotationResetTwoPageView');
  }

  private onEditClick_() {
    this.$.dialog.close();
  }

  private onCancelClick_() {
    this.$.dialog.cancel();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-annotations-mode-dialog': ViewerAnnotationsModeDialogElement;
  }
}

customElements.define(
    ViewerAnnotationsModeDialogElement.is, ViewerAnnotationsModeDialogElement);
