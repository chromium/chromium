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
       * Whether the default-hidden related visits are visible.
       */
      expanded_: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },

      /**
       * Whether there are related visits that can be shown.
       */
      hasRelatedVisits_: {
        type: Boolean,
        computed: 'computeHasRelatedVisits_(relatedVisits_)',
      },

      /**
       * The default-hidden related visits.
       */
      hiddenRelatedVisits_: {
        type: Object,
        computed: `computeHiddenRelatedVisits_(relatedVisits_)`,
      },

      /**
       * The related visits that can be shown.
       */
      relatedVisits_: {
        type: Object,
        computed: `computeRelatedVisits_(visit.*)`,
      },

      /**
       * The always-visible related visits.
       */
      visibleRelatedVisits_: {
        type: Object,
        computed: `computeVisibleRelatedVisits_(relatedVisits_)`,
      },
    };
  }

  //============================================================================
  // Properties
  //============================================================================

  visit: URLVisit;
  private expanded_: boolean;
  private relatedVisits_: Array<URLVisit>;

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

    // Notify the parent <history-cluster> element of this event.
    this.dispatchEvent(new CustomEvent('related-visits-visibility-toggled', {
      bubbles: true,
      composed: true,
    }));

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

  private computeHasRelatedVisits_(): boolean {
    return this.relatedVisits_.length > 0;
  }

  private computeHiddenRelatedVisits_(): Array<URLVisit> {
    return this.relatedVisits_.filter((visit: URLVisit) => {
      return visit.belowTheFold;
    });
  }

  private computeRelatedVisits_(): Array<URLVisit> {
    return this.visit.relatedVisits ?
        this.visit.relatedVisits.filter((visit: URLVisit) => {
          // 'Ghost' visits with scores of 0 (or below) are never to be shown,
          // unless the debug flag is switched on.
          return visit.score > 0 ||
              loadTimeData.getBoolean('isHistoryClustersDebug');
        }) :
        [];
  }

  private computeVisibleRelatedVisits_(): Array<URLVisit> {
    return this.relatedVisits_.filter((visit: URLVisit) => {
      return !visit.belowTheFold;
    });
  }

  /**
   * Returns the label of the toggle button based on whether the default-hidden
   * related visits are visible.
   */
  private getToggleButtonLabel_(expanded: boolean): string {
    return loadTimeData.getString(
        expanded ? 'toggleButtonLabelLess' : 'toggleButtonLabelMore');
  }

  /**
   * Returns the index of `visit` among the visits in the cluster.
   */
  private getVisitIndex_(relatedVisits: Array<URLVisit>, visit: URLVisit):
      number {
    return relatedVisits.indexOf(visit) + 1;  // Index 0 is the top visit.
  }
}

customElements.define(TopVisitElement.is, TopVisitElement);
