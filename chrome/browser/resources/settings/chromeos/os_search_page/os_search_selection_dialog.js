// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-search-selection-dialog' is a dialog for setting
 * the preferred search engine.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/md_select.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../../settings_shared.css.js';

import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/cr_elements/web_ui_listener_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SearchEngine, SearchEnginesBrowserProxy, SearchEnginesBrowserProxyImpl, SearchEnginesInfo} from './search_engines_browser_proxy.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {WebUIListenerBehaviorInterface}
 */
const OsSettingsSearchSelectionDialogElementBase =
    mixinBehaviors([WebUIListenerBehavior], PolymerElement);

/** @polymer */
class OsSettingsSearchSelectionDialogElement extends
    OsSettingsSearchSelectionDialogElementBase {
  static get is() {
    return 'os-settings-search-selection-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * List of default search engines available.
       * @private {!Array<!SearchEngine>}
       */
      searchEngines_: {
        type: Array,
        value() {
          return [];
        },
      },
    };
  }

  /** @override */
  constructor() {
    super();

    /** @private {!SearchEnginesBrowserProxy} */
    this.browserProxy_ = SearchEnginesBrowserProxyImpl.getInstance();
  }

  /** @override */
  ready() {
    super.ready();

    this.browserProxy_.getSearchEnginesList().then(
        this.updateSearchEngines_.bind(this));
    this.addWebUIListener(
        'search-engines-changed', this.updateSearchEngines_.bind(this));
  }

  /**
   * @param {!SearchEnginesInfo} searchEngines
   * @private
   */
  updateSearchEngines_(searchEngines) {
    this.set('searchEngines_', searchEngines.defaults);
  }

  /**
   * Enables the checked languages.
   * @private
   */
  onActionButtonClick_() {
    const select = /** @type {!HTMLSelectElement} */ (
        this.shadowRoot.querySelector('select'));
    const searchEngine = this.searchEngines_[select.selectedIndex];
    this.browserProxy_.setDefaultSearchEngine(searchEngine.modelIndex);

    this.$.dialog.close();
  }

  /** @private */
  onCancelButtonClick_() {
    this.$.dialog.close();
  }

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onKeydown_(e) {
    if (e.key === 'Escape') {
      this.onCancelButtonClick_();
    }
  }
}

customElements.define(
    OsSettingsSearchSelectionDialogElement.is,
    OsSettingsSearchSelectionDialogElement);
