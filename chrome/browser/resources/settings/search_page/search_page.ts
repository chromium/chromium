// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-search-page' is the settings page containing search settings.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import '/shared/settings/controls/cr_policy_pref_indicator.js';
import '/shared/settings/controls/extension_controlled_indicator.js';
import './extension_controlled_message.js';
import './search_engine_icon.js';
import './search_engine_list_dialog.js';
import '../settings_page/settings_section.js';

import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Router} from '../router.js';
import {SettingsViewMixinLit} from '../settings_page/settings_view_mixin_lit.js';

import type {CategorizedTemplateUrls, SearchEngine, SearchEnginesBrowserProxy, SearchEnginesInfo} from './search_engines_browser_proxy.js';
import {SearchEnginesBrowserProxyImpl, SearchEnginesInteractions} from './search_engines_browser_proxy.js';
import {getCss} from './search_page.css.js';
import {getHtml} from './search_page.html.js';

const SettingsSearchPageElementBase = PrefServiceObserverMixinLit(
    SettingsViewMixinLit(WebUiListenerMixinLit(I18nMixinLit(CrLitElement))));

export class SettingsSearchPageElement extends SettingsSearchPageElementBase {
  static get is() {
    return 'settings-search-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      defaultSearchProviderDataPref_: {type: Object},

      /**
       * List of search engines available in the search engine list dialog.
       */
      searchEngines_: {type: Array},

      // The selected default search engine.
      defaultSearchEngine_: {type: Object},

      // The title of the page and the default search engine card.
      searchPageTitle_: {type: String},

      // Boolean to check whether we need to show the dialog or not.
      showSearchEngineListDialog_: {type: Boolean},

      // The label of the confirmation toast that is displayed when the user
      // chooses a default search engine.
      confirmationToastLabel_: {type: String},

      // With this enabled, the shortcuts settings are present on this page
      // rather than the search engines subpage.
      searchSettingsUpdateEnabled_: {type: Boolean},
    };
  }

  protected accessor defaultSearchProviderDataPref_:
      chrome.settingsPrivate.PrefObject|undefined;
  protected accessor searchEngines_: SearchEngine[] = [];
  protected accessor showSearchEngineListDialog_: boolean = false;
  protected accessor defaultSearchEngine_: SearchEngine|null = null;
  protected accessor searchSettingsUpdateEnabled_: boolean =
      loadTimeData.getBoolean('searchSettingsUpdate');
  protected accessor searchPageTitle_: string = '';
  protected accessor confirmationToastLabel_: string = '';

  private browserProxy_: SearchEnginesBrowserProxy =
      SearchEnginesBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.mirrorPref(
        'default_search_provider_data.template_url_data',
        'defaultSearchProviderDataPref_');

    if (this.searchSettingsUpdateEnabled_) {
      // Only regional search engines and the default engine should be visible
      // in the search engine list dialog. No need to sort these since the
      // `activeSiteShortcuts` are already in the expected order (sorted
      // regional search engines first, then default engine if it is not in the
      // list).
      const updateSearchEngines =
          (categorizedTemplateUrls: CategorizedTemplateUrls) => {
            this.searchEngines_ =
                categorizedTemplateUrls.activeSiteShortcuts.filter(
                    engine => engine.isPrepopulated || engine.default);
          };
      this.browserProxy_.getCategorizedTemplateUrls().then(updateSearchEngines);
      this.addWebUiListener('search-engines-changed', updateSearchEngines);
      return;
    }

    const updateSearchEngines = (searchEngines: SearchEnginesInfo) => {
      this.searchEngines_ = searchEngines.defaults;
    };
    this.browserProxy_.getSearchEnginesList().then(updateSearchEngines);
    this.addWebUiListener('search-engines-changed', updateSearchEngines);
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('searchEngines_')) {
      this.defaultSearchEngine_ = this.computeDefaultSearchEngine_();
    }

    if (changedPrivateProperties.has('searchSettingsUpdateEnabled_')) {
      this.searchPageTitle_ = this.computeSearchPageTitle_();
    }
  }

  protected onDisableExtensionClick_() {
    this.fire('refresh-pref', 'default_search_provider.enabled');
  }

  protected onManageSearchEnginesClick_() {
    this.browserProxy_.recordSearchEnginesPageHistogram(
        SearchEnginesInteractions.SUBPAGE_NAVIGATED);
    Router.getInstance().navigateTo(routes.SEARCH_ENGINES);
  }

  protected isDefaultSearchControlledByPolicy_(): boolean {
    return !!this.defaultSearchProviderDataPref_ &&
        this.defaultSearchProviderDataPref_.controlledBy ===
        chrome.settingsPrivate.ControlledBy.USER_POLICY;
  }

  protected isDefaultSearchEngineEnforced_(): boolean {
    return !!this.defaultSearchProviderDataPref_ &&
        this.defaultSearchProviderDataPref_.enforcement ===
        chrome.settingsPrivate.Enforcement.ENFORCED;
  }

  private computeSearchPageTitle_(): string {
    return this.i18n(
        this.searchSettingsUpdateEnabled_ ? 'defaultSearch' :
                                            'searchPageTitle');
  }

  private computeDefaultSearchEngine_(): SearchEngine|null {
    if (!this.searchEngines_.length) {
      return null;
    }

    return this.searchEngines_.find(engine => engine.default) || null;
  }

  protected onOpenDialogButtonClick_() {
    this.showSearchEngineListDialog_ = true;
    chrome.metricsPrivate.recordUserAction('ChooseDefaultSearchEngine');
  }

  protected onSearchEngineChanged_(
      e: CustomEvent<{searchEngine: SearchEngine}>) {
    this.confirmationToastLabel_ = this.i18n(
        'searchEnginesConfirmationToastLabel', e.detail.searchEngine.name);
    const confirmationToast =
        this.shadowRoot.querySelector<CrToastElement>('#confirmationToast');
    assert(confirmationToast);
    confirmationToast.show();
  }

  protected onSearchEngineListDialogClose_() {
    this.showSearchEngineListDialog_ = false;
  }

  // SettingsViewMixin implementation.
  override getFocusConfig() {
    const map = new Map();

    if (!this.searchSettingsUpdateEnabled_) {
      map.set(routes.SEARCH_ENGINES.path, '#enginesSubpageTrigger');
    }
    return map;
  }

  // SettingsViewMixin implementation.
  override getAssociatedControlFor(childViewId: string): HTMLElement {
    assert(!this.searchSettingsUpdateEnabled_);
    assert(childViewId === 'searchEngines');
    const control =
        this.shadowRoot.querySelector<HTMLElement>('#enginesSubpageTrigger');
    assert(
        control,
        `Failed to find associated control for child '${childViewId}'`);
    return control;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-search-page': SettingsSearchPageElement;
  }
}

customElements.define(SettingsSearchPageElement.is, SettingsSearchPageElement);
