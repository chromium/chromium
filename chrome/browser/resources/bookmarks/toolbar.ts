// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_selection_overlay.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '/strings.m.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';

import type {CrToolbarElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import type {CrToolbarSearchFieldElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {setSearchTerm} from './actions.js';
import {BookmarksCommandManagerElement} from './command_manager.js';
import {Command, MenuSource} from './constants.js';
import {StoreClientMixinLit} from './store_client_mixin_lit.js';
import {getCss} from './toolbar.css.js';
import {getHtml} from './toolbar.html.js';
import type {BookmarksPageState} from './types.js';

const BookmarksToolbarElementBase = StoreClientMixinLit(CrLitElement);

export class BookmarksToolbarElement extends BookmarksToolbarElementBase {
  static get is() {
    return 'bookmarks-toolbar';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      sidebarWidth: {type: String},
      showSelectionOverlay: {type: Boolean},

      narrow_: {
        type: Boolean,
        reflect: true,
      },

      searchTerm_: {type: String},
      selectedItems_: {type: Object},
      globalCanEdit_: {type: Boolean},
    };
  }

  accessor sidebarWidth: string = '';
  accessor showSelectionOverlay: boolean = false;
  protected accessor narrow_: boolean = false;
  private accessor searchTerm_: string = '';
  private accessor selectedItems_: Set<string> = new Set();
  private accessor globalCanEdit_: boolean = false;

  override connectedCallback() {
    super.connectedCallback();
    this.updateFromStore();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('selectedItems_') ||
        changedPrivateProperties.has('globalCanEdit_')) {
      this.showSelectionOverlay =
          this.selectedItems_.size > 1 && this.globalCanEdit_;
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    if (changedProperties.has('sidebarWidth')) {
      this.style.setProperty('--sidebar-width', this.sidebarWidth);
    }

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('searchTerm_')) {
      // Note: searchField getter accesses the DOM.
      this.searchField.setValue(this.searchTerm_ || '');
    }
  }

  override onStateChanged(state: BookmarksPageState) {
    this.searchTerm_ = state.search.term;
    this.selectedItems_ = state.selection.items;
    this.globalCanEdit_ = state.prefs.canEdit;
  }

  get searchField(): CrToolbarSearchFieldElement {
    return this.shadowRoot.querySelector<CrToolbarElement>(
                              'cr-toolbar')!.getSearchField();
  }

  protected onMenuButtonOpenClick_(e: Event) {
    this.fire('open-command-menu', {
      targetElement: e.target,
      source: MenuSource.TOOLBAR,
    });
  }

  protected onDeleteSelectionClick_() {
    const selection = this.selectedItems_;
    const commandManager = BookmarksCommandManagerElement.getInstance();
    assert(commandManager.canExecute(Command.DELETE, selection));
    commandManager.handle(Command.DELETE, selection);
  }

  protected onClearSelectionClick_() {
    const commandManager = BookmarksCommandManagerElement.getInstance();
    assert(
        commandManager.canExecute(Command.DESELECT_ALL, this.selectedItems_));
    commandManager.handle(Command.DESELECT_ALL, this.selectedItems_);
  }

  protected onSearchChanged_(e: CustomEvent<string>) {
    if (e.detail !== this.searchTerm_) {
      this.dispatch(setSearchTerm(e.detail));
    }
  }

  protected onNarrowChanged_(e: CustomEvent<{value: boolean}>) {
    this.narrow_ = e.detail.value;
  }

  protected canDeleteSelection_(): boolean {
    return this.showSelectionOverlay &&
        BookmarksCommandManagerElement.getInstance().canExecute(
            Command.DELETE, this.selectedItems_);
  }

  protected getItemsSelectedString_(): string {
    return loadTimeData.getStringF('itemsSelected', this.selectedItems_.size);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'bookmarks-toolbar': BookmarksToolbarElement;
  }
}

customElements.define(BookmarksToolbarElement.is, BookmarksToolbarElement);
