// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-search-page' is the settings page containing search settings.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import '/shared/settings/controls/cr_policy_pref_indicator.js';
import '/shared/settings/controls/extension_controlled_indicator.js';
import './search_engine_list_dialog.js';
import '../settings_page/settings_section.js';
import '../settings_shared.css.js';
import '../settings_vars.css.js';
import '../site_favicon.js';

import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Router} from '../router.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

import type {SearchEngine, SearchEnginesBrowserProxy, SearchEnginesInfo} from './search_engines_browser_proxy.js';
import {SearchEnginesBrowserProxyImpl} from './search_engines_browser_proxy.js';
import {getTemplate} from './search_page.html.js';

const SettingsSearchPageElementBase =
    SettingsViewMixin(WebUiListenerMixin(I18nMixin(PolymerElement)));

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

      // The selected default search engine.
      defaultSearchEngine_: {
        type: Object,
        computed: 'computeDefaultSearchEngine_(searchEngines_)',
      },

      // Boolean to check whether we need to show the dialog or not.
      showSearchEngineListDialog_: Boolean,

      // The label of the confirmation toast that is displayed when the user
      // chooses a default search engine.
      confirmationToastLabel_: String,
    };
  }

  declare prefs: Object;
  declare private searchEngines_: SearchEngine[];
  declare private showSearchEngineListDialog_: boolean;
  declare private defaultSearchEngine_: SearchEngine|null;
  private browserProxy_: SearchEnginesBrowserProxy =
      SearchEnginesBrowserProxyImpl.getInstance();

  // Whether we need to set the icon size to large because they are loaded
  // in the binary or smaller because we get them from the favicon service.
  private isEeaChoiceCountry_: boolean =
      loadTimeData.getBoolean('isEeaChoiceCountry');

  declare private confirmationToastLabel_: string;

  override ready() {
    super.ready();

    // Omnibox search engine
    const updateSearchEngines = (searchEngines: SearchEnginesInfo) => {
      this.searchEngines_ = searchEngines.defaults;
    };
    this.browserProxy_.getSearchEnginesList().then(updateSearchEngines);
    this.addWebUiListener('search-engines-changed', updateSearchEngines);
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
        '--favicon-size', this.isEeaChoiceCountry_ ? '24px' : '16px');
  }

  // SettingsViewMixin implementation.
  override getFocusConfig() {
    return new Map([
      [routes.SEARCH_ENGINES.path, '#enginesSubpageTrigger'],
    ]);
  }

  // SettingsViewMixin implementation.
  override getAssociatedControlFor(childViewId: string): HTMLElement {
    assert(childViewId === 'searchEngines');
    const control =
        this.shadowRoot!.querySelector<HTMLElement>('#enginesSubpageTrigger');
    assert(control);
    return control;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-search-page': SettingsSearchPageElement;
  }
}

customElements.define(SettingsSearchPageElement.is, SettingsSearchPageElement);
