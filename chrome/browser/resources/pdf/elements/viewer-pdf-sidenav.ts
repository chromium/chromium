// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shared-vars.css.js';
import '../pdf_viewer_shared_style.css.js';
import './icons.html.js';
import './viewer-document-outline.js';
import './viewer-thumbnail-bar.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Bookmark} from '../bookmark_type.js';
import {record, UserAction} from '../metrics.js';

import {getTemplate} from './viewer-pdf-sidenav.html.js';

export class ViewerPdfSidenavElement extends PolymerElement {
  static get is() {
    return 'viewer-pdf-sidenav';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      activePage: Number,

      bookmarks: {
        type: Array,
        value: () => [],
      },

      clockwiseRotations: Number,

      docLength: Number,

      thumbnailView_: {
        type: Boolean,
        value: true,
      },
    };
  }

  activePage: number;
  bookmarks: Bookmark[];
  clockwiseRotations: number;
  docLength: number;
  private thumbnailView_: boolean;

  private onThumbnailClick_() {
    record(UserAction.SELECT_SIDENAV_THUMBNAILS);
    this.thumbnailView_ = true;
  }

  private onOutlineClick_() {
    record(UserAction.SELECT_SIDENAV_OUTLINE);
    this.thumbnailView_ = false;
  }

  private outlineButtonClass_(): string {
    return this.thumbnailView_ ? '' : 'selected';
  }

  private thumbnailButtonClass_(): string {
    return this.thumbnailView_ ? 'selected' : '';
  }

  private getAriaSelectedThumbnails_(): string {
    return this.thumbnailView_ ? 'true' : 'false';
  }

  private getAriaSelectedOutline_(): string {
    return this.thumbnailView_ ? 'false' : 'true';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-pdf-sidenav': ViewerPdfSidenavElement;
  }
}

customElements.define(ViewerPdfSidenavElement.is, ViewerPdfSidenavElement);
