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
import '/shared/settings/prefs/prefs.js';
import '../controls/settings_toggle_button.js';
import '../people_page/signout_dialog.js';
import 'chrome://resources/cr_elements/md_select.css.js';
// <if expr="not chromeos_ash">
import '../relaunch_confirmation_dialog.js';
// </if>
import '../settings_shared.css.js';
// <if expr="not chromeos_ash">
import '//resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';

// </if>

import type {CrLinkRowElement} from '//resources/cr_elements/cr_link_row/cr_link_row.js';
import type {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import {WebUiListenerMixin} from '//resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from '//resources/js/assert.js';
import {focusWithoutInk} from '//resources/js/focus_without_ink.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {ChromeSigninUserChoiceInfo, SyncBrowserProxy, SyncStatus} from '/shared/settings/people_page/sync_browser_proxy.js';
import {ChromeSigninUserChoice, SignedInState, StatusAction, SyncBrowserProxyImpl} from '/shared/settings/people_page/sync_browser_proxy.js';
import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import type {MetricsReporting, PrivacyPageBrowserProxy} from '/shared/settings/privacy_page/privacy_page_browser_proxy.js';
import {PrivacyPageBrowserProxyImpl} from '/shared/settings/privacy_page/privacy_page_browser_proxy.js';
import {HelpBubbleMixin} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import type {FocusConfig} from '../focus_config.js';
import {loadTimeData} from '../i18n_setup.js';
import type {PrivacyPageVisibility} from '../page_visibility.js';
import type {SettingsSignoutDialogElement} from '../people_page/signout_dialog.js';
import {RelaunchMixin, RestartType} from '../relaunch_mixin.js';
import {Router} from '../router.js';

import {getTemplate} from './personalization_options.html.js';

export interface SettingsPersonalizationOptionsElement {
  $: {
    toast: CrToastElement,
    signinAllowedToggle: SettingsToggleButtonElement,
    metricsReportingControl: SettingsToggleButtonElement,
    metricsReportingLink: CrLinkRowElement,
    urlCollectionToggle: SettingsToggleButtonElement,
    chromeSigninUserChoiceSelection: HTMLSelectElement,
  };
}

const SettingsPersonalizationOptionsElementBase = HelpBubbleMixin(
    RelaunchMixin(WebUiListenerMixin(I18nMixin(PrefsMixin(PolymerElement)))));

// browser_element_identifiers constants
const ANONYMIZED_URL_COLLECTION_ID =
    'kAnonymizedUrlCollectionPersonalizationSettingId';

export class SettingsPersonalizationOptionsElement extends
    SettingsPersonalizationOptionsElementBase {
  static get is() {
    return 'settings-personalization-options';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: {
        type: Object,
        notify: true,
      },

      focusConfig: {
        type: Object,
        observer: 'onFocusConfigChange_',
      },

      pageVisibility: Object,

      syncStatus: Object,

      // <if expr="_google_chrome and not chromeos_ash">
      // TODO(dbeam): make a virtual.* pref namespace and set/get this normally
      // (but handled differently in C++).
      metricsReportingPref_: {
        type: Object,
        value() {
          // TODO(dbeam): this is basically only to appease PrefControlMixin.
          // Maybe add a no-validate attribute instead? This makes little sense.
          return {};
        },
      },

      showRestart_: Boolean,
      // </if>

      showSignoutDialog_: Boolean,

      syncFirstSetupInProgress_: {
        type: Boolean,
        value: false,
        computed: 'computeSyncFirstSetupInProgress_(syncStatus)',
      },

      // <if expr="not is_chromeos">
      signinAvailable_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('signinAvailable'),
      },

      chromeSigninUserChoiceInfo_: {
        type: Object,
        value: null,
      },

      /** Expose ChromeSigninUserChoice enum to HTML bindings. */
      chromeSigninUserChoiceEnum_: {
        type: Object,
        value: ChromeSigninUserChoice,
      },
      // </if>

      enableAiSettingsPageRefresh_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('enableAiSettingsPageRefresh'),
      },

      enablePageContentSetting_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enablePageContentSetting');
        },
      },

      showHistorySearchControl_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('showHistorySearchControl');
        },
      },
    };
  }

  pageVisibility: PrivacyPageVisibility;
  focusConfig: FocusConfig;
  syncStatus: SyncStatus;

  // <if expr="_google_chrome and not chromeos_ash">
  private metricsReportingPref_: chrome.settingsPrivate.PrefObject<boolean>;
  private showRestart_: boolean;
  // </if>

  private showSignoutDialog_: boolean;
  private syncFirstSetupInProgress_: boolean;

  // <if expr="not is_chromeos">
  private signinAvailable_: boolean;

  private chromeSigninUserChoiceInfo_: ChromeSigninUserChoiceInfo;
  // </if>

  private enableAiSettingsPageRefresh_: boolean;
  private enablePageContentSetting_: boolean;
  private showHistorySearchControl_: boolean;

  private browserProxy_: PrivacyPageBrowserProxy =
      PrivacyPageBrowserProxyImpl.getInstance();

  private syncBrowserProxy_: SyncBrowserProxy =
      SyncBrowserProxyImpl.getInstance();

  private onFocusConfigChange_() {
    if (!this.enablePageContentSetting_) {
      // TODO(crbug.com/40070860): Remove once crbug.com/1476887 launched.
      return;
    }

    this.focusConfig.set(
        Router.getInstance().getRoutes().PAGE_CONTENT.path, () => {
          const toFocus =
              this.shadowRoot!.querySelector<HTMLElement>('#pageContentRow');
          assert(toFocus);
          focusWithoutInk(toFocus);
        });
  }

  private computeSyncFirstSetupInProgress_(): boolean {
    return !!this.syncStatus && !!this.syncStatus.firstSetupInProgress;
  }

  private showPriceEmailNotificationsToggle_(): boolean {
    // Only show the toggle when the user signed in.
    return loadTimeData.getBoolean('changePriceEmailNotificationsEnabled') &&
        !!this.syncStatus &&
        this.syncStatus.signedInState === SignedInState.SYNCING;
  }

  private getPriceEmailNotificationsPrefDesc_(): string {
    const username = this.syncStatus!.signedInUsername || '';
    return loadTimeData.getStringF('priceEmailNotificationsPrefDesc', username);
  }

  override ready() {
    super.ready();

    // <if expr="_google_chrome and not chromeos_ash">
    const setMetricsReportingPref = (metricsReporting: MetricsReporting) =>
        this.setMetricsReportingPref_(metricsReporting);
    this.addWebUiListener('metrics-reporting-change', setMetricsReportingPref);
    this.browserProxy_.getMetricsReporting().then(setMetricsReportingPref);
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
        this.$.urlCollectionToggle.getBubbleAnchor(), {anchorPaddingTop: 10});
  }

  // <if expr="chromeos_ash">
  /**
   * @return the autocomplete search suggestions CrToggleElement.
   */
  getSearchSuggestToggle(): SettingsToggleButtonElement|null {
    return this.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#searchSuggestToggle');
  }

  /**
   * @return the anonymized URL collection CrToggleElement.
   */
  getUrlCollectionToggle(): SettingsToggleButtonElement|null {
    return this.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#urlCollectionToggle');
  }

  /**
   * @return the Drive suggestions CrToggleElement.
   */
  getDriveSuggestToggle(): SettingsToggleButtonElement|null {
    return this.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#driveSuggestControl');
  }
  // </if>

  // <if expr="_google_chrome and not chromeos_ash">
  private onMetricsReportingChange_() {
    const enabled = this.$.metricsReportingControl.checked;
    this.browserProxy_.setMetricsReportingEnabled(enabled);
  }

  private setMetricsReportingPref_(metricsReporting: MetricsReporting) {
    const hadPreviousPref = this.metricsReportingPref_.value !== undefined;
    const pref: chrome.settingsPrivate.PrefObject<boolean> = {
      key: '',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: metricsReporting.enabled,
    };
    if (metricsReporting.managed) {
      pref.enforcement = chrome.settingsPrivate.Enforcement.ENFORCED;
      pref.controlledBy = chrome.settingsPrivate.ControlledBy.USER_POLICY;
    }

    // Ignore the next change because it will happen when we set the pref.
    this.metricsReportingPref_ = pref;

    // TODO(dbeam): remember whether metrics reporting was enabled when Chrome
    // started.
    if (metricsReporting.managed) {
      this.showRestart_ = false;
    } else if (hadPreviousPref) {
      this.showRestart_ = true;
    }
  }
  // </if>

  private showSearchSuggestToggle_(): boolean {
    if (this.pageVisibility === undefined) {
      // pageVisibility isn't defined in non-Guest profiles (crbug.com/1288911).
      return true;
    }
    return this.pageVisibility.searchPrediction;
  }

  private navigateTo_(url: string): void {
    window.location.href = url;
  }

  // <if expr="chromeos_ash">
  private onMetricsReportingLinkClick_() {
    // TODO(wesokuhara) Deep link directly to metrics toggle via settingId.
    this.navigateTo_(loadTimeData.getString('osSettingsPrivacyHubSubpageUrl'));
  }
  // </if>

  // <if expr="_google_chrome">
  private onUseSpellingServiceToggle_(event: Event) {
    // If turning on using the spelling service, automatically turn on
    // spellcheck so that the spelling service can run.
    if ((event.target as SettingsToggleButtonElement).checked) {
      this.setPrefValue('browser.enable_spellchecking', true);
    }
  }

  // <if expr="not chromeos_ash">
  private showSpellCheckControlToggle_(): boolean {
    return (
        !!(this.prefs as {spellcheck?: any}).spellcheck &&
        this.getPref<string[]>('spellcheck.dictionaries').value.length > 0);
  }
  // </if><!-- not chromeos -->

  // <if expr="chromeos_ash">
  private showSpellCheckControlLink_(): boolean {
    return (
        !!(this.prefs as {spellcheck?: any}).spellcheck &&
        this.getPref<string[]>('spellcheck.dictionaries').value.length > 0);
  }

  private onUseSpellingServiceLinkClick_() {
    this.navigateTo_(loadTimeData.getString('osSyncSetupSettingsUrl'));
  }
  // </if><!-- chromeos -->
  // </if><!-- _google_chrome -->

  private shouldShowDriveSuggest_(): boolean {
    if (loadTimeData.getBoolean('driveSuggestNoSetting')) {
      return false;
    }

    if (!loadTimeData.getBoolean('driveSuggestAvailable')) {
      return false;
    }

    if (loadTimeData.getBoolean('driveSuggestNoSyncRequirement')) {
      return true;
    }

    return !!this.syncStatus &&
        this.syncStatus.signedInState === SignedInState.SYNCING &&
        this.syncStatus.statusAction !== StatusAction.REAUTHENTICATE;
  }

  private onSigninAllowedChange_() {
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

  private onSignoutDialogClosed_() {
    if (this.shadowRoot!
            .querySelector<SettingsSignoutDialogElement>(
                'settings-signout-dialog')!.wasConfirmed()) {
      this.$.signinAllowedToggle.checked = false;
      this.$.signinAllowedToggle.sendPrefChange();
      this.$.toast.show();
    }
    this.showSignoutDialog_ = false;
  }

  private onRestartClick_(e: Event) {
    e.stopPropagation();
    this.performRestart(RestartType.RESTART);
  }

  private onPageContentRowClick_() {
    const router = Router.getInstance();
    router.navigateTo(router.getRoutes().PAGE_CONTENT);
  }

  private shouldShowHistorySearchControl_(): boolean {
    return this.showHistorySearchControl_ && !this.enableAiSettingsPageRefresh_;
  }

  private onHistorySearchRowClick_() {
    const router = Router.getInstance();
    router.navigateTo(router.getRoutes().HISTORY_SEARCH);
  }

  private computePageContentRowSublabel_() {
    return this.getPref('page_content_collection.enabled').value ?
        this.i18n('pageContentLinkRowSublabelOn') :
        this.i18n('pageContentLinkRowSublabelOff');
  }

  // <if expr="not is_chromeos">
  private setChromeSigninUserChoiceInfo_(info: ChromeSigninUserChoiceInfo) {
    this.chromeSigninUserChoiceInfo_ = info;
    if (info.choice !== ChromeSigninUserChoice.NO_CHOICE) {
      this.$.chromeSigninUserChoiceSelection.value = info.choice.toString();
    }
  }

  private onChromeSigninChoiceSelectionChanged_() {
    const selected = Number(this.$.chromeSigninUserChoiceSelection.value);
    assert(selected !== ChromeSigninUserChoice.NO_CHOICE);
    this.syncBrowserProxy_.setChromeSigninUserChoice(
        selected, this.chromeSigninUserChoiceInfo_.signedInEmail);
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
