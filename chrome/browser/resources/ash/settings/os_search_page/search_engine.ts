// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-search-engine' is the settings module for setting
 * the preferred search engine.
 */
import 'chrome://resources/ash/common/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/ash/common/cr_elements/policy/cr_policy_pref_indicator.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '../settings_shared.css.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';

import {getTemplate} from './search_engine.html.js';
import type {CategorizedTemplateUrls, SearchEngine, SearchEnginesBrowserProxy, SearchEnginesInfo} from './search_engines_browser_proxy.js';
import {SearchEnginesBrowserProxyImpl} from './search_engines_browser_proxy.js';

const SettingsSearchEngineElementBase =
    I18nMixin(WebUiListenerMixin(PolymerElement));

export class SettingsSearchEngineElement extends
    SettingsSearchEngineElementBase {
  static get is() {
    return 'settings-search-engine' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
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

  override connectedCallback() {
    super.connectedCallback();

    if (loadTimeData.getBoolean('searchSettingsUpdate')) {
      this.browserProxy_.getCategorizedTemplateUrls().then(
          this.updateCurrentSearchEngineFromCategorizedUrls_.bind(this));
      this.addWebUiListener(
          'search-engines-changed',
          this.updateCurrentSearchEngineFromCategorizedUrls_.bind(this));
      return;
    }

    this.browserProxy_.getSearchEnginesList().then(
        this.updateCurrentSearchEngineFromDefaults_.bind(this));
    this.addWebUiListener(
        'search-engines-changed',
        this.updateCurrentSearchEngineFromDefaults_.bind(this));
  }

  private updateCurrentSearchEngineFromDefaults_(
      searchEngines: SearchEnginesInfo): void {
    const defaultSearchEngine = castExists(
        searchEngines.defaults.find(searchEngine => searchEngine.default));
    this.currentSearchEngine_ = defaultSearchEngine;
  }

  private updateCurrentSearchEngineFromCategorizedUrls_(
      categorizedTemplateUrls: CategorizedTemplateUrls) {
    const defaultSearchEngine =
        castExists(categorizedTemplateUrls.activeSiteShortcuts.find(
            searchEngine => searchEngine.default));
    this.currentSearchEngine_ = defaultSearchEngine;
  }

  override focus(): void {
    this.shadowRoot!.getElementById('browserSearchSettingsLink')!.focus();
  }

  private onSearchEngineLinkClick_(): void {
    this.browserProxy_.openBrowserSearchSettings();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsSearchEngineElement.is]: SettingsSearchEngineElement;
  }
}

customElements.define(
    SettingsSearchEngineElement.is, SettingsSearchEngineElement);
