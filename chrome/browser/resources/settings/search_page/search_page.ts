// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-search-page' is the settings page containing search settings.
 */
import '/shared/settings/controls/cr_policy_pref_indicator.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '/shared/settings/controls/extension_controlled_indicator.js';
import '../settings_page/settings_animated_pages.js';
import '../settings_page/settings_subpage.js';
import '../settings_shared.css.js';
import '../settings_vars.css.js';
import '../site_favicon.js';

import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin} from '../base_mixin.js';
import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Router} from '../router.js';
import type {SearchEngine, SearchEnginesBrowserProxy, SearchEnginesInfo} from '../search_engines_page/search_engines_browser_proxy.js';
import {SearchEnginesBrowserProxyImpl} from '../search_engines_page/search_engines_browser_proxy.js';

import {getTemplate} from './search_page.html.js';

const SettingsSearchPageElementBase =
    BaseMixin(WebUiListenerMixin(I18nMixin(PolymerElement)));

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
       * List of search engines available.
       */
      searchEngines_: Array,

      // Whether we need to set the icon size to large because they are loaded
      // in the binary or smaller because we get them from the favicon service.
      isEeaChoiceCountry_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isEeaChoiceCountry');
        },
      },

      // The selected default search engine.
      defaultSearchEngine_: {
        type: Object,
        computed: 'computeDefaultSearchEngine_(searchEngines_)',
      },

      /** Filter applied to search engines. */
      searchEnginesFilter_: String,

      focusConfig_: Object,

      // Boolean to check whether we need to show the dialog or not.
      showSearchEngineListDialog_: Boolean,

      // The label of the confirmation toast that is displayed when the user
      // chooses a default search engine.
      confirmationToastLabel_: String,
    };
  }

  prefs: Object;
  private searchEngines_: SearchEngine[];
  private searchEnginesFilter_: string;
  private showSearchEngineListDialog_: boolean;
  private defaultSearchEngine_: SearchEngine|null;
  private focusConfig_: Map<string, string>|null;
  private browserProxy_: SearchEnginesBrowserProxy =
      SearchEnginesBrowserProxyImpl.getInstance();
  private useLargeSearchEngineIcons_: boolean;
  private confirmationToastLabel_: string;

  override ready() {
    super.ready();

    // Omnibox search engine
    const updateSearchEngines = (searchEngines: SearchEnginesInfo) => {
      this.searchEngines_ = searchEngines.defaults;
    };
    this.browserProxy_.getSearchEnginesList().then(updateSearchEngines);
    this.addWebUiListener('search-engines-changed', updateSearchEngines);

    this.focusConfig_ = new Map();
    if (routes.SEARCH_ENGINES) {
      this.focusConfig_.set(
          routes.SEARCH_ENGINES.path, '#enginesSubpageTrigger');
    }
  }

  override connectedCallback() {
    super.connectedCallback();
    this.setFaviconSize_();
  }

  private onDisableExtension_() {
    this.dispatchEvent(new CustomEvent('refresh-pref', {
      bubbles: true,
      composed: true,
      detail: 'default_search_provider.enabled',
    }));
  }

  private onManageSearchEnginesClick_() {
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

  private computeDefaultSearchEngine_() {
    if (!this.searchEngines_.length) {
      return null;
    }

    return this.searchEngines_.find(engine => engine.default)!;
  }

  private onOpenDialogButtonClick_() {
    this.showSearchEngineListDialog_ = true;
    chrome.metricsPrivate.recordUserAction('ChooseDefaultSearchEngine');
  }

  private onDefaultSearchEngineChangedInDialog_(e: CustomEvent) {
    this.confirmationToastLabel_ = this.i18n(
        'searchEnginesConfirmationToastLabel', e.detail.searchEngine.name);
    this.shadowRoot!.querySelector<CrToastElement>(
                        '#confirmationToast')!.show();
  }

  private onSearchEngineListDialogClose_() {
    this.showSearchEngineListDialog_ = false;
  }

  private setFaviconSize_() {
    this.style.setProperty(
        '--favicon-size', this.useLargeSearchEngineIcons_ ? '24px' : '16px');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-search-page': SettingsSearchPageElement;
  }
}

customElements.define(SettingsSearchPageElement.is, SettingsSearchPageElement);
