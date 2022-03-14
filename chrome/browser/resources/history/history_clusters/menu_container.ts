// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shared_style.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';

import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Annotation, URLVisit} from './history_clusters.mojom-webui.js';
import {getTemplate} from './menu_container.html.js';
import {MetricsProxyImpl, VisitAction, VisitType} from './metrics_proxy.js';

/**
 * @fileoverview This file provides a custom element displaying an action menu.
 * It's meant to be flexible enough to be associated with either a specific
 * visit, or the whole cluster, or the top visit of unlabelled cluster.
 */

declare global {
  interface HTMLElementTagNameMap {
    'menu-container': MenuContainerElement;
  }
}

interface MenuContainerElement {
  $: {
    actionMenu: CrLazyRenderElement<CrActionMenuElement>,
    actionMenuButton: Element,
  };
}

class MenuContainerElement extends PolymerElement {
  static get is() {
    return 'menu-container';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The index of the cluster this menu belongs to.
       */
      clusterIndex: {
        type: Number,
        value: -1,  // Initialized to an invalid value.
      },

      /**
       * The index of the visit in the cluster.
       */
      visitIndex: {
        type: Number,
        value: -1,  // Initialized to an invalid value.
      },

      /**
       * The visit associated with this menu.
       */
      visit: Object,
    };
  }

  //============================================================================
  // Properties
  //============================================================================

  clusterIndex: number;
  visitIndex: number;
  visit: URLVisit;

  //============================================================================
  // Event handlers
  //============================================================================

  private onActionMenuButtonClick_(event: MouseEvent) {
    this.$.actionMenu.get().showAt(this.$.actionMenuButton);
    event.preventDefault();  // Prevent default browser action (navigation).
  }

  private onOpenAllButtonClick_(event: MouseEvent) {
    event.preventDefault();  // Prevent default browser action (navigation).

    this.dispatchEvent(new CustomEvent('open-all-visits', {
      bubbles: true,
      composed: true,
    }));

    this.$.actionMenu.get().close();
  }

  private onRemoveAllButtonClick_(event: MouseEvent) {
    event.preventDefault();  // Prevent default browser action (navigation).

    this.dispatchEvent(new CustomEvent('remove-visits', {
      bubbles: true,
      composed: true,
      detail: [this.visit, ...this.visit.relatedVisits],
    }));

    this.$.actionMenu.get().close();
  }

  private onRemoveSelfButtonClick_(event: MouseEvent) {
    event.preventDefault();  // Prevent default browser action (navigation).

    this.dispatchEvent(new CustomEvent('remove-visits', {
      bubbles: true,
      composed: true,
      detail: [this.visit],
    }));

    this.$.actionMenu.get().close();

    MetricsProxyImpl.getInstance().recordVisitAction(
        VisitAction.DELETED, this.visitIndex, this.getVisitType_());
  }

  //============================================================================
  // Helper methods
  //============================================================================

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

customElements.define(MenuContainerElement.is, MenuContainerElement);
