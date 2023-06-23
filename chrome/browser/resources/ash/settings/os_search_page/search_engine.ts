// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-search-engine' is the settings module for setting
 * the preferred search engine.
 */
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/policy/cr_policy_pref_indicator.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import './os_search_selection_dialog.js';
import '/shared/settings/controls/extension_controlled_indicator.js';
import '/shared/settings/controls/controlled_button.js';
import '/shared/settings/controls/settings_toggle_button.js';
import 'chrome://resources/cr_components/settings_prefs/prefs.js';
import 'chrome://resources/cr_components/settings_prefs/pref_util.js';
import '../settings_shared.css.js';
import '../settings_vars.css.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';

import {getTemplate} from './search_engine.html.js';
import {SearchEngine, SearchEnginesBrowserProxy, SearchEnginesBrowserProxyImpl, SearchEnginesInfo} from './search_engines_browser_proxy.js';

const SettingsSearchEngineElementBase =
    I18nMixin(WebUiListenerMixin(PolymerElement));

class SettingsSearchEngineElement extends SettingsSearchEngineElementBase {
  static get is() {
    return 'settings-search-engine';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: Object,

      /** The current selected search engine. */
      currentSearchEngine_: Object,
    };
  }

  private browserProxy_: SearchEnginesBrowserProxy;
  private currentSearchEngine_: SearchEngine;

  constructor() {
    super();

    this.browserProxy_ = SearchEnginesBrowserProxyImpl.getInstance();
  }

  override ready() {
    super.ready();

    this.browserProxy_.getSearchEnginesList().then(
        this.updateCurrentSearchEngine_.bind(this));
    this.addWebUiListener(
        'search-engines-changed', this.updateCurrentSearchEngine_.bind(this));
  }

  private updateCurrentSearchEngine_(searchEngines: SearchEnginesInfo) {
    const defaultSearchEngine = castExists(
        searchEngines.defaults.find(searchEngine => searchEngine.default));
    this.currentSearchEngine_ = defaultSearchEngine;
  }

  override focus() {
    this.getBrowserSearchSettingsLink_().focus();
  }

  private onDisableExtension_() {
    const event = new CustomEvent('refresh-pref', {
      bubbles: true,
      composed: true,
      detail: 'default_search_provider.enabled',
    });
    this.dispatchEvent(event);
  }

  private onSearchEngineLinkClick_() {
    this.browserProxy_.openBrowserSearchSettings();
  }

  private getBrowserSearchSettingsLink_() {
    return castExists(
        this.shadowRoot!.getElementById('browserSearchSettingsLink'));
  }

  private getSearchSelectionDialogButton_() {
    return castExists(
        this.shadowRoot!.getElementById('searchSelectionDialogButton'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-search-engine': SettingsSearchEngineElement;
  }
}

customElements.define(
    SettingsSearchEngineElement.is, SettingsSearchEngineElement);
