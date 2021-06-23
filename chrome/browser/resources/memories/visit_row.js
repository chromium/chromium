// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './page_favicon.js';
import './shared_style.js';
import './strings.m.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.m.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';

import {Annotation, URLVisit} from '/components/history_clusters/core/history_clusters.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getHostnameFromUrl} from './utils.js';

/**
 * @fileoverview This file provides a custom element displaying a visit to a
 * page within a Cluster. A visit features the page favicon, title, a timestamp,
 * as well as an action menu.
 */

class VisitRowElement extends PolymerElement {
  static get is() {
    return 'visit-row';
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
       * The visit to display.
       * @type {!URLVisit}
       */
      visit: Object,

      /**
       * Whether the visit is a top visit.
       * @type {boolean}
       */
      isTopVisit: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      //========================================================================
      // Private properties
      //========================================================================

      /**
       * Whether the visit is to the search results page.
       * @private {boolean}
       */
      isSearchResultsPage_: {
        type: Boolean,
        value: false,
        computed: 'computeIsSearchResultsPage_(visit)',
      },

      /**
       * Title of the action menu button to remove this visit and its related
       * visits, if applicable.
       * @private {string}
       */
      removeButtonTitle_: {
        type: String,
        computed: 'computeRemoveButtonTitle_(visit)',
      },
    };
  }

  //============================================================================
  // Event handlers
  //============================================================================

  /**
   * @param {!MouseEvent} event
   * @private
   */
  onActionMenuButtonClick_(event) {
    // Only handle main (usually the left) and auxiliary (usually the wheel or
    // the middle) button presses.
    if (event.button > 1) {
      return;
    }

    this.$.actionMenu.get().showAt(this.$.actionMenuButton);

    // Prevent the enclosing <cr-expand-button> from receiving this event.
    event.stopPropagation();
  }

  /**
   * @param {!MouseEvent} event
   * @private
   */
  onRemoveButtonClick_(event) {
    // Only handle main (usually the left) and auxiliary (usually the wheel or
    // the middle) button presses.
    if (event.button > 1) {
      return;
    }

    this.dispatchEvent(new CustomEvent('remove-visits', {
      bubbles: true,
      composed: true,
      detail: [this.visit, ...this.visit.relatedVisits],
    }));

    this.$.actionMenu.get().close();

    // Prevent the enclosing <cr-expand-button> from receiving this event.
    event.stopPropagation();
  }

  //============================================================================
  // Helper methods
  //============================================================================

  /** @private */
  computeIsSearchResultsPage_() {
    return this.visit.annotations.includes(Annotation.kSearchResultsPage);
  }

  /** @private */
  computeRemoveButtonTitle_() {
    return loadTimeData.getString(
        this.visit.relatedVisits.length > 0 ? 'removeAllFromHistory' :
                                              'removeFromHistory');
  }

  /**
   * @param {!Url} url
   * @return {string} The domain name of the URL without the leading 'www.'.
   * @private
   */
  getHostnameFromUrl_(url) {
    return getHostnameFromUrl(url);
  }
}

customElements.define(VisitRowElement.is, VisitRowElement);
