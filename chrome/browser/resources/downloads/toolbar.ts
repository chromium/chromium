// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/js/util.js';
import 'chrome://resources/polymer/v3_0/paper-styles/color.js';
import './strings.m.js';

import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {getToastManager} from 'chrome://resources/cr_elements/cr_toast/cr_toast_manager.js';
import {CrToolbarElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';
import {MojomData} from './data.js';
import {PageHandlerInterface} from './downloads.mojom-webui.js';
import {SearchService} from './search_service.js';
import {getTemplate} from './toolbar.html.js';

export interface DownloadsToolbarElement {
  $: {
    'moreActionsMenu': CrActionMenuElement,
    'moreActions': CrIconButtonElement,
    'toolbar': CrToolbarElement,
  };
}

export class DownloadsToolbarElement extends PolymerElement {
  static get is() {
    return 'downloads-toolbar';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      hasClearableDownloads: {
        type: Boolean,
        observer: 'updateClearAll_',
      },

      items: Array,

      spinnerActive: {
        type: Boolean,
        notify: true,
      },
    };
  }

  private mojoHandler_: PageHandlerInterface|null = null;
  hasClearableDownloads: boolean = false;
  spinnerActive: boolean;
  items: MojomData[] = [];

  /** @override */
  override ready() {
    super.ready();
    this.mojoHandler_ = BrowserProxy.getInstance().handler;
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

  private onClearAllClick_(e: Event) {
    assert(this.canClearAll());
    this.mojoHandler_!.clearAll();
    this.$.moreActionsMenu.close();
    const canUndo =
        this.items.some(data => !data.isDangerous && !data.isInsecure);
    getToastManager().show(
        loadTimeData.getString('toastClearedAll'),
        /* hideSlotted= */ !canUndo);
    // Stop propagating a click to the document to remove toast.
    e.stopPropagation();
    e.preventDefault();
  }

  private onMoreActionsClick_() {
    this.$.moreActionsMenu.showAt(this.$.moreActions);
  }

  private onSearchChanged_(event: CustomEvent<string>) {
    const searchService = SearchService.getInstance();
    if (searchService.search(event.detail)) {
      this.spinnerActive = searchService.isSearching();
    }
    this.updateClearAll_();
  }

  private onOpenDownloadsFolderClick_() {
    this.mojoHandler_!.openDownloadsFolderRequiringGesture();
    this.$.moreActionsMenu.close();
  }

  private updateClearAll_() {
    this.shadowRoot!.querySelector<HTMLButtonElement>('.clear-all')!.hidden =
        !this.canClearAll();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'downloads-toolbar': DownloadsToolbarElement;
  }
}

customElements.define(DownloadsToolbarElement.is, DownloadsToolbarElement);
