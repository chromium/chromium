// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shared_vars.js';

import {SearchQuery} from '/components/history_clusters/core/memories.mojom-webui.js';
import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MojomConversionMixinBase} from './mojom_conversion_mixin.js';

/**
 * @fileoverview This file provides a custom element displaying a search query.
 */

/** @polymer */
class SearchQueryElement extends MojomConversionMixinBase {
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
}

customElements.define(SearchQueryElement.is, SearchQueryElement);
