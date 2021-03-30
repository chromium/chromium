// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shared_vars.js';

import {SearchQuery} from '/components/memories/core/memories.mojom-webui.js';
import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {decodeMojoString16} from './utils.js';

/**
 * @fileoverview This file provides a custom element displaying a search query.
 */

class SearchQueryElement extends PolymerElement {
  static get is() {
    return 'search-query';
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
       * The search query to display.
       * @type {!SearchQuery}
       */
      searchQuery: Object,
    };
  }

  //============================================================================
  // Helper methods
  //============================================================================

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

customElements.define(SearchQueryElement.is, SearchQueryElement);
