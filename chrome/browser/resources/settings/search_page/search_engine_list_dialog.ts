// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 *
 * 'settings-search-engine-list-dialog' is the dialog shown for displaying the
 * list of search engines from which the user can choose a default.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import './search_engine_icon.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './search_engine_list_dialog.css.js';
import {getHtml} from './search_engine_list_dialog.html.js';
import type {SearchEngine, SearchEnginesBrowserProxy} from './search_engines_browser_proxy.js';
import {ChoiceMadeLocation, SearchEnginesBrowserProxyImpl} from './search_engines_browser_proxy.js';

export interface SettingsSearchEngineListDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

const SettingsSearchEngineListDialogElementBase =
    WebUiListenerMixinLit(CrLitElement);

export class SettingsSearchEngineListDialogElement extends
    SettingsSearchEngineListDialogElementBase {
  static get is() {
    return 'settings-search-engine-list-dialog';
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
       * List of search engines available.
       */
      searchEngines: {type: Array},

      /**
       * The id of the search engine that is selected by the user.
       */
      selectedEngineId_: {type: String},

      /**
       * Whether the checkbox to save the search engine choice in guest mode
       * should be shown.
       */
      showSaveGuestChoice_: {type: Boolean},

      /**
       * State of the checkbox to save the search engine in guest mode. Null if
       * checkbox is not displayed.
       */
      saveGuestChoice_: {type: Boolean},
    };
  }

  accessor searchEngines: SearchEngine[] = [];
  protected accessor selectedEngineId_: string = '';
  protected accessor saveGuestChoice_: boolean|null = null;
  protected accessor showSaveGuestChoice_: boolean = false;

  private browserProxy_: SearchEnginesBrowserProxy =
      SearchEnginesBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.browserProxy_.getSaveGuestChoice().then(
        (saveGuestChoice: boolean|null) => {
          this.saveGuestChoice_ = saveGuestChoice;
        });
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('searchEngines')) {
      this.searchEnginesChanged_();
    }

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('saveGuestChoice_')) {
      this.showSaveGuestChoice_ = this.saveGuestChoice_ !== null;
    }
  }

  protected onSetAsDefaultClick_() {
    const searchEngine = this.searchEngines.find(
        engine => engine.id === parseInt(this.selectedEngineId_));
    assert(searchEngine);

    this.browserProxy_.setDefaultSearchEngine(
        searchEngine.id, ChoiceMadeLocation.SEARCH_SETTINGS,
        this.saveGuestChoice_);

    this.fire('search-engine-changed', {searchEngine});
    this.$.dialog.close();
  }

  protected onCancelClick_() {
    this.$.dialog.close();
  }

  protected onDialogCancel_() {
    this.$.dialog.close();
  }

  private searchEnginesChanged_() {
    if (!this.searchEngines.length) {
      this.selectedEngineId_ = '';
      return;
    }

    const defaultSearchEngine =
        this.searchEngines.find(searchEngine => searchEngine.default);
    assert(defaultSearchEngine);
    this.selectedEngineId_ = defaultSearchEngine.id.toString();
  }

  protected onRadioGroupSelectedChanged_(e: CustomEvent<{value: string}>) {
    this.selectedEngineId_ = e.detail.value;
  }

  protected onSaveGuestChoiceCheckedChanged_(e: CustomEvent<{value: boolean}>) {
    this.saveGuestChoice_ = e.detail.value;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-search-engine-list-dialog': SettingsSearchEngineListDialogElement;
  }
}

customElements.define(
    SettingsSearchEngineListDialogElement.is,
    SettingsSearchEngineListDialogElement);
