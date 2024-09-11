// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/js/util.js';
import './strings.m.js';

import {getToastManager} from 'chrome://resources/cr_elements/cr_toast/cr_toast_manager.js';
import type {CrToolbarElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {BrowserProxy} from './browser_proxy.js';
import type {MojomData} from './data.js';
import type {PageHandlerInterface} from './downloads.mojom-webui.js';
import {SearchService} from './search_service.js';
import {getCss} from './toolbar.css.js';
import {getHtml} from './toolbar.html.js';

export interface DownloadsToolbarElement {
  $: {
    clearAll: HTMLElement,
    toolbar: CrToolbarElement,
  };
}

export class DownloadsToolbarElement extends CrLitElement {
  static get is() {
    return 'downloads-toolbar';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      hasClearableDownloads: {type: Boolean},
      items: {type: Array},
      spinnerActive: {type: Boolean},
    };
  }

  private mojoHandler_: PageHandlerInterface|null = null;
  hasClearableDownloads: boolean = false;
  spinnerActive: boolean = false;
  items: MojomData[] = [];

  override firstUpdated() {
    this.mojoHandler_ = BrowserProxy.getInstance().handler;
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('hasClearableDownloads')) {
      this.updateClearAll_();
    }
  }

  /** @return Whether removal can be undone. */
  canUndo(): boolean {
    return !this.isSearchFocused();
  }

  /** @return Whether "Clear all" should be allowed. */
  canClearAll(): boolean {
    return this.getSearchText().length === 0 && this.hasClearableDownloads;
  }

  /** @return The full text being searched. */
  getSearchText(): string {
    return this.$.toolbar.getSearchField().getValue();
  }

  focusOnSearchInput() {
    this.$.toolbar.getSearchField().showAndFocus();
  }

  isSearchFocused(): boolean {
    return this.$.toolbar.getSearchField().isSearchFocused();
  }

  protected onClearAllClick_(e: Event) {
    assert(this.canClearAll());
    this.mojoHandler_!.clearAll();
    const canUndo =
        this.items.some(data => !data.isDangerous && !data.isInsecure);
    getToastManager().show(
        loadTimeData.getString('toastClearedAll'),
        /* hideSlotted= */ !canUndo);
    // Stop propagating a click to the document to remove toast.
    e.stopPropagation();
    e.preventDefault();
  }

  protected onSearchChanged_(event: CustomEvent<string>) {
    const searchService = SearchService.getInstance();
    if (searchService.search(event.detail)) {
      this.spinnerActive = searchService.isSearching();
      this.dispatchEvent(new CustomEvent('spinner-active-changed', {
        detail: {value: this.spinnerActive},
        bubbles: true,
        composed: true,
      }));
    }
    this.updateClearAll_();
  }

  private updateClearAll_() {
    this.$.clearAll.hidden = !this.canClearAll();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'downloads-toolbar': DownloadsToolbarElement;
  }
}

customElements.define(DownloadsToolbarElement.is, DownloadsToolbarElement);
