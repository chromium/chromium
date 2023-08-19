// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/icons.html.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Attachment} from '../constants.js';

import {getTemplate} from './viewer-attachment.html.js';

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

export class ViewerAttachmentElement extends PolymerElement {
  static get is() {
    return 'viewer-attachment';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      attachment: Object,

      index: Number,

      saveAllowed_: {
        type: Boolean,
        reflectToAttribute: true,
        computed: 'computeSaveAllowed_(attachment.size)',
      },
    };
  }

  attachment: Attachment;
  index: number;
  private saveAllowed_: boolean;

  /** Indicate whether the attachment can be downloaded. */
  private computeSaveAllowed_(): boolean {
    return this.attachment.size !== -1;
  }

  private onDownloadClick_() {
    if (this.attachment.size === -1) {
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
