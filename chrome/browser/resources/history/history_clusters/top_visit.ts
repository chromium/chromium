// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './search_query.js';
import './shared_style.js';
import './url_visit.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {URLVisit} from './history_clusters.mojom-webui.js';

/**
 * @fileoverview This file provides a custom element displaying a top visit
 * within a Cluster. A top visit is a featured, i.e., visible, visit with an
 * optional set of related visits which are not visible by default.
 */

declare global {
  interface HTMLElementTagNameMap {
    'top-visit': TopVisitElement;
  }
}

class TopVisitElement extends PolymerElement {
  static get is() {
    return 'top-visit';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * The top visit to display
       */
      visit: Object,

      /**
       * Whether the related visits of the top visit are expanded/visible.
       */
      expanded_: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },

      /**
       * Related visits that are initially hidden.
       */
      hiddenRelatedVisits_: {
        type: Object,
        computed: `computeHiddenRelatedVisits_(visit.*)`,
      },

      toggleButtonLabel_: {
        type: String,
        computed: `computeToggleButtonLabel_(expanded_)`,
      },

      /**
       * Related visits that are always visible.
       */
      visibleRelatedVisits_: {
        type: Object,
        computed: `computeVisibleRelatedVisits_(visit.*)`,
      },
    };
  }

  //============================================================================
  // Properties
  //============================================================================

  visit: URLVisit = new URLVisit();
  private expanded_: boolean = false;
  private hiddenRelatedVisits_: Array<URLVisit> = [];
  private toggleButtonLabel_: string = '';
  private visibleRelatedVisits_: Array<URLVisit> = [];

  //============================================================================
  // Event handlers
  //============================================================================

  private onToggleButtonKeyDown_(e: KeyboardEvent) {
    if (e.key !== 'Enter' && e.key !== ' ') {
      return;
    }

    e.stopPropagation();
    e.preventDefault();

    this.onToggleButtonClick_();
  }

  private onToggleButtonClick_() {
    this.expanded_ = !this.expanded_;

    // Dispatch an event to notify the parent elements of a resize. Note that
    // this simple solution only works because the child iron-collapse has
    // animations disabled. Otherwise, it gets an incorrect mid-animation size.
    this.dispatchEvent(new CustomEvent('iron-resize', {
      bubbles: true,
      composed: true,
    }));
  }

  //============================================================================
  // Helper methods
  //============================================================================

  private computeHiddenRelatedVisits_(): Array<URLVisit> {
    return this.visit && this.visit.relatedVisits ?
        this.visit.relatedVisits.filter((visit: URLVisit) => {
          // 'Ghost' visits with scores of 0 (or below) are never to be shown,
          // unless the debug flag is switched on.
          if (visit.score <= 0 &&
              !loadTimeData.getBoolean('isHistoryClustersDebug')) {
            return false;
          }
          return visit.belowTheFold;
        }) :
        [];
  }

  private computeToggleButtonLabel_(): string {
    return loadTimeData.getString(
        this.expanded_ ? 'toggleButtonLabelLess' : 'toggleButtonLabelMore');
  }

  private computeVisibleRelatedVisits_(): Array<URLVisit> {
    return this.visit && this.visit.relatedVisits ?
        this.visit.relatedVisits.filter((visit: URLVisit) => {
          // 'Ghost' visits with scores of 0 (or below) are never to be shown,
          // unless the debug flag is switched on.
          if (visit.score <= 0 &&
              !loadTimeData.getBoolean('isHistoryClustersDebug')) {
            return false;
          }
          return !visit.belowTheFold;
        }) :
        [];
  }
}

customElements.define(TopVisitElement.is, TopVisitElement);
