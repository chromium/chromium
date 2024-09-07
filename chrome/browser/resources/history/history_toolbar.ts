// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shared_style.css.js';
import './strings.m.js';
import 'chrome://resources/cr_components/history_embeddings/icons.html.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_selection_overlay.js';

import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import type {CrToolbarElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import type {CrToolbarSearchFieldElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './history_toolbar.html.js';
import {TABBED_PAGES} from './router.js';

export interface HistoryToolbarElement {
  $: {
    mainToolbar: CrToolbarElement,
  };
}

export class HistoryToolbarElement extends PolymerElement {
  static get is() {
    return 'history-toolbar';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      // Number of history items currently selected.
      // TODO(calamity): bind this to
      // listContainer.selectedItem.selectedPaths.length.
      count: {
        type: Number,
        observer: 'changeToolbarView_',
      },

      // True if 1 or more history items are selected. When this value changes
      // the background colour changes.
      itemsSelected_: Boolean,

      pendingDelete: Boolean,

      searchIconOverride_: {
        type: String,
        computed: 'computeSearchIconOverride_(selectedPage)',
      },

      searchInputAriaDescription_: {
        type: String,
        computed: 'computeSearchInputAriaDescriptionOverride_(selectedPage)',
      },

      searchPrompt_: {
        type: String,
        computed: 'computeSearchPrompt_(selectedPage)',
      },

      // The most recent term entered in the search field. Updated incrementally
      // as the user types.
      searchTerm: {
        type: String,
        observer: 'searchTermChanged_',
      },

      selectedPage: String,

      // True if the backend is processing and a spinner should be shown in the
      // toolbar.
      spinnerActive: {
        type: Boolean,
        value: false,
      },

      hasDrawer: {
        type: Boolean,
        reflectToAttribute: true,
      },

      hasMoreResults: Boolean,

      querying: Boolean,

      queryInfo: Object,

      // Whether to show the menu promo (a tooltip that points at the menu
      // button
      // in narrow mode).
      showMenuPromo: Boolean,
    };
  }

  count: number = 0;
  private searchIconOverride_?: string;
  private searchInputAriaDescription_?: string;
  private searchPrompt_: string;
  searchTerm: string;
  selectedPage: string;
  spinnerActive: boolean;
  showMenuPromo: boolean;
  private itemsSelected_: boolean = false;

  private fire_(eventName: string, detail?: any) {
    this.dispatchEvent(
        new CustomEvent(eventName, {bubbles: true, composed: true, detail}));
  }

  get searchField(): CrToolbarSearchFieldElement {
    return this.$.mainToolbar.getSearchField();
  }

  deleteSelectedItems() {
    this.fire_('delete-selected');
  }

  clearSelectedItems() {
    this.fire_('unselect-all');
    getAnnouncerInstance().announce(loadTimeData.getString('itemsUnselected'));
  }

  /**
   * Changes the toolbar background color depending on whether any history items
   * are currently selected.
   */
  private changeToolbarView_() {
    this.itemsSelected_ = this.count > 0;
  }

  /**
   * When changing the search term externally, update the search field to
   * reflect the new search term.
   */
  private searchTermChanged_() {
    if (this.searchField.getValue() !== this.searchTerm) {
      this.searchField.showAndFocus();
      this.searchField.setValue(this.searchTerm);
    }
  }

  private canShowMenuPromo_(): boolean {
    return this.showMenuPromo && !loadTimeData.getBoolean('isGuestSession');
  }

  private onSearchChanged_(event: CustomEvent<string>) {
    this.fire_(
        'change-query',
        {search: event.detail, /* Prevent updating after date. */ after: null});
  }

  private numberOfItemsSelected_(count: number): string {
    return count > 0 ? loadTimeData.getStringF('itemsSelected', count) : '';
  }

  private computeSearchIconOverride_(): string|undefined {
    if (loadTimeData.getBoolean('enableHistoryEmbeddings') &&
        TABBED_PAGES.includes(this.selectedPage)) {
      return 'history-embeddings:search';
    }

    return undefined;
  }

  private computeSearchInputAriaDescriptionOverride_(): string|undefined {
    if (loadTimeData.getBoolean('enableHistoryEmbeddings') &&
        TABBED_PAGES.includes(this.selectedPage)) {
      return loadTimeData.getString('historyEmbeddingsDisclaimer');
    }

    return undefined;
  }

  private computeSearchPrompt_(): string {
    if (loadTimeData.getBoolean('enableHistoryEmbeddings') &&
        TABBED_PAGES.includes(this.selectedPage)) {
      return loadTimeData.getString('historyEmbeddingsSearchPrompt');
    }

    return loadTimeData.getString('searchPrompt');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'history-toolbar': HistoryToolbarElement;
  }
}

customElements.define(HistoryToolbarElement.is, HistoryToolbarElement);
