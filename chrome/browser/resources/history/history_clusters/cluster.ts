// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './menu_container.js';
import './search_query.js';
import './shared_style.js';
import './shared_vars.js';
import './url_visit.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import {getTemplate} from './cluster.html.js';
import {Cluster, PageCallbackRouter, URLVisit} from './history_clusters.mojom-webui.js';
import {ClusterAction, MetricsProxyImpl, VisitAction} from './metrics_proxy.js';
import {insertHighlightedTextIntoElement} from './utils.js';

/**
 * @fileoverview This file provides a custom element displaying a cluster.
 */

declare global {
  interface HTMLElementTagNameMap {
    'history-cluster': HistoryClusterElement;
  }
}

interface HistoryClusterElement {
  $: {
    label: HTMLElement,
  };
}

class HistoryClusterElement extends PolymerElement {
  static get is() {
    return 'history-cluster';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The cluster displayed by this element.
       */
      cluster: Object,

      /**
       * The index of the cluster.
       */
      index: {
        type: Number,
        value: -1,  // Initialized to an invalid value.
      },

      /**
       * The current query for which related clusters are requested and shown.
       */
      query: String,

      /**
       * Whether the default-hidden visits are visible.
       */
      expanded_: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },

      /**
       * The default-hidden visits.
       */
      hiddenVisits_: {
        type: Object,
        computed: `computeHiddenVisits_(cluster.visits.*)`,
      },

      /**
       * The label for the cluster. This property is actually unused. The side
       * effect of the compute function is used to insert the HTML elements for
       * highlighting into this.$.label element.
       */
      unusedLabel_: {
        type: String,
        computed: 'computeLabel_(cluster.label)',
      },

      /**
       * The always-visible visits.
       */
      visibleVisits_: {
        type: Object,
        computed: `computeVisibleVisits_(cluster.visits.*)`,
      },
    };
  }

  //============================================================================
  // Properties
  //============================================================================

  cluster: Cluster;
  index: number;
  query: string;
  private callbackRouter_: PageCallbackRouter;
  private expanded_: boolean;
  private hiddenVisits_: Array<URLVisit>;
  private onVisitsRemovedListenerId_: number|null = null;
  private unusedLabel_: string;
  private visibleVisits_: Array<URLVisit>;

  //============================================================================
  // Overridden methods
  //============================================================================

  constructor() {
    super();
    this.callbackRouter_ = BrowserProxyImpl.getInstance().callbackRouter;

    // This element receives a tabindex, because it's an iron-list item.
    // However, what we really want to do is to pass that focus onto an
    // eligible child, so we set `delegatesFocus` to true.
    this.attachShadow({mode: 'open', delegatesFocus: true});
  }

  override connectedCallback() {
    super.connectedCallback();
    this.onVisitsRemovedListenerId_ =
        this.callbackRouter_.onVisitsRemoved.addListener(
            this.onVisitsRemoved_.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.onVisitsRemovedListenerId_);
    this.callbackRouter_.removeListener(this.onVisitsRemovedListenerId_);
    this.onVisitsRemovedListenerId_ = null;
  }

  //============================================================================
  // Event handlers
  //============================================================================

  private onRelatedSearchClicked_() {
    MetricsProxyImpl.getInstance().recordClusterAction(
        ClusterAction.RELATED_SEARCH_CLICKED, this.index);
  }

  private onVisitClicked_(event: CustomEvent<URLVisit>) {
    MetricsProxyImpl.getInstance().recordClusterAction(
        ClusterAction.VISIT_CLICKED, this.index);

    const visit = event.detail;
    MetricsProxyImpl.getInstance().recordVisitAction(
        VisitAction.CLICKED, this.getVisitIndex_(visit),
        MetricsProxyImpl.getVisitType(visit));
  }

  private onOpenAllVisits_() {
    const visitsToOpen = this.visibleVisits_;
    // Only try to open the hidden visits if the user actually has
    // expanded the cluster by clicking "Show More".
    if (this.expanded_) {
      visitsToOpen.push(...this.hiddenVisits_);
    }

    BrowserProxyImpl.getInstance().handler.openVisitUrlsInTabGroup(
        visitsToOpen);

    MetricsProxyImpl.getInstance().recordClusterAction(
        ClusterAction.OPENED_IN_TAB_GROUP, this.index);
  }

  private onRemoveAllVisits_() {
    // Pass event up with new detail of all this cluster's visits.
    this.dispatchEvent(new CustomEvent('remove-visits', {
      bubbles: true,
      composed: true,
      detail: this.cluster.visits,
    }));
  }

  private onRemoveVisit_(event: CustomEvent<URLVisit>) {
    // The actual removal is handled at in clusters.ts. This is just a good
    // place to record the metric.
    const visit = event.detail;
    MetricsProxyImpl.getInstance().recordVisitAction(
        VisitAction.DELETED, this.getVisitIndex_(visit),
        MetricsProxyImpl.getVisitType(visit));

    this.dispatchEvent(new CustomEvent('remove-visits', {
      bubbles: true,
      composed: true,
      detail: [visit],
    }));
  }

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

    MetricsProxyImpl.getInstance().recordClusterAction(
        ClusterAction.RELATED_VISITS_VISIBILITY_TOGGLED, this.index);

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

  /**
   * Called with the original remove params when the last accepted request to
   * browser to remove visits succeeds. Since the same visit may appear in
   * multiple Clusters, all Clusters receive this callback in order to get a
   * chance to remove their matching visits.
   */
  private onVisitsRemoved_(removedVisits: Array<URLVisit>) {
    const visitHasBeenRemoved = (visit: URLVisit) => {
      return removedVisits.findIndex((removedVisit) => {
        return visit.normalizedUrl.url === removedVisit.normalizedUrl.url &&
            visit.lastVisitTime.internalValue <=
            removedVisit.lastVisitTime.internalValue &&
            visit.firstVisitTime.internalValue >=
            removedVisit.firstVisitTime.internalValue;
      }) !== -1;
    };

    const allVisits = this.cluster.visits;
    const remainingVisits = allVisits.filter(v => !visitHasBeenRemoved(v));
    if (allVisits.length === remainingVisits.length) {
      return;
    }

    if (!remainingVisits.length) {
      // If all the visits are removed, fire an event to also remove this
      // cluster from the list of clusters.
      this.dispatchEvent(new CustomEvent('remove-cluster', {
        bubbles: true,
        composed: true,
        detail: this.index,
      }));

      MetricsProxyImpl.getInstance().recordClusterAction(
          ClusterAction.DELETED, this.index);
    } else {
      this.set('cluster.visits', remainingVisits);
    }

    this.dispatchEvent(new CustomEvent('iron-resize', {
      bubbles: true,
      composed: true,
    }));
  }

  private computeHiddenVisits_(): Array<URLVisit> {
    return this.cluster.visits.filter((visit: URLVisit) => {
      return visit.hidden;
    });
  }

  private computeLabel_(): string {
    insertHighlightedTextIntoElement(
        this.$.label, this.cluster.label!, this.query);
    return this.cluster.label!;
  }

  private computeVisibleVisits_(): Array<URLVisit> {
    return this.cluster.visits.filter((visit: URLVisit) => {
      return !visit.hidden;
    });
  }

  /**
   * Returns true if this should be considered a top visit.
   */
  private isTopVisit_(index: number, label: string): boolean {
    return index === 0 && !label;
  }

  /**
   * Returns the label of the toggle button based on whether the default-hidden
   * visits are visible.
   */
  private getToggleButtonLabel_(_expanded: boolean): string {
    return loadTimeData.getString(
        this.expanded_ ? 'toggleButtonLabelLess' : 'toggleButtonLabelMore');
  }

  /**
   * Returns the index of `visit` among the visits in the cluster. Returns -1
   * if the visit is not found in the cluster at all.
   */
  private getVisitIndex_(visit: URLVisit): number {
    return this.cluster.visits.indexOf(visit);
  }
}

customElements.define(HistoryClusterElement.is, HistoryClusterElement);
