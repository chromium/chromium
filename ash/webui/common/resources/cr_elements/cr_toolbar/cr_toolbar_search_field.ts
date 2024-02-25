// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Forked from
// ui/webui/resources/cr_elements/cr_toolbar/cr_toolbar_search_field.ts

import '../cr_icon_button/cr_icon_button.js';
import '../cr_icons.css.js';
import '../icons.html.js';
import '../cr_shared_style.css.js';
import '../cr_shared_vars.css.js';
import '//resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';

import {DomIf, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrSearchFieldMixin} from '../cr_search_field/cr_search_field_mixin.js';

import {getTemplate} from './cr_toolbar_search_field.html.js';

export interface CrToolbarSearchFieldElement {
  $: {
    searchInput: HTMLInputElement,
    searchTerm: HTMLElement,
    spinnerTemplate: DomIf,
  };
}

const CrToolbarSearchFieldElementBase = CrSearchFieldMixin(PolymerElement);

export class CrToolbarSearchFieldElement extends
    CrToolbarSearchFieldElementBase {
  static get is() {
    return 'cr-toolbar-search-field';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      narrow: {
        type: Boolean,
        reflectToAttribute: true,
      },

      showingSearch: {
        type: Boolean,
        value: false,
        notify: true,
        observer: 'showingSearchChanged_',
        reflectToAttribute: true,
      },

      disabled: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      autofocus: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      // When true, show a loading spinner to indicate that the backend is
      // processing the search. Will only show if the search field is open.
      spinnerActive: {type: Boolean, reflectToAttribute: true},

      isSpinnerShown_: {
        type: Boolean,
        computed: 'computeIsSpinnerShown_(spinnerActive, showingSearch)',
      },

      searchFocused_: {reflectToAttribute: true, type: Boolean, value: false},
    };
  }

  narrow: boolean;
  showingSearch: boolean;
  disabled: boolean;
  override autofocus: boolean;
  spinnerActive: boolean;
  private isSpinnerShown_: boolean;
  private searchFocused_: boolean;

  override ready() {
    super.ready();
    this.addEventListener('click', e => this.showSearch_(e));
  }

  override getSearchInput(): HTMLInputElement {
    return this.$.searchInput;
  }

  isSearchFocused(): boolean {
    return this.searchFocused_;
  }

  showAndFocus() {
    this.showingSearch = true;
    this.focus_();
  }

  override onSearchTermInput() {
    super.onSearchTermInput();
    this.showingSearch = this.hasSearchText || this.isSearchFocused();
  }

  private onSearchIconClicked_() {
    this.dispatchEvent(new CustomEvent(
        'search-icon-clicked', {bubbles: true, composed: true}));
  }

  private focus_() {
    this.getSearchInput().focus();
  }

  private computeIconTabIndex_(narrow: boolean): number {
    return narrow && !this.hasSearchText ? 0 : -1;
  }

  private computeIconAriaHidden_(narrow: boolean): string {
    return Boolean(!narrow || this.hasSearchText).toString();
  }

  private computeIsSpinnerShown_(): boolean {
    const showSpinner = this.spinnerActive && this.showingSearch;
    if (showSpinner) {
      this.$.spinnerTemplate.if = true;
    }
    return showSpinner;
  }

  private onInputFocus_() {
    this.searchFocused_ = true;
  }

  private onInputBlur_() {
    this.searchFocused_ = false;
    if (!this.hasSearchText) {
      this.showingSearch = false;
    }
  }

  private onSearchTermKeydown_(e: KeyboardEvent) {
    if (e.key === 'Escape') {
      this.showingSearch = false;
    }
  }

  private showSearch_(e: Event) {
    if (e.target !== this.shadowRoot!.querySelector('#clearSearch')) {
      this.showingSearch = true;
    }
  }

  private clearSearch_() {
    this.setValue('');
    this.focus_();
    this.spinnerActive = false;
  }

  private showingSearchChanged_(_current: boolean, previous?: boolean) {
    // Prevent unnecessary 'search-changed' event from firing on startup.
    if (previous === undefined) {
      return;
    }

    if (this.showingSearch) {
      this.focus_();
      return;
    }

    this.setValue('');
    this.getSearchInput().blur();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-toolbar-search-field': CrToolbarSearchFieldElement;
  }
}

customElements.define(
    CrToolbarSearchFieldElement.is, CrToolbarSearchFieldElement);
