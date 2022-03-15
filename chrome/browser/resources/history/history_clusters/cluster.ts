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

/**
 * @fileoverview This file provides a custom element displaying a cluster.
 */

declare global {
  interface HTMLElementTagNameMap {
    'history-cluster': HistoryClusterElement,
  }
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
       * The index of the cluster.
       */
      index: {
        type: Number,
        value: -1,  // Initialized to an invalid value.
      },

      /**
       * The cluster displayed by this element.
       */
      cluster: Object,

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
        computed: `computeHiddenRelatedVisits_(cluster.visit.relatedVisits.*)`,
      },

      /**
       * The always-visible related visits.
       */
      visibleRelatedVisits_: {
        type: Object,
        computed: `computeVisibleRelatedVisits_(cluster.visit.relatedVisits.*)`,
      },
    };
  }

  //============================================================================
  // Properties
  //============================================================================

  cluster: Cluster;
  index: number;
  private callbackRouter_: PageCallbackRouter;
  private onVisitsRemovedListenerId_: number|null = null;
  private expanded_: boolean;
  private hiddenRelatedVisits_: Array<URLVisit>;
  private visibleRelatedVisits_: Array<URLVisit>;

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
    const visitsToOpen = [this.cluster.visit, ...this.visibleRelatedVisits_];
    // Only try to open the hidden related visits if the user actually has
    // expanded the cluster by clicking "Show More".
    if (this.expanded_) {
      visitsToOpen.push(...this.hiddenRelatedVisits_);
    }

    BrowserProxyImpl.getInstance().handler.openVisitUrlsInTabGroup(
        visitsToOpen);

    MetricsProxyImpl.getInstance().recordClusterAction(
        ClusterAction.OPENED_IN_TAB_GROUP, this.index);
  }

  private onRemoveVisits_(event: CustomEvent<Array<URLVisit>>) {
    // The actual removal is handled at in clusters.ts. This is just a good
    // place to record the metric.
    const visitsToBeRemoved = event.detail;

    // To match the historic semantics, we only record this metric when a single
    // visit is requested to be removed by the user.
    if (visitsToBeRemoved.length === 1) {
      const visit = visitsToBeRemoved[0];
      MetricsProxyImpl.getInstance().recordVisitAction(
          VisitAction.DELETED, this.getVisitIndex_(visit),
          MetricsProxyImpl.getVisitType(visit));
    }
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

    // Flatten the cluster's constitutent visits and filter out the removed
    // ones, if any.
    const allVisits = [this.cluster.visit, ...this.cluster.visit.relatedVisits];
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
      // Reconstitute the cluster by setting the top visit with the
      // `remainingVisits` as its related visits.
      this.set('cluster.visit', remainingVisits.shift()!);
      this.set('cluster.visit.relatedVisits', remainingVisits);
    }

    this.dispatchEvent(new CustomEvent('iron-resize', {
      bubbles: true,
      composed: true,
    }));
  }

  private computeHasHiddenRelatedVisits_(): boolean {
    return this.hiddenRelatedVisits_.length > 0;
  }

  private computeHiddenRelatedVisits_(): Array<URLVisit> {
    return this.cluster.visit.relatedVisits.filter((visit: URLVisit) => {
      return visit.belowTheFold;
    });
  }

  private computeVisibleRelatedVisits_(): Array<URLVisit> {
    return this.cluster.visit.relatedVisits.filter((visit: URLVisit) => {
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
   * Returns the index of `visit` among the visits in the cluster. Returns -1
   * if the visit is not found in the cluster at all.
   */
  private getVisitIndex_(visit: URLVisit): number {
    if (visit === this.cluster.visit) {
      return 0;
    }

    const relatedVisitIndex = this.cluster.visit.relatedVisits.indexOf(visit);
    if (relatedVisitIndex === -1) {
      return -1;
    }
    // Add one, because the "top visit" is the 0th visit.
    return relatedVisitIndex + 1;
  }
}

customElements.define(HistoryClusterElement.is, HistoryClusterElement);
