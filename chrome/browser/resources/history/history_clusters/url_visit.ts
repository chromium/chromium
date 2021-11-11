// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './page_favicon.js';
import './shared_style.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';

import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import {Annotation, PageHandlerRemote, URLVisit} from './history_clusters.mojom-webui.js';
import {ClusterAction, MetricsProxyImpl, VisitAction, VisitType} from './metrics_proxy.js';
import {OpenWindowProxyImpl} from './open_window_proxy.js';

/**
 * @fileoverview This file provides a custom element displaying a visit to a
 * page within a cluster. A visit features the page favicon, title, a timestamp,
 * as well as an action menu.
 */

/**
 * Maps supported annotations to localized string identifiers.
 */
const annotationToStringId: Map<number, string> = new Map([
  [Annotation.kBookmarked, 'bookmarked'],
  [Annotation.kTabGrouped, 'savedInTabGroup'],
]);

declare global {
  interface HTMLElementTagNameMap {
    'url-visit': VisitRowElement;
  }
}

interface VisitRowElement {
  $: {
    actionMenu: CrLazyRenderElement<CrActionMenuElement>,
    actionMenuButton: Element,
  };
}

class VisitRowElement extends PolymerElement {
  static get is() {
    return 'url-visit';
  }

  static get template() {
    return html`{__html_template__}`;
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
       * Whether this is a top visit.
       */
      isTopVisit: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },

      /**
       * The index of the visit in the cluster.
       */
      index: {
        type: Number,
        value: -1,  // Initialized to an invalid value.
      },

      /**
       * The visit to display.
       */
      visit: Object,

      /**
       * Annotations to show for the visit (e.g., whether page was bookmarked).
       */
      annotations_: {
        type: Object,
        computed: 'computeAnnotations_(visit)',
      },

      /**
       * Debug info for the visit.
       */
      debugInfo_: {
        type: String,
        computed: 'computeDebugInfo_(visit)',
      },
    };
  }

  //============================================================================
  // Properties
  //============================================================================

  clusterIndex: number;
  index: number;
  visit: URLVisit;
  private pageHandler_: PageHandlerRemote;

  //============================================================================
  // Overridden methods
  //============================================================================

  constructor() {
    super();
    this.pageHandler_ = BrowserProxyImpl.getInstance().handler;
  }

  //============================================================================
  // Event handlers
  //============================================================================

  private onActionMenuButtonClick_(event: MouseEvent) {
    this.$.actionMenu.get().showAt(this.$.actionMenuButton);
    event.preventDefault();  // Prevent default browser action (navigation).
  }

  private onAuxClick_() {
    MetricsProxyImpl.getInstance().recordVisitAction(
        VisitAction.CLICKED, this.index, this.getVisitType_());

    // Notify the parent <history-cluster> element of this event.
    this.dispatchEvent(new CustomEvent('visit-clicked', {
      bubbles: true,
      composed: true,
    }));
  }

  private onClick_(event: MouseEvent) {
    // Ignore previously handled events.
    if (event.defaultPrevented) {
      return;
    }

    event.preventDefault();  // Prevent default browser action (navigation).

    // To record metrics.
    this.onAuxClick_();

    OpenWindowProxyImpl.getInstance().open(this.visit.normalizedUrl.url);
  }

  private onKeydown_(e: KeyboardEvent) {
    // To be consistent with <history-list>, only handle Enter, and not Space.
    if (e.key !== 'Enter') {
      return;
    }

    // To record metrics.
    this.onAuxClick_();

    OpenWindowProxyImpl.getInstance().open(this.visit.normalizedUrl.url);
  }

  private onOpenAllButtonClick_() {
    this.pageHandler_.openVisitUrlsInTabGroup(
        [this.visit, ...this.visit.relatedVisits]);

    this.$.actionMenu.get().close();

    MetricsProxyImpl.getInstance().recordClusterAction(
        ClusterAction.OPENED_IN_TAB_GROUP, this.clusterIndex);
  }

  private onRemoveAllButtonClick_() {
    this.dispatchEvent(new CustomEvent('remove-visits', {
      bubbles: true,
      composed: true,
      detail: [this.visit, ...this.visit.relatedVisits],
    }));

    this.$.actionMenu.get().close();
  }

  private onRemoveSelfButtonClick_() {
    this.dispatchEvent(new CustomEvent('remove-visits', {
      bubbles: true,
      composed: true,
      detail: [this.visit],
    }));

    this.$.actionMenu.get().close();

    MetricsProxyImpl.getInstance().recordVisitAction(
        VisitAction.DELETED, this.index, this.getVisitType_());
  }

  //============================================================================
  // Helper methods
  //============================================================================

  private computeAnnotations_(): Array<string> {
    return this.visit.annotations
        .map((annotation: number) => annotationToStringId.get(annotation))
        .filter(
            (id: string|undefined):
                id is string => {
                  return !!id;
                })
        .map((id: string) => loadTimeData.getString(id));
  }

  private computeDebugInfo_(): string {
    if (!loadTimeData.getBoolean('isHistoryClustersDebug')) {
      return '';
    }

    return JSON.stringify(this.visit.debugInfo);
  }

  /**
   * Returns the domain name without the leading 'www.', if applicable.
   */
  private getHostname_(_visit: URLVisit): string {
    try {
      return new URL(this.visit.normalizedUrl.url)
          .hostname.replace(/^(www\.)/, '')
          .trim();
    } catch (err) {
      return '';
    }
  }

  /**
   * Returns the VisitType based on whether this is a visit to the default
   * search provider's results page.
   */
  private getVisitType_(): VisitType {
    return this.visit.annotations.includes(Annotation.kSearchResultsPage) ?
        VisitType.SRP :
        VisitType.NON_SRP;
  }
}

customElements.define(VisitRowElement.is, VisitRowElement);
