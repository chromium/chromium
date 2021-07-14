// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/managed_footnote/managed_footnote.js';
import './item.js';
import './shared_style.js';

import {CrContainerShadowBehavior} from 'chrome://resources/cr_elements/cr_container_shadow_behavior.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {IronA11yAnnouncer} from 'chrome://resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ExtensionsItemElement, ItemDelegate} from './item.js';

type Filter = (info: chrome.developerPrivate.ExtensionInfo) => boolean;

const ExtensionsItemListElementBase =
    mixinBehaviors([CrContainerShadowBehavior, I18nBehavior], PolymerElement) as
    {new (): PolymerElement & I18nBehavior};

class ExtensionsItemListElement extends ExtensionsItemListElementBase {
  static get is() {
    return 'extensions-item-list';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      apps: Array,
      extensions: Array,
      delegate: Object,

      inDevMode: {
        type: Boolean,
        value: false,
      },

      filter: {
        type: String,
      },

      computedFilter_: {
        type: String,
        computed: 'computeFilter_(filter)',
        observer: 'announceSearchResults_',
      },

      maxColumns_: {
        type: Number,
        value: 3,
      },

      shownAppsCount_: {
        type: Number,
        value: 0,
      },

      shownExtensionsCount_: {
        type: Number,
        value: 0,
      },
    };
  }

  apps: Array<chrome.developerPrivate.ExtensionInfo>;
  extensions: Array<chrome.developerPrivate.ExtensionInfo>;
  delegate: ItemDelegate;
  inDevMode: boolean;
  filter: string;
  private computedFilter_: string;
  private maxColumns_: number;
  private shownAppsCount_: number;
  private shownExtensionsCount_: number;

  getDetailsButton(id: string): HTMLElement|null {
    const item =
        this.shadowRoot!.querySelector<ExtensionsItemElement>(`#${id}`);
    return item && item.getDetailsButton();
  }

  getErrorsButton(id: string): HTMLElement|null {
    const item =
        this.shadowRoot!.querySelector<ExtensionsItemElement>(`#${id}`);
    return item && item.getErrorsButton();
  }

  /**
   * Computes the filter function to be used for determining which items
   * should be shown. A |null| value indicates that everything should be
   * shown.
   * return {?Function}
   */
  private computeFilter_(): Filter|null {
    const formattedFilter = this.filter.trim().toLowerCase();
    if (!formattedFilter) {
      return null;
    }

    return i => [i.name, i.id].some(
               s => s.toLowerCase().includes(formattedFilter));
  }

  private shouldShowEmptyItemsMessage_() {
    if (!this.apps || !this.extensions) {
      return;
    }

    return this.apps.length === 0 && this.extensions.length === 0;
  }

  private shouldShowEmptySearchMessage_() {
    return !this.shouldShowEmptyItemsMessage_() && this.shownAppsCount_ === 0 &&
        this.shownExtensionsCount_ === 0;
  }

  private onNoExtensionsTap_(e: Event) {
    if ((e.target as HTMLElement).tagName === 'A') {
      chrome.metricsPrivate.recordUserAction('Options_GetMoreExtensions');
    }
  }

  private announceSearchResults_() {
    if (this.computedFilter_) {
      IronA11yAnnouncer.requestAvailability();
      setTimeout(() => {  // Async to allow list to update.
        const total = this.shownAppsCount_ + this.shownExtensionsCount_;
        this.dispatchEvent(new CustomEvent('iron-announce', {
          bubbles: true,
          composed: true,
          detail: {
            text: this.shouldShowEmptySearchMessage_() ?
                this.i18n('noSearchResults') :
                (total === 1 ?
                     this.i18n('searchResultsSingular', this.filter) :
                     this.i18n(
                         'searchResultsPlural', total.toString(), this.filter)),
          }
        }));
      }, 0);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extensions-item-list': ExtensionsItemListElement;
  }
}

customElements.define(ExtensionsItemListElement.is, ExtensionsItemListElement);
