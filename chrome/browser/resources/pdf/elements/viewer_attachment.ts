// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {Attachment} from '../constants.js';

import {getCss} from './viewer_attachment.css.js';
import {getHtml} from './viewer_attachment.html.js';

export interface SaveAttachment {
  index: number;
}

declare global {
  interface HTMLElementEventMap {
    'save-attachment': CustomEvent<SaveAttachment>;
  }
}

export interface ViewerAttachmentElement {
  $: {
    title: HTMLElement,
    download: HTMLElement,
  };
}

export class ViewerAttachmentElement extends CrLitElement {
  static get is() {
    return 'viewer-attachment';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      attachment: {type: Object},
      index: {type: Number},
      saveAllowed_: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  attachment: Attachment = {name: '', size: 0, readable: false};
  index: number = -1;
  protected saveAllowed_: boolean = false;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('attachment')) {
      this.saveAllowed_ = !!this.attachment && this.attachment.size !== -1;
    }
  }

  protected onDownloadClick_() {
    if (!this.attachment || this.attachment.size === -1) {
      return;
    }
    this.dispatchEvent(new CustomEvent(
        'save-attachment',
        {detail: this.index, bubbles: true, composed: true}));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-attachment': ViewerAttachmentElement;
  }
}

customElements.define(ViewerAttachmentElement.is, ViewerAttachmentElement);
