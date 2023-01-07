// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './pdf-shared.css.js';
import './viewer-bookmark.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Bookmark} from '../bookmark_type.js';

import {getTemplate} from './viewer-document-outline.html.js';

export class ViewerDocumentOutlineElement extends PolymerElement {
  static get is() {
    return 'viewer-document-outline';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      bookmarks: Array,
    };
  }

  bookmarks: Bookmark[];
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-document-outline': ViewerDocumentOutlineElement;
  }
}

customElements.define(
    ViewerDocumentOutlineElement.is, ViewerDocumentOutlineElement);
