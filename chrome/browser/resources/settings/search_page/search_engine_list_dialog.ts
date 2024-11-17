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
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../settings_shared.css.js';
import '../site_favicon.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SearchEngine, SearchEnginesBrowserProxy} from '../search_engines_page/search_engines_browser_proxy.js';
import {ChoiceMadeLocation, SearchEnginesBrowserProxyImpl} from '../search_engines_page/search_engines_browser_proxy.js';

import {getTemplate} from './search_engine_list_dialog.html.js';

export interface SettingsSearchEngineListDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

const SettingsSearchEngineListDialogElementBase =
    WebUiListenerMixin(PolymerElement);

export class SettingsSearchEngineListDialogElement extends
    SettingsSearchEngineListDialogElementBase {
  static get is() {
    return 'settings-search-engine-list-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * List of search engines available.
       */
      searchEngines: {
        type: Array,
        observer: 'searchEnginesChanged_',
      },

      /**
       * The id of the search engine that is selected by the user.
       */
      selectedEngineId_: {
        type: String,
        value: '',
      },

      /**
       * Whether the checkbox to save the search engine choice in guest mode
       * should be shown.
       */
      showSaveGuestChoice_: {
        type: Boolean,
        computed: 'computeShowSaveGuestChoice_(saveGuestChoice_)',
      },

      /**
       * State of the checkbox to save the search engine in guest mode. Null if
       * checkbox is not displayed.
       */
      saveGuestChoice_: {
        type: Boolean,
        value: null,
        notify: true,
      },
    };
  }

  searchEngines: SearchEngine[];

  private selectedEngineId_: string;
  private saveGuestChoice_: boolean|null = null;
  private browserProxy_: SearchEnginesBrowserProxy =
      SearchEnginesBrowserProxyImpl.getInstance();

  override ready() {
    super.ready();

    this.browserProxy_.getSaveGuestChoice().then(
        (saveGuestChoice: boolean|null) => {
          this.saveGuestChoice_ = saveGuestChoice;
        });
  }

  private onSetAsDefaultClick_() {
    const searchEngine = this.searchEngines.find(
        engine => engine.id === parseInt(this.selectedEngineId_));
    assert(searchEngine);

    this.browserProxy_.setDefaultSearchEngine(
        searchEngine.modelIndex, ChoiceMadeLocation.SEARCH_SETTINGS,
        this.saveGuestChoice_);

    this.dispatchEvent(new CustomEvent('search-engine-changed', {
      bubbles: true,
      composed: true,
      detail: {
        searchEngine: searchEngine,
      },
    }));
    this.$.dialog.close();
  }

  private onCancelClick_() {
    this.$.dialog.close();
  }

  private searchEnginesChanged_() {
    if (!this.searchEngines.length) {
      return;
    }

    const defaultSearchEngine =
        this.searchEngines.find(searchEngine => searchEngine.default);
    assert(defaultSearchEngine);
    this.selectedEngineId_ = defaultSearchEngine.id.toString();
  }

  private computeShowSaveGuestChoice_(saveGuestChoice: boolean|null): boolean {
    return saveGuestChoice !== null;
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
