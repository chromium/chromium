// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shared-vars.js';
import '../pdf_viewer_shared_style.js';
import './icons.js';
import './viewer-document-outline.js';
import './viewer-thumbnail-bar.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Bookmark} from '../bookmark_type.js';
import {record, UserAction} from '../metrics.js';

export class ViewerPdfSidenavElement extends PolymerElement {
  static get is() {
    return 'viewer-pdf-sidenav';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      activePage: Number,

      /** @type {!Array<!Bookmark>} */
      bookmarks: {
        type: Array,
        value: () => [],
      },

      clockwiseRotations: Number,

      docLength: Number,

      /** @private */
      thumbnailView_: {
        type: Boolean,
        value: true,
      },
    };
  }

  /** @private */
  onThumbnailClick_() {
    record(UserAction.SELECT_SIDENAV_THUMBNAILS);
    this.thumbnailView_ = true;
  }

  /** @private */
  onOutlineClick_() {
    record(UserAction.SELECT_SIDENAV_OUTLINE);
    this.thumbnailView_ = false;
  }

  /**
   * @return {string}
   * @private
   */
  outlineButtonClass_() {
    return this.thumbnailView_ ? '' : 'selected';
  }

  /**
   * @return {string}
   * @private
   */
  thumbnailButtonClass_() {
    return this.thumbnailView_ ? 'selected' : '';
  }

  /**
   * @return {string}
   * @private
   */
  getAriaSelectedThumbnails_() {
    return this.thumbnailView_ ? 'true' : 'false';
  }

  /**
   * @return {string}
   * @private
   */
  getAriaSelectedOutline_() {
    return this.thumbnailView_ ? 'false' : 'true';
  }
}

customElements.define(ViewerPdfSidenavElement.is, ViewerPdfSidenavElement);
