// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-search-page' is the settings page containing search settings.
 */
import 'chrome://resources/cr_elements/policy/cr_policy_pref_indicator.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/md_select_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../controls/extension_controlled_indicator.js';
import '../i18n_setup.js';
import '../settings_page/settings_animated_pages.js';
import '../settings_page/settings_subpage.js';
import '../settings_shared_css.js';
import '../settings_vars_css.js';

import {addWebUIListener} from 'chrome://resources/js/cr.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin, BaseMixinInterface} from '../base_mixin.js';
import {routes} from '../route.js';
import {Router} from '../router.js';
import {SearchEngine, SearchEnginesBrowserProxy, SearchEnginesBrowserProxyImpl} from '../search_engines_page/search_engines_browser_proxy.js';


/**
 * @constructor
 * @extends {PolymerElement}
 * @appliesMixin {BaseMixin}
 * @implements {BaseMixinInterface}
 */
const SettingsSearchPageElementBase = BaseMixin(PolymerElement);

/** @polymer */
export class SettingsSearchPageElement extends SettingsSearchPageElementBase {
  static get is() {
    return 'settings-search-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      prefs: Object,

      /**
       * List of default search engines available.
       * @private {!Array<!SearchEngine>}
       */
      searchEngines_: {
        type: Array,
        value() {
          return [];
        }
      },

      /** @private Filter applied to search engines. */
      searchEnginesFilter_: String,

      /** @type {?Map<string, string>} */
      focusConfig_: Object,
    };
  }

  constructor() {
    super();

    /** @private {!SearchEnginesBrowserProxy} */
    this.browserProxy_ = SearchEnginesBrowserProxyImpl.getInstance();
  }

  /** @override */
  ready() {
    super.ready();

    // Omnibox search engine
    const updateSearchEngines = searchEngines => {
      this.set('searchEngines_', searchEngines.defaults);
    };
    this.browserProxy_.getSearchEnginesList().then(updateSearchEngines);
    addWebUIListener('search-engines-changed', updateSearchEngines);

    this.focusConfig_ = new Map();
    if (routes.SEARCH_ENGINES) {
      this.focusConfig_.set(
          routes.SEARCH_ENGINES.path, '#enginesSubpageTrigger');
    }
  }

  /** @private */
  onChange_() {
    const select = /** @type {!HTMLSelectElement} */ (
        this.shadowRoot.querySelector('select'));
    const searchEngine = this.searchEngines_[select.selectedIndex];
    this.browserProxy_.setDefaultSearchEngine(searchEngine.modelIndex);
  }

  /** @private */
  onDisableExtension_() {
    this.dispatchEvent(new CustomEvent('refresh-pref', {
      bubbles: true,
      composed: true,
      detail: 'default_search_provider.enabled'
    }));
  }

  /** @private */
  onManageSearchEnginesTap_() {
    Router.getInstance().navigateTo(routes.SEARCH_ENGINES);
  }

  /**
   * @param {!chrome.settingsPrivate.PrefObject} pref
   * @return {boolean}
   * @private
   */
  isDefaultSearchControlledByPolicy_(pref) {
    return pref.controlledBy ===
        chrome.settingsPrivate.ControlledBy.USER_POLICY;
  }

  /**
   * @param {!chrome.settingsPrivate.PrefObject} pref
   * @return {boolean}
   * @private
   */
  isDefaultSearchEngineEnforced_(pref) {
    return pref.enforcement === chrome.settingsPrivate.Enforcement.ENFORCED;
  }
}

customElements.define(SettingsSearchPageElement.is, SettingsSearchPageElement);
