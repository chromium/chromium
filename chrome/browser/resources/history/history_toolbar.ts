// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://resources/cr_components/history_embeddings/icons.html.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_selection_overlay.js';

import type {HistoryQuery} from 'chrome://resources/cr_components/history/history.mojom-webui.js';
import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import type {CrToolbarElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import type {CrToolbarSearchFieldElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './history_toolbar.css.js';
import {getHtml} from './history_toolbar.html.js';
import {TABBED_PAGES} from './router.js';

export interface HistoryToolbarElement {
  $: {
    mainToolbar: CrToolbarElement,
  };
}

export class HistoryToolbarElement extends CrLitElement {
  static get is() {
    return 'history-toolbar';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      // Number of history items currently selected.
      // TODO(calamity): bind this to
      // listContainer.selectedItem.selectedPaths.length.
      count: {type: Number},

      // True if 1 or more history items are selected. When this value changes
      // the background colour changes.
      itemsSelected_: {type: Boolean},

      pendingDelete: {type: Boolean},

      // The most recent term entered in the search field. Updated incrementally
      // as the user types.
      searchTerm: {type: String},

      selectedPage: {type: String},

      // True if the backend is processing and a spinner should be shown in the
      // toolbar.
      spinnerActive: {type: Boolean},

      hasDrawer: {
        type: Boolean,
        reflect: true,
      },

      hasMoreResults: {type: Boolean},

      querying: {type: Boolean},

      queryInfo: {type: Object},

      // Whether to show the menu promo (a tooltip that points at the menu
      // button
      // in narrow mode).
      showMenuPromo: {type: Boolean},
    };
  }

  accessor count: number = 0;
  accessor pendingDelete: boolean = false;
  accessor searchTerm: string = '';
  accessor selectedPage: string = '';
  accessor hasDrawer: boolean = false;
  accessor hasMoreResults: boolean = false;
  accessor querying: boolean = false;
  accessor queryInfo: HistoryQuery|undefined;
  accessor spinnerActive: boolean = false;
  accessor showMenuPromo: boolean = false;
  protected accessor itemsSelected_: boolean = false;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('count')) {
      this.changeToolbarView_();
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    // Querying and modifying the DOM should happen in updated().
    if (changedProperties.has('searchTerm')) {
      this.searchTermChanged_();
    }
  }

  get searchField(): CrToolbarSearchFieldElement {
    return this.$.mainToolbar.getSearchField();
  }

  deleteSelectedItems() {
    this.fire('delete-selected');
  }

  openSelectedItems() {
    this.fire('open-selected');
  }

  clearSelectedItems() {
    this.fire('unselect-all');
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

  protected onSearchChanged_(event: CustomEvent<string>) {
    this.fire(
        'change-query',
        {search: event.detail, /* Prevent updating after date. */ after: null});
  }

  protected numberOfItemsSelected_(count: number): string {
    return count > 0 ? loadTimeData.getStringF('itemsSelected', count) : '';
  }

  protected computeSearchIconOverride_(): string|undefined {
    if (loadTimeData.getBoolean('enableHistoryEmbeddings') &&
        TABBED_PAGES.includes(this.selectedPage)) {
      return 'history-embeddings:search';
    }

    return undefined;
  }

  protected computeSearchInputAriaDescriptionOverride_(): string|undefined {
    if (loadTimeData.getBoolean('enableHistoryEmbeddings') &&
        TABBED_PAGES.includes(this.selectedPage)) {
      return loadTimeData.getString('historyEmbeddingsDisclaimer');
    }

    return undefined;
  }

  protected computeSearchPrompt_(): string {
    if (loadTimeData.getBoolean('enableHistoryEmbeddings') &&
        TABBED_PAGES.includes(this.selectedPage)) {
      if (loadTimeData.getBoolean('enableHistoryEmbeddingsAnswers')) {
        const possiblePrompts = [
          'historyEmbeddingsSearchPrompt',
          'historyEmbeddingsAnswersSearchAlternativePrompt1',
          'historyEmbeddingsAnswersSearchAlternativePrompt2',
          'historyEmbeddingsAnswersSearchAlternativePrompt3',
          'historyEmbeddingsAnswersSearchAlternativePrompt4',
        ];
        const randomIndex = Math.floor(Math.random() * possiblePrompts.length);
        return loadTimeData.getString(possiblePrompts[randomIndex]);
      }

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
