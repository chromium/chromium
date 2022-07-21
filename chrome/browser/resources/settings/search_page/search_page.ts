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
import '../settings_shared.css.js';
import '../settings_vars.css.js';

import {addWebUIListener} from 'chrome://resources/js/cr.m.js';
import {I18nMixin} from 'chrome://resources/js/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin} from '../base_mixin.js';
import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Router} from '../router.js';
import {SearchEngine, SearchEnginesBrowserProxy, SearchEnginesBrowserProxyImpl, SearchEnginesInfo} from '../search_engines_page/search_engines_browser_proxy.js';

import {getTemplate} from './search_page.html.js';

const SettingsSearchPageElementBase = BaseMixin(I18nMixin(PolymerElement));

export class SettingsSearchPageElement extends SettingsSearchPageElementBase {
  static get is() {
    return 'settings-search-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: Object,

      /**
       * List of default search engines available.
       */
      searchEngines_: {
        type: Array,
        value() {
          return [];
        },
      },

      /** Filter applied to search engines. */
      searchEnginesFilter_: String,

      focusConfig_: Object,

      isActiveSearchEnginesFlagEnabled_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('isActiveSearchEnginesFlagEnabled'),
      },

      searchEnginesPageTitle_: {
        type: String,
        computed: 'computeSearchEnginesPageTitle_()',
      },
    };
  }

  prefs: Object;
  private searchEnginesPageTitle_: string;
  private isActiveSearchEnginesFlagEnabled_: boolean;
  private searchEngines_: SearchEngine[];
  private searchEnginesFilter_: string;
  private focusConfig_: Map<string, string>|null;
  private browserProxy_: SearchEnginesBrowserProxy =
      SearchEnginesBrowserProxyImpl.getInstance();

  override ready() {
    super.ready();

    // Omnibox search engine
    const updateSearchEngines = (searchEngines: SearchEnginesInfo) => {
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

  private onChange_() {
    const select = this.shadowRoot!.querySelector('select')!;
    const searchEngine = this.searchEngines_[select.selectedIndex];
    this.browserProxy_.setDefaultSearchEngine(searchEngine.modelIndex);
  }

  private onDisableExtension_() {
    this.dispatchEvent(new CustomEvent('refresh-pref', {
      bubbles: true,
      composed: true,
      detail: 'default_search_provider.enabled',
    }));
  }

  private onManageSearchEnginesTap_() {
    Router.getInstance().navigateTo(routes.SEARCH_ENGINES);
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

  private computeSearchEnginesPageTitle_(): string {
    return this.isActiveSearchEnginesFlagEnabled_ ?
        this.i18n('searchEnginesManageSiteSearch') :
        this.i18n('searchEnginesManage');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-search-page': SettingsSearchPageElement;
  }
}

customElements.define(SettingsSearchPageElement.is, SettingsSearchPageElement);
