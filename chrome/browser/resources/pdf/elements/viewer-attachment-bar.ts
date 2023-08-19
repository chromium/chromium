// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './pdf-shared.css.js';
import './viewer-attachment.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Attachment} from '../constants.js';

import {getTemplate} from './viewer-attachment-bar.html.js';

export class ViewerAttachmentBarElement extends PolymerElement {
  static get is() {
    return 'viewer-attachment-bar';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      attachments: Array,

      exceedSizeLimit_: {
        type: Boolean,
        computed: 'computeExceedSizeLimit_(attachments)',
      },
    };
  }

  attachments: Attachment[];
  private exceedSizeLimit_: boolean;

  /* Indicates whether any oversized attachments exist */
  private computeExceedSizeLimit_(): boolean {
    return this.attachments.some(attachment => attachment.size === -1);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-attachment-bar': ViewerAttachmentBarElement;
  }
}

customElements.define(
    ViewerAttachmentBarElement.is, ViewerAttachmentBarElement);
