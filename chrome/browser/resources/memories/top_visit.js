// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shared_vars.js';
import './visit_row.js';
import './search_query.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';

import {URLVisit} from '/components/history_clusters/core/history_clusters.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview This file provides a custom element displaying a top visit
 * within a Cluster. A top visit is a featured, i.e., visible, visit with an
 * optional set of related visits which are not visible by default.
 */

class TopVisitElement extends PolymerElement {
  static get is() {
    return 'top-visit';
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
       * The top visit to display
       * @type {!URLVisit}
       */
      visit: Object,

      //========================================================================
      // Private properties
      //========================================================================

      /**
       * Whether the related visits of the top visit are expanded/visible.
       * @private {boolean}
       */
      expanded_: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },

      /**
       * Related visits that are initially hidden.
       * @private {!Array<!URLVisit>}
       */
      hiddenRelatedVisits_: {
        type: Object,
        computed: `computeHiddenRelatedVisits_(visit)`,
      },

      /** @private {string} */
      toggleButtonLabel_: {
        type: String,
        computed: `computeToggleButtonLabel_(expanded_)`,
      },

      /**
       * Related visits that are always visible.
       * @private {!Array<!URLVisit>}
       */
      visibleRelatedVisits_: {
        type: Object,
        computed: `computeVisibleRelatedVisits_(visit)`,
      },
    };
  }

  //============================================================================
  // Event handlers
  //============================================================================

  /**
   * @param {!Event} e
   * @private
   */
  onToggleButtonKeyDown_(e) {
    if (e.key !== 'Enter' && e.key !== ' ') {
      return;
    }

    e.stopPropagation();
    e.preventDefault();

    this.onToggleButtonClick_();
  }

  /** @private */
  onToggleButtonClick_() {
    this.expanded_ = !this.expanded_;
  }

  //============================================================================
  // Helper methods
  //============================================================================

  /** @private */
  computeHiddenRelatedVisits_() {
    return this.visit && this.visit.relatedVisits ?
        this.visit.relatedVisits.filter(visit => {
          // "Ghost" visits with scores of 0 (or below) are never to be shown.
          return visit.score > 0 && visit.belowTheFold;
        }) :
        [];
  }

  /** @private */
  computeToggleButtonLabel_() {
    return loadTimeData.getString(
        this.expanded_ ? 'toggleButtonLabelLess' : 'toggleButtonLabelMore');
  }

  /** @private */
  computeVisibleRelatedVisits_() {
    return this.visit && this.visit.relatedVisits ?
        this.visit.relatedVisits.filter(visit => {
          // "Ghost" visits with scores of 0 (or below) are never to be shown.
          return visit.score > 0 && !visit.belowTheFold;
        }) :
        [];
  }
}

customElements.define(TopVisitElement.is, TopVisitElement);
