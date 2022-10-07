// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './lens_upload_dialog.html.js';
export interface LensUploadDialogElement {
  $: {
    dialog: HTMLDivElement,
  };
}

// Modal that lets the user upload images for search on Lens.
export class LensUploadDialogElement extends PolymerElement {
  static get is() {
    return 'ntp-lens-upload-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      isHidden_: Boolean,
    };
  }

  private isHidden_: boolean = true;
  private outsideClickHandler_: (event: MouseEvent) => void;
  private outsideClickHandlerAttached_ = false;

  constructor() {
    super();
    this.outsideClickHandler_ = (event: MouseEvent) => {
      const outsideDialog = !event.composedPath().includes(this.$.dialog);
      if (outsideDialog) {
        this.closeDialog();
      }
    };
  }

  override connectedCallback() {
    super.connectedCallback();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.detachOutsideClickHandler_();
  }

  openDialog() {
    this.isHidden_ = false;
    // Click handler needs to be attached outside of the initial event handler,
    // otherwise the click of the icon which initially opened the dialog would
    // also be registered in the outside click handler, causing the dialog to
    // immediately close after opening.
    afterNextRender(this, () => this.attachOutsideClickHandler_());
  }

  closeDialog() {
    this.isHidden_ = true;
    this.detachOutsideClickHandler_();
    this.dispatchEvent(new Event('close-lens-search'));
  }

  private attachOutsideClickHandler_() {
    if (!this.outsideClickHandlerAttached_) {
      document.addEventListener('click', this.outsideClickHandler_);
      this.outsideClickHandlerAttached_ = true;
    }
  }

  private detachOutsideClickHandler_() {
    if (this.outsideClickHandlerAttached_) {
      document.removeEventListener('click', this.outsideClickHandler_);
      this.outsideClickHandlerAttached_ = false;
    }
  }

  private onCloseButtonClick_() {
    this.closeDialog();
  }
}
declare global {
  interface HTMLElementTagNameMap {
    'ntp-lens-upload-dialog': LensUploadDialogElement;
  }
}

customElements.define(LensUploadDialogElement.is, LensUploadDialogElement);
