// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './viewer_bookmark.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {Bookmark} from '../bookmark_type.js';

import {getCss} from './viewer_document_outline.css.js';
import {getHtml} from './viewer_document_outline.html.js';

export class ViewerDocumentOutlineElement extends CrLitElement {
  static get is() {
    return 'viewer-document-outline';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      bookmarks: {type: Array},
    };
  }

  bookmarks: Bookmark[] = [];
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-document-outline': ViewerDocumentOutlineElement;
  }
}

customElements.define(
    ViewerDocumentOutlineElement.is, ViewerDocumentOutlineElement);
