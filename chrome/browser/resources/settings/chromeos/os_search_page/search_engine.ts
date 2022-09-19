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
import '../../controls/extension_controlled_indicator.js';
import '../../controls/controlled_button.js';
import '../../controls/settings_toggle_button.js';
import '../../prefs/prefs.js';
import '../../prefs/pref_util.js';
import '../../settings_shared.css.js';
import '../../settings_vars.css.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUIListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink_js.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './search_engine.html.js';
import {SearchEngine, SearchEnginesBrowserProxy, SearchEnginesBrowserProxyImpl, SearchEnginesInfo} from './search_engines_browser_proxy.js';

const SettingsSearchEngineElementBase =
    I18nMixin(WebUIListenerMixin(PolymerElement));

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

      showSearchSelectionDialog_: Boolean,

      syncSettingsCategorizationEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('syncSettingsCategorizationEnabled');
        },
      },
    };
  }

  private browserProxy_: SearchEnginesBrowserProxy;
  private currentSearchEngine_: SearchEngine;
  private showSearchSelectionDialog_: boolean;
  private syncSettingsCategorizationEnabled_: boolean;

  constructor() {
    super();

    this.browserProxy_ = SearchEnginesBrowserProxyImpl.getInstance();
  }

  override ready() {
    super.ready();

    this.browserProxy_.getSearchEnginesList().then(
        this.updateCurrentSearchEngine_.bind(this));
    this.addWebUIListener(
        'search-engines-changed', this.updateCurrentSearchEngine_.bind(this));
  }

  private updateCurrentSearchEngine_(searchEngines: SearchEnginesInfo) {
    const defaultSearchEngine =
        searchEngines.defaults.find(searchEngine => searchEngine.default);
    assert(defaultSearchEngine);
    this.currentSearchEngine_ = defaultSearchEngine;
  }

  override focus() {
    if (loadTimeData.getBoolean('syncSettingsCategorizationEnabled')) {
      this.getBrowserSearchSettingsLink_().focus();
    } else {
      this.getSearchSelectionDialogButton_().focus();
    }
  }

  private onDisableExtension_() {
    const event = new CustomEvent('refresh-pref', {
      bubbles: true,
      composed: true,
      detail: 'default_search_provider.enabled',
    });
    this.dispatchEvent(event);
  }

  private onShowSearchSelectionDialogClick_() {
    this.showSearchSelectionDialog_ = true;
  }

  private onSearchSelectionDialogClose_() {
    this.showSearchSelectionDialog_ = false;
    focusWithoutInk(this.getSearchSelectionDialogButton_());
  }

  private onSearchEngineLinkClick_() {
    window.open('chrome://settings/search');
  }

  private isDefaultSearchControlledByPolicy_(
      pref: chrome.settingsPrivate.PrefObject): boolean {
    return pref.controlledBy ===
        chrome.settingsPrivate.ControlledBy.USER_POLICY;
  }

  private isDefaultSearchEngineEnforced_(
      pref: chrome.settingsPrivate.PrefObject): boolean {
    return pref.enforcement === chrome.settingsPrivate.Enforcement.ENFORCED;
  }

  private getBrowserSearchSettingsLink_() {
    const browserSearchSettingsLink =
        this.shadowRoot!.getElementById('browserSearchSettingsLink');
    assert(browserSearchSettingsLink);
    return browserSearchSettingsLink;
  }

  private getSearchSelectionDialogButton_() {
    const searchSelectionDialogButton =
        this.shadowRoot!.getElementById('searchSelectionDialogButton');
    assert(searchSelectionDialogButton);
    return searchSelectionDialogButton;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-search-engine': SettingsSearchEngineElement;
  }
}

customElements.define(
    SettingsSearchEngineElement.is, SettingsSearchEngineElement);
