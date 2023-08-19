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
import 'chrome://resources/cr_components/settings_prefs/prefs.js';
import '/shared/settings/controls/settings_toggle_button.js';
import '../people_page/signout_dialog.js';
// <if expr="not chromeos_ash">
import '../relaunch_confirmation_dialog.js';
// </if>
import '../settings_shared.css.js';
// <if expr="not chromeos_ash">
import '//resources/cr_elements/cr_toast/cr_toast.js';

// </if>

import {CrLinkRowElement} from '//resources/cr_elements/cr_link_row/cr_link_row.js';
import {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import {WebUiListenerMixin} from '//resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {SettingsToggleButtonElement} from '/shared/settings/controls/settings_toggle_button.js';
import {StatusAction, SyncStatus} from '/shared/settings/people_page/sync_browser_proxy.js';
import {MetricsReporting, PrivacyPageBrowserProxy, PrivacyPageBrowserProxyImpl} from '/shared/settings/privacy_page/privacy_page_browser_proxy.js';
import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';

import {loadTimeData} from '../i18n_setup.js';
import {PrivacyPageVisibility} from '../page_visibility.js';
import {SettingsSignoutDialogElement} from '../people_page/signout_dialog.js';
import {RelaunchMixin, RestartType} from '../relaunch_mixin.js';

import {getTemplate} from './personalization_options.html.js';


export interface SettingsPersonalizationOptionsElement {
  $: {
    toast: CrToastElement,
    signinAllowedToggle: SettingsToggleButtonElement,
    metricsReportingControl: SettingsToggleButtonElement,
    metricsReportingLink: CrLinkRowElement,
  };
}

const SettingsPersonalizationOptionsElementBase =
    RelaunchMixin(WebUiListenerMixin(PrefsMixin(PolymerElement)));

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
      // </if>
    };
  }

  pageVisibility: PrivacyPageVisibility;
  syncStatus: SyncStatus;

  // <if expr="_google_chrome and not chromeos_ash">
  private metricsReportingPref_: chrome.settingsPrivate.PrefObject<boolean>;
  private showRestart_: boolean;
  // </if>

  private showSignoutDialog_: boolean;
  private syncFirstSetupInProgress_: boolean;

  // <if expr="not is_chromeos">
  private signinAvailable_: boolean;
  // </if>

  private browserProxy_: PrivacyPageBrowserProxy =
      PrivacyPageBrowserProxyImpl.getInstance();

  private computeSyncFirstSetupInProgress_(): boolean {
    return !!this.syncStatus && !!this.syncStatus.firstSetupInProgress;
  }

  private showPriceEmailNotificationsToggle_(): boolean {
    // Only show the toggle when the user signed in.
    return loadTimeData.getBoolean('changePriceEmailNotificationsEnabled') &&
        !!this.syncStatus && !!this.syncStatus.signedIn;
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
    if (loadTimeData.getBoolean('osDeprecateSyncMetricsToggle')) {
      this.navigateTo_(loadTimeData.getString('osPrivacySettingsUrl'));
    } else {
      this.navigateTo_(loadTimeData.getString('osSyncSetupSettingsUrl'));
    }
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

    return !!this.syncStatus && !!this.syncStatus.signedIn &&
        this.syncStatus.statusAction !== StatusAction.REAUTHENTICATE;
  }

  private onSigninAllowedChange_() {
    if (this.syncStatus.signedIn && !this.$.signinAllowedToggle.checked) {
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
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-personalization-options': SettingsPersonalizationOptionsElement;
  }
}

customElements.define(
    SettingsPersonalizationOptionsElement.is,
    SettingsPersonalizationOptionsElement);
