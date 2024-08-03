// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './viewer_attachment.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {Attachment} from '../constants.js';

import {getCss} from './viewer_attachment_bar.css.js';
import {getHtml} from './viewer_attachment_bar.html.js';

export class ViewerAttachmentBarElement extends CrLitElement {
  static get is() {
    return 'viewer-attachment-bar';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      attachments: {type: Array},
    };
  }

  attachments: Attachment[] = [];

  /* Indicates whether any oversized attachments exist */
  protected exceedSizeLimit_(): boolean {
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
