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
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Annotation, URLVisit} from './history_clusters.mojom-webui.js';

/**
 * @fileoverview This file provides a custom element displaying a visit to a
 * page within a Cluster. A visit features the page favicon, title, a timestamp,
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
       * The visit to display.
       */
      visit: Object,

      /**
       * Whether the visit is a top visit.
       */
      isTopVisit: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /**
       * Annotations to show for the visit (e.g., whether page was bookmarked).
       */
      annotations_: {
        type: Object,
        computed: 'computeAnnotations_(visit)',
      },

      /**
       * Whether the visit has related visits, regardless of initial visibility.
       */
      hasRelatedVisits_: {
        type: Boolean,
        value: false,
        computed: 'computeHasRelatedVisits_(visit.*)',
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

  isTopVisit: boolean = false;
  visit: URLVisit = new URLVisit();
  private annotations_: Array<string> = [];
  private hasRelatedVisits_: boolean = false;

  //============================================================================
  // Event handlers
  //============================================================================

  private onActionMenuButtonClick_(event: MouseEvent) {
    // Only handle main (usually the left) and auxiliary (usually the wheel or
    // the middle) button presses.
    if (event.button > 1) {
      return;
    }

    this.$.actionMenu.get().showAt(this.$.actionMenuButton);
  }

  private onRemoveAllButtonClick_(event: MouseEvent) {
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
  }

  private onRemoveSelfButtonClick_(event: MouseEvent) {
    // Only handle main (usually the left) and auxiliary (usually the wheel or
    // the middle) button presses.
    if (event.button > 1) {
      return;
    }

    this.dispatchEvent(new CustomEvent('remove-visits', {
      bubbles: true,
      composed: true,
      detail: [this.visit],
    }));

    this.$.actionMenu.get().close();
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

  private computeHasRelatedVisits_(): boolean {
    return this.visit.relatedVisits
               .filter(visit => {
                 // "Ghost" visits with scores of 0 (or below) are never shown.
                 // TODO(tommycli): If there are only ghost visits within the
                 // related visits, and the user deletes the top visit, the
                 // ghost visits don't get deleted and get attached to a nearby
                 // cluster next time. We should fix this semantics issue.
                 return visit.score > 0;
               })
               .length > 0;
  }

  private computeDebugInfo_(): string {
    if (!loadTimeData.getBoolean('isHistoryClustersDebug')) {
      return '';
    }

    return JSON.stringify(this.visit.debugInfo);
  }

  /**
   * Returns the domain name of `url` without the leading 'www.'.
   */
  private getHostnameFromUrl_(url: Url): string {
    return new URL(url.url).hostname.replace(/^(www\.)/, '');
  }
}

customElements.define(VisitRowElement.is, VisitRowElement);
