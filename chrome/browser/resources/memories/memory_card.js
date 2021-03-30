// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './memory_tile.js';
import './page_favicon.js';
import './page_thumbnail.js';
import './search_query.js';
import './shared_vars.js';
import './top_visit.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';

import {Memory} from '/components/history_clusters/core/memories.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MojomConversionMixinBase} from './mojom_conversion_mixin.js';
import {getHostnameFromUrl} from './utils.js';

/**
 * @fileoverview This file provides a custom element displaying a Memory.
 */

/** @polymer */
class MemoryCardElement extends MojomConversionMixinBase {
  static get is() {
    return 'memory-card';
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
       * The Memory displayed by this element.
       * @type {!Memory}
       */
      memory: Object,

      //========================================================================
      // Private properties
      //========================================================================

      /**
       * Whether the Memory has related tab groups or bookmarks.
       * @private {boolean}
       */
      hasRelatedTabGroupsOrBookmarks_: {
        type: Boolean,
        computed: 'computeHasRelatedTabGroupsOrBookmarks_(memory)'
      },
    };
  }

  //============================================================================
  // Helper methods
  //============================================================================

  /** @private */
  computeHasRelatedTabGroupsOrBookmarks_() {
    return this.memory.relatedTabGroups.length > 0 ||
        this.memory.bookmarks.length > 0;
  }

  /**
   * @param {!Url} url
   * @return {string} The domain name of the URL without the leading 'www.'.
   * @private
   */
  getHostnameFromUrl_(url) {
    return getHostnameFromUrl(url);
  }

  /**
   * @param {!Array} array
   * @param {number} num
   * @return {!Array} Shallow copy of the first |num| items of the input array.
   * @private
   */
  arrayItems_(array, num) {
    return array.slice(0, num);
  }
}

customElements.define(MemoryCardElement.is, MemoryCardElement);
