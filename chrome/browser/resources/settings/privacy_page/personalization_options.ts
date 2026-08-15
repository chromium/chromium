// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'personalization-options' contains several toggles related to
 * personalizations.
 */
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_toggle/cr_toggle.js';
import '../controls/settings_toggle_button.js';
import '../people_page/signout_dialog.js';
// <if expr="not is_chromeos">
import '../relaunch_confirmation_dialog.js';
// </if>

// <if expr="_google_chrome">
// <if expr="is_chromeos">
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
// </if>
// <if expr="not is_chromeos">
import '//resources/cr_elements/policy/cr_policy_indicator.js';
// </if>
// </if>

// <if expr="not is_chromeos">
import '//resources/cr_elements/cr_toast/cr_toast.js';

import type {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import type {CrToggleElement} from '//resources/cr_elements/cr_toggle/cr_toggle.js';
// </if>
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {ChromeSigninUserChoiceInfo, SyncBrowserProxy, SyncStatus} from '/shared/settings/people_page/sync_browser_proxy.js';
import {ChromeSigninUserChoice, SignedInState, StatusAction, SyncBrowserProxyImpl} from '/shared/settings/people_page/sync_browser_proxy.js';
import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import type {MetricsReporting, PrivacyPageBrowserProxy} from '/shared/settings/privacy_page/privacy_page_browser_proxy.js';
import {PrivacyPageBrowserProxyImpl} from '/shared/settings/privacy_page/privacy_page_browser_proxy.js';
import {HelpBubbleMixinLit} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin_lit.js';
// <if expr="is_chromeos">
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';

// </if>

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
import {pageVisibility} from '../page_visibility.js';
import type {PrivacyPageVisibility} from '../page_visibility.js';
import type {SettingsSignoutDialogElement} from '../people_page/signout_dialog.js';
import {RelaunchMixinLit, RestartType} from '../relaunch_mixin_lit.js';

import {getCss} from './personalization_options.css.js';
import {getHtml} from './personalization_options.html.js';

export interface SettingsPersonalizationOptionsElement {
  $: {
    urlCollectionToggle: SettingsToggleButtonElement,
    // <if expr="not is_chromeos">
    toast: CrToastElement,
    signinAllowedToggle: SettingsToggleButtonElement,
    chromeSigninUserChoiceSelection: HTMLSelectElement,
    chromeSigninUserChoiceToast: CrToastElement,
    // </if>
  };
}

const SettingsPersonalizationOptionsElementBase =
    HelpBubbleMixinLit(RelaunchMixinLit(WebUiListenerMixinLit(
        I18nMixinLit(PrefServiceObserverMixinLit(CrLitElement)))));

// browser_element_identifiers constants
const ANONYMIZED_URL_COLLECTION_ID =
    'kAnonymizedUrlCollectionPersonalizationSettingId';

export type PersonalizationOptionsElement =
    SettingsPersonalizationOptionsElement;

export class SettingsPersonalizationOptionsElement extends
    SettingsPersonalizationOptionsElementBase {
  static get is() {
    return 'settings-personalization-options';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      syncStatus: {type: Object},

      // <if expr="_google_chrome and not is_chromeos">
      metricsReporting_: {type: Object},

      showRestart_: {type: Boolean},
      // </if>

      showSearchAggregatorSuggest_: {type: Boolean},

      searchAggregatorSuggestFakePref_: {type: Object},

      showSignoutDialog_: {type: Boolean},

      shouldUseMetricsConsentRestructure_: {type: Boolean},

      syncFirstSetupInProgress_: {type: Boolean},

      // <if expr="not is_chromeos">
      signinAvailable_: {type: Boolean},

      chromeSigninUserChoiceInfo_: {type: Object},
      // </if>

      spellCheckDictionariesPref_: {type: Object},
    };
  }

  accessor syncStatus: SyncStatus = {statusAction: StatusAction.NO_ACTION};

  // <if expr="_google_chrome and not is_chromeos">
  protected accessor metricsReporting_: MetricsReporting|undefined;
  protected accessor showRestart_: boolean = false;
  // </if>

  protected accessor showSearchAggregatorSuggest_: boolean =
      loadTimeData.getBoolean('showSearchAggregatorSuggest');
  protected accessor searchAggregatorSuggestFakePref_:
      chrome.settingsPrivate.PrefObject<boolean> = {
    key: 'enterprise_search_aggregator_settings.fake_pref',
    type: chrome.settingsPrivate.PrefType.BOOLEAN,
    value: true,
    enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
    controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
  };

  protected accessor showSignoutDialog_: boolean = false;
  protected accessor syncFirstSetupInProgress_: boolean = false;
  protected accessor shouldUseMetricsConsentRestructure_: boolean =
      loadTimeData.getBoolean('shouldUseMetricsConsentRestructure');

  // <if expr="not is_chromeos">
  protected accessor signinAvailable_: boolean =
      loadTimeData.getBoolean('signinAvailable');

  protected accessor chromeSigninUserChoiceInfo_: ChromeSigninUserChoiceInfo|
      undefined;
  // </if>

  protected accessor spellCheckDictionariesPref_:
      chrome.settingsPrivate.PrefObject<string[]>|undefined;

  private browserProxy_: PrivacyPageBrowserProxy =
      PrivacyPageBrowserProxyImpl.getInstance();

  private syncBrowserProxy_: SyncBrowserProxy =
      SyncBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.mirrorPref('spellcheck.dictionaries', 'spellCheckDictionariesPref_');
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('syncStatus')) {
      this.syncFirstSetupInProgress_ = this.computeSyncFirstSetupInProgress_();
    }
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);

    // <if expr="_google_chrome and not is_chromeos">
    const setMetricsReporting = (metricsReporting: MetricsReporting) =>
        this.setMetricsReporting_(metricsReporting);
    this.addWebUiListener('metrics-reporting-change', setMetricsReporting);
    this.browserProxy_.getMetricsReporting().then(setMetricsReporting);
    // </if>

    // <if expr="not is_chromeos">
    this.syncBrowserProxy_.getChromeSigninUserChoiceInfo().then(
        this.setChromeSigninUserChoiceInfo_.bind(this));
    this.addWebUiListener(
        'chrome-signin-user-choice-info-change',
        this.setChromeSigninUserChoiceInfo_.bind(this));
    // </if>

    this.registerHelpBubble(
        ANONYMIZED_URL_COLLECTION_ID,
        this.$.urlCollectionToggle.getBubbleAnchor(), {paddingTop: 10});
  }

  private computeSyncFirstSetupInProgress_(): boolean {
    return !!this.syncStatus.firstSetupInProgress;
  }

  protected showPriceEmailNotificationsToggle_(): boolean {
    if (!loadTimeData.getBoolean('changePriceEmailNotificationsEnabled')) {
      return false;
    }
    // Only show the toggle when the user signed in.
    if (loadTimeData.getBoolean('replaceSyncPromosWithSignInPromos') &&
        this.syncStatus.signedInState === SignedInState.SIGNED_IN) {
      return true;
    }
    return this.syncStatus.signedInState === SignedInState.SYNCING;
  }

  protected getPriceEmailNotificationsPrefDesc_(): string {
    const username = this.syncStatus.signedInUsername || '';
    return loadTimeData.getStringF('priceEmailNotificationsPrefDesc', username);
  }

  // <if expr="is_chromeos">
  /**
   * @return the autocomplete search suggestions CrToggleElement.
   */
  getSearchSuggestToggle(): SettingsToggleButtonElement|null {
    return this.shadowRoot.querySelector<SettingsToggleButtonElement>(
        '#searchSuggestToggle');
  }

  /**
   * @return the anonymized URL collection CrToggleElement.
   */
  getUrlCollectionToggle(): SettingsToggleButtonElement|null {
    return this.shadowRoot.querySelector<SettingsToggleButtonElement>(
        '#urlCollectionToggle');
  }
  // </if>

  // <if expr="_google_chrome and not is_chromeos">
  protected onMetricsReportingChange_(e: Event) {
    const target = e.target as CrToggleElement;
    this.browserProxy_.setMetricsReportingEnabled(target.checked);
  }

  private setMetricsReporting_(metricsReporting: MetricsReporting) {
    const hadPrevious = this.metricsReporting_ !== undefined;
    this.metricsReporting_ = metricsReporting;

    // TODO(dbeam): remember whether metrics reporting was enabled when Chrome
    // started.
    if (metricsReporting.managed) {
      this.showRestart_ = false;
    } else if (hadPrevious) {
      this.showRestart_ = true;
    }
  }
  // </if>

  protected showSearchSuggestToggle_(): boolean {
    if (pageVisibility?.privacy === undefined) {
      // pageVisibility isn't defined in non-Guest profiles
      // (crbug.com/40211731).
      return true;
    }
    return (pageVisibility.privacy as PrivacyPageVisibility).searchPrediction;
  }

  // <if expr="is_chromeos">
  protected onMetricsReportingLinkClick_() {
    // TODO(wesokuhara) Deep link directly to metrics toggle via settingId.
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('osSettingsPrivacyHubSubpageUrl'));
  }
  // </if>

  // <if expr="_google_chrome">
  protected onUseSpellingServiceSettingsBooleanControlChange_(event: Event) {
    // If turning on using the spelling service, automatically turn on
    // spellcheck so that the spelling service can run.
    if ((event.target as SettingsToggleButtonElement).checked) {
      PrefService.getInstance().setPrefValue(
          'browser.enable_spellchecking', true);
    }
  }

  // <if expr="not is_chromeos">
  protected showSpellCheckControlToggle_(): boolean {
    return !!this.spellCheckDictionariesPref_ &&
        this.spellCheckDictionariesPref_.value.length > 0;
  }
  // </if><!-- not chromeos -->

  // <if expr="is_chromeos">
  protected showSpellCheckControlLink_(): boolean {
    return !!this.spellCheckDictionariesPref_ &&
        this.spellCheckDictionariesPref_.value.length > 0;
  }

  protected onUseSpellingServiceLinkClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('osSyncSetupSettingsUrl'));
  }
  // </if><!-- chromeos -->
  // </if><!-- _google_chrome -->

  protected onSignoutDialogClose_() {
    if (this.shadowRoot
            .querySelector<SettingsSignoutDialogElement>(
                'settings-signout-dialog')!.wasConfirmed()) {
      // <if expr="not is_chromeos">
      this.$.signinAllowedToggle.checked = false;
      this.$.signinAllowedToggle.sendPrefChange();
      this.$.toast.show();
      // </if>
    }
    this.showSignoutDialog_ = false;
  }

  // <if expr="not is_chromeos">
  protected onSigninAllowedSettingsBooleanControlChange_() {
    if (this.syncStatus.signedInState === SignedInState.SYNCING &&
        !this.$.signinAllowedToggle.checked) {
      // Switch the toggle back on and show the signout dialog.
      this.$.signinAllowedToggle.checked = true;
      this.showSignoutDialog_ = true;
    } else {
      this.$.signinAllowedToggle.sendPrefChange();
      this.$.toast.show();
    }
  }

  protected onRestartClick_(e: Event) {
    e.stopPropagation();
    this.performRestart(RestartType.RESTART);
  }

  private setChromeSigninUserChoiceInfo_(info: ChromeSigninUserChoiceInfo) {
    this.chromeSigninUserChoiceInfo_ = info;
    if (info.choice !== ChromeSigninUserChoice.NO_CHOICE) {
      this.$.chromeSigninUserChoiceSelection.value = info.choice.toString();
    }
  }

  protected onChromeSigninChoiceChange_() {
    const selected = Number(this.$.chromeSigninUserChoiceSelection.value);
    assert(selected !== ChromeSigninUserChoice.NO_CHOICE);

    this.$.chromeSigninUserChoiceToast.show();
    this.syncBrowserProxy_.setChromeSigninUserChoice(
        selected, this.chromeSigninUserChoiceInfo_?.signedInEmail || '');
  }
  // </if>
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-personalization-options': SettingsPersonalizationOptionsElement;
  }
}

customElements.define(
    SettingsPersonalizationOptionsElement.is,
    SettingsPersonalizationOptionsElement);
