// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 *
 * 'settings-search-engine-more-engines-dialog' is shown by
 * 'settings-search-page' in place of 'settings-search-engine-list-dialog', to
 * let the user search additional search engines and choose one as default.
 *
 * It closes when canceled (Cancel or Esc), or when an engine is chosen, in
 * which case it makes the engine default and fires a 'search-engine-changed'
 * event first.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import 'chrome://resources/cr_elements/cr_search_field/cr_search_field.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrSearchFieldElement} from 'chrome://resources/cr_elements/cr_search_field/cr_search_field.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {stripDiacritics} from 'chrome://resources/js/search_highlight_utils.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../i18n_setup.js';

import {getCss} from './search_engine_more_engines_dialog.css.js';
import {getHtml} from './search_engine_more_engines_dialog.html.js';
import type {SearchEngine, SearchEnginesBrowserProxy} from './search_engines_browser_proxy.js';
import {ChoiceMadeLocation, SearchEnginesBrowserProxyImpl} from './search_engines_browser_proxy.js';

export interface SettingsSearchEngineMoreEnginesDialogElement {
  $: {
    dialog: CrDialogElement,
    dialogBody: HTMLElement,
    searchField: CrSearchFieldElement,
  };
}

const SettingsSearchEngineMoreEnginesDialogElementBase =
    WebUiListenerMixinLit(CrLitElement);

export class SettingsSearchEngineMoreEnginesDialogElement extends
    SettingsSearchEngineMoreEnginesDialogElementBase {
  static get is() {
    return 'settings-search-engine-more-engines-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * State of the checkbox to save the search engine choice in guest mode,
       * or null if the checkbox is not displayed. Initialized by the parent,
       * then updated by the checkbox, firing 'save-guest-choice-changed'.
       */
      saveGuestChoice: {
        type: Boolean,
        notify: true,
      },

      /** The engines to search, or null until they are retrieved. */
      engines_: {type: Array},

      /** The search query. */
      query_: {type: String},

      /**
       * ID of the selected engine, or '' if none. Always one of
       * `filteredEngines_`, so that the user can't commit an engine they
       * can't see.
       */
      selectedId_: {type: String},

      /** `engines_` matching `query_`. */
      filteredEngines_: {type: Array},
    };
  }

  accessor saveGuestChoice: boolean|null = null;
  private accessor engines_: SearchEngine[]|null = null;
  private accessor query_: string = '';
  protected accessor selectedId_: string = '';
  protected accessor filteredEngines_: SearchEngine[] = [];

  private browserProxy_: SearchEnginesBrowserProxy =
      SearchEnginesBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.fetchMoreEngines_();
    this.addWebUiListener(
        'search-engines-changed', this.fetchMoreEngines_.bind(this));
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('engines_') ||
        changedPrivateProperties.has('query_')) {
      this.filteredEngines_ = this.computeFilteredEngines_();
      // Don't let the user commit an engine they can't see.
      if (!this.filteredEngines_.some(e => e.id === this.selectedId_)) {
        this.selectedId_ = '';
      }
    }
  }

  override firstUpdated() {
    this.$.dialog.setTitleAriaLabel(
        loadTimeData.getString('searchEnginesMoreEngines'));
  }

  private computeFilteredEngines_(): SearchEngine[] {
    const engines = this.engines_ || [];
    const query = stripDiacritics(this.query_.trim().toLowerCase());
    if (!query) {
      return engines;
    }
    return engines.filter(engine => {
      const terms = [engine.name, engine.keyword];
      return terms.some(
          term => stripDiacritics(term.toLowerCase()).includes(query));
    });
  }

  private async fetchMoreEngines_() {
    this.engines_ = await this.browserProxy_.getMoreSearchEngines();
  }

  protected showNoSearchResults_(): boolean {
    return this.engines_ !== null && !this.filteredEngines_.length;
  }

  protected onSearchChanged_(e: CustomEvent<string>) {
    this.query_ = e.detail;
    // The list otherwise keeps its scroll position, which can hide the first
    // results. The scrolling element is the parent of `#dialogBody`, inside
    // the dialog's shadow root, so `#dialogBody` is scrolled into view instead.
    this.$.dialogBody.scrollIntoView({block: 'start', container: 'nearest'});
  }

  protected onRadioGroupSelectedChanged_(e: CustomEvent<{value: string}>) {
    this.selectedId_ = e.detail.value;
  }

  protected onSaveGuestChoiceCheckedChanged_(e: CustomEvent<{value: boolean}>) {
    this.saveGuestChoice = e.detail.value;
  }

  protected onCancelClick_() {
    this.$.dialog.cancel();
  }

  protected onSetAsDefaultClick_() {
    const searchEngine =
        this.filteredEngines_.find(e => e.id === this.selectedId_);
    assert(searchEngine);

    this.browserProxy_.setDefaultSearchEngine(
        searchEngine.id, ChoiceMadeLocation.SEARCH_SETTINGS,
        this.saveGuestChoice);

    this.fire('search-engine-changed', {searchEngine});
    this.$.dialog.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-search-engine-more-engines-dialog':
        SettingsSearchEngineMoreEnginesDialogElement;
  }
}

customElements.define(
    SettingsSearchEngineMoreEnginesDialogElement.is,
    SettingsSearchEngineMoreEnginesDialogElement);
