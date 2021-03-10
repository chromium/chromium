// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/shared_style_css.m.js';

import {WebPage} from '/components/memories/core/memories.mojom-webui.js';
import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {decodeMojoString16} from './utils.js';

/**
 * @fileoverview This file provides a custom element displaying a page
 * thumbnail.
 */

class PageThumbnailElement extends PolymerElement {
  static get is() {
    return 'page-thumbnail';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      //========================================================================
      // Public properties
      //========================================================================

      /**
       * The page for which the thumbnail is shown.
       * @type {!WebPage}
       */
      page: Object,
    };
  }

  //============================================================================
  // Helper methods
  //============================================================================

  /**
   * @param {Url} thumbnailUrl
   * @return {string}
   * @private
   */
  thumbnailSrc_(thumbnailUrl) {
    return thumbnailUrl ? `chrome://image?${thumbnailUrl.url}` : '';
  }

  /**
   * Converts a Mojo String16 to a JS string.
   * @param {String16} str
   * @return {string}
   * @private
   */
  decodeMojoString16_(str) {
    return decodeMojoString16(str);
  }
}

customElements.define(PageThumbnailElement.is, PageThumbnailElement);
