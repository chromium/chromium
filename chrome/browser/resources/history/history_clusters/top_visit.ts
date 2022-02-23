// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './search_query.js';
import './shared_style.js';
import './url_visit.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {URLVisit} from './history_clusters.mojom-webui.js';
import {getTemplate} from './top_visit.html.js';

/**
 * @fileoverview This file provides a custom element displaying a top visit
 * within a cluster. A top visit is a featured, i.e., visible, visit with an
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
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The index of the cluster this visit belongs to.
       */
      clusterIndex: {
        type: Number,
        value: -1,  // Initialized to an invalid value.
      },

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
       * Whether there are default-hidden related visits.
       */
      hasHiddenRelatedVisits_: {
        type: Boolean,
        computed: `computeHasHiddenRelatedVisits_(hiddenRelatedVisits_)`,
        reflectToAttribute: true,
      },

      /**
       * The default-hidden related visits.
       */
      hiddenRelatedVisits_: {
        type: Object,
        computed: `computeHiddenRelatedVisits_(visit.relatedVisits.*)`,
      },

      /**
       * The always-visible related visits.
       */
      visibleRelatedVisits_: {
        type: Object,
        computed: `computeVisibleRelatedVisits_(visit.relatedVisits.*)`,
      },
    };
  }

  //============================================================================
  // Properties
  //============================================================================

  visit: URLVisit;
  private expanded_: boolean;
  private hiddenRelatedVisits_: Array<URLVisit>;

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

  private computeHasHiddenRelatedVisits_(): boolean {
    return this.hiddenRelatedVisits_.length > 0;
  }

  private computeHiddenRelatedVisits_(): Array<URLVisit> {
    return this.visit.relatedVisits.filter((visit: URLVisit) => {
      return visit.belowTheFold;
    });
  }

  private computeVisibleRelatedVisits_(): Array<URLVisit> {
    return this.visit.relatedVisits.filter((visit: URLVisit) => {
      return !visit.belowTheFold;
    });
  }

  /**
   * Returns the label of the toggle button based on whether the default-hidden
   * related visits are visible.
   */
  private getToggleButtonLabel_(_expanded: boolean): string {
    return loadTimeData.getString(
        this.expanded_ ? 'toggleButtonLabelLess' : 'toggleButtonLabelMore');
  }

  /**
   * Returns the index of `relatedVisit` among the visits in the cluster.
   */
  private getVisitIndex_(relatedVisit: URLVisit): number {
    // Index 0 represents the top visit.
    return this.visit.relatedVisits.indexOf(relatedVisit) + 1;
  }
}

customElements.define(TopVisitElement.is, TopVisitElement);
