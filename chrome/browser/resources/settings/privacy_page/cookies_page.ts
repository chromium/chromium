// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-cookies-page' is the settings page containing cookies
 * settings.
 */

import 'chrome://resources/cr_components/settings_prefs/prefs.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import '/shared/settings/controls/settings_toggle_button.js';
import '../icons.html.js';
import '../settings_shared.css.js';
import '../site_settings/site_list.js';
import './collapse_radio_button.js';
import './do_not_track_toggle.js';
import '/shared/settings/controls/settings_radio_group.js';

import {SettingsRadioGroupElement} from '/shared/settings/controls/settings_radio_group.js';
import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {FocusConfig} from '../focus_config.js';
import {loadTimeData} from '../i18n_setup.js';
import {MetricsBrowserProxy, MetricsBrowserProxyImpl, PrivacyElementInteractions} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import {Route, RouteObserverMixin, Router} from '../router.js';
import {ContentSetting, ContentSettingsTypes, CookieControlsMode} from '../site_settings/constants.js';
import {CookiePrimarySetting} from '../site_settings/site_settings_prefs_browser_proxy.js';

import {getTemplate} from './cookies_page.html.js';

/**
 * Must be kept in sync with the C++ enum of the same name (see
 * chrome/browser/prefetch/prefetch_prefs.h).
 */
export enum NetworkPredictionOptions {
  STANDARD = 0,
  WIFI_ONLY_DEPRECATED = 1,
  DISABLED = 2,
  EXTENDED = 3,
  DEFAULT = 1,
}

export interface SettingsCookiesPageElement {
  $: {
    toast: CrToastElement,
  };
}

const SettingsCookiesPageElementBase = RouteObserverMixin(
    WebUiListenerMixin(I18nMixin(PrefsMixin(PolymerElement))));

export class SettingsCookiesPageElement extends SettingsCookiesPageElementBase {
  static get is() {
    return 'settings-cookies-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },

      /**
       * Current search term.
       */
      searchTerm: {
        type: String,
        notify: true,
        value: '',
      },

      /**
       * Primary cookie control states for use in bindings.
       */
      cookiePrimarySettingEnum_: {
        type: Object,
        value: CookiePrimarySetting,
      },

      /** Cookie control modes for use in bindings. */
      cookieControlsModeEnum_: {
        type: Object,
        value: CookieControlsMode,
      },

      /**
       * Used for HTML bindings. This is defined as a property rather than
       * within the ready callback, because the value needs to be available
       * before local DOM initialization - otherwise, the toggle has unexpected
       * behavior.
       */
      networkPredictionUncheckedValue_: {
        type: Number,
        value: NetworkPredictionOptions.DISABLED,
      },

      contentSetting_: {
        type: Object,
        value: ContentSetting,
      },

      cookiesContentSettingType_: {
        type: String,
        value: ContentSettingsTypes.COOKIES,
      },

      exceptionListsReadOnly_: {
        type: Boolean,
        value: false,
      },

      blockAllPref_: {
        type: Object,
        value() {
          return {};
        },
      },

      focusConfig: {
        type: Object,
        observer: 'focusConfigChanged_',
      },

      enableFirstPartySetsUI_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('firstPartySetsUIEnabled'),
      },

      isPrivacySandboxSettings4_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('isPrivacySandboxSettings4'),
      },

      showPreloadingSubPage_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('showPreloadingSubPage'),
      },
    };
  }

  static get observers() {
    return [`onGeneratedPrefsUpdated_(prefs.generated.cookie_session_only,
        prefs.generated.cookie_primary_setting,
        prefs.generated.cookie_default_content_setting)`];
  }

  searchTerm: string;
  private cookiesContentSettingType_: ContentSettingsTypes;
  private exceptionListsReadOnly_: boolean;
  private blockAllPref_: chrome.settingsPrivate.PrefObject;
  focusConfig: FocusConfig;
  private enableFirstPartySetsUI_: boolean;
  private isPrivacySandboxSettings4_: boolean;
  private showPreloadingSubPage_: boolean;

  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  private focusConfigChanged_(_newConfig: FocusConfig, oldConfig: FocusConfig) {
    assert(!oldConfig);
    const selectSiteDataLinkRow = () => {
      const toFocus =
          this.shadowRoot!.querySelector<HTMLElement>('#site-data-trigger');
      assert(toFocus);
      focusWithoutInk(toFocus);
    };
    this.focusConfig.set(
        `${routes.SITE_SETTINGS_ALL.path}_${routes.COOKIES.path}`,
        selectSiteDataLinkRow);

    if (this.showPreloadingSubPage_) {
      const selectPreloadingLinkRow = () => {
        const toFocus =
            this.shadowRoot!.querySelector<HTMLElement>('#preloadingLinkRow');
        assert(toFocus);
        focusWithoutInk(toFocus);
      };
      this.focusConfig.set(
          `${routes.PRELOADING.path}_${routes.COOKIES.path}`,
          selectPreloadingLinkRow);
    }
  }

  override currentRouteChanged(route: Route) {
    if (route !== routes.COOKIES) {
      this.$.toast.hide();
    }
  }

  private getThirdPartyCookiesPageBlockThirdPartyIncognitoBulTwoLabel_():
      string {
    return this.i18n(
        this.enableFirstPartySetsUI_ ?
            'cookiePageBlockThirdIncognitoBulTwoFps' :
            'thirdPartyCookiesPageBlockIncognitoBulTwo');
  }

  private getCookiesPageBlockThirdPartyIncognitoBulTwoLabel_(): string {
    return this.i18n(
        this.enableFirstPartySetsUI_ ?
            'cookiePageBlockThirdIncognitoBulTwoFps' :
            'cookiePageBlockThirdIncognitoBulTwo');
  }

  // <if expr="not chromeos_ash">
  private getClearOnExitSubLabel_(): string {
    // <if expr="chromeos_lacros">
    if (loadTimeData.getBoolean('isSecondaryUser')) {
      return '';
    }
    // </if>

    return this.i18n('cookiePageClearOnExitDesc');
  }
  // </if>

  private onSiteDataClick_() {
    Router.getInstance().navigateTo(routes.SITE_SETTINGS_ALL);
  }

  private onGeneratedPrefsUpdated_() {
    if (this.isPrivacySandboxSettings4_) {
      // If the default cookie content setting is managed, the exception lists
      // should be disabled. `profile.cookie_controls_mode` doesn't control the
      // ability to create exceptions but the content setting does.
      const defaultContentSettingPref =
          this.getPref('generated.cookie_default_content_setting');
      this.exceptionListsReadOnly_ = defaultContentSettingPref.enforcement ===
          chrome.settingsPrivate.Enforcement.ENFORCED;
      return;
    }

    // TODO(crbug.com/1378703): Clean up after the feature is launched and these
    // generated preferences are deprecated. New page won't have 'session only'
    // controls.
    const sessionOnlyPref = this.getPref('generated.cookie_session_only');

    // If the clear on exit toggle is managed this implies a content setting
    // policy is present and the exception lists should be disabled.
    this.exceptionListsReadOnly_ = sessionOnlyPref.enforcement ===
        chrome.settingsPrivate.Enforcement.ENFORCED;

    // It is not currently possible to represent multiple management
    // sources for a single a preference. In all management scenarios,
    // the blockAll setting shares the same controlledBy as the
    // cookie_session_only pref. To support this, the controlledBy
    // fields for the |cookie_primary_setting| pref provided to the
    // blockAll control are overwritten with values from the session_only
    // preference.
    this.set(
        'blockAllPref_',
        Object.assign(this.getPref('generated.cookie_primary_setting'), {
          controlledBy: sessionOnlyPref.controlledBy,
          controlledByName: sessionOnlyPref.controlledByName,
        }));
  }

  private onCookieControlsModeChanged_() {
    // TODO(crbug.com/1378703): Use this.$.primarySettingGroup after the feature
    // is launched and element isn't in dom-if anymore.
    const primarySettingGroup: SettingsRadioGroupElement =
        this.shadowRoot!.querySelector('#primarySettingGroup')!;
    const selection = Number(primarySettingGroup.selected);
    if (selection === CookieControlsMode.OFF) {
      this.metricsBrowserProxy_.recordSettingsPageHistogram(
          PrivacyElementInteractions.THIRD_PARTY_COOKIES_ALLOW);
    } else if (selection === CookieControlsMode.INCOGNITO_ONLY) {
      this.metricsBrowserProxy_.recordSettingsPageHistogram(
          PrivacyElementInteractions.THIRD_PARTY_COOKIES_BLOCK_IN_INCOGNITO);
    } else {
      assert(selection === CookieControlsMode.BLOCK_THIRD_PARTY);
      this.metricsBrowserProxy_.recordSettingsPageHistogram(
          PrivacyElementInteractions.THIRD_PARTY_COOKIES_BLOCK);
    }

    // If this change resulted in the user now blocking 3P cookies where they
    // previously were not, and any of privacy sandbox APIs are enabled,
    // the privacy sandbox toast should be shown.
    const currentCookieControlsMode =
        this.getPref('profile.cookie_controls_mode').value;
    const areAnyPrivacySandboxApisEnabled =
        this.getPref('privacy_sandbox.m1.topics_enabled').value ||
        this.getPref('privacy_sandbox.m1.fledge_enabled').value ||
        this.getPref('privacy_sandbox.m1.fledge_enabled').value;
    const areThirdPartyCookiesAllowed =
        currentCookieControlsMode === CookieControlsMode.OFF ||
        currentCookieControlsMode === CookieControlsMode.INCOGNITO_ONLY;

    if (areAnyPrivacySandboxApisEnabled && areThirdPartyCookiesAllowed &&
        selection === CookieControlsMode.BLOCK_THIRD_PARTY) {
      if (!loadTimeData.getBoolean('isPrivacySandboxRestricted')) {
        this.$.toast.show();
      }
      this.metricsBrowserProxy_.recordAction(
          'Settings.PrivacySandbox.Block3PCookies');
    } else {
      this.$.toast.hide();
    }

    primarySettingGroup.sendPrefChange();
  }

  /**
   * Record interaction metrics for the primary cookie radio setting.
   */
  private onCookiePrimarySettingChanged_() {
    const primarySettingGroup: SettingsRadioGroupElement =
        this.shadowRoot!.querySelector('#primarySettingGroup')!;
    const selection = Number(primarySettingGroup.selected);
    if (selection === CookiePrimarySetting.ALLOW_ALL) {
      this.metricsBrowserProxy_.recordSettingsPageHistogram(
          PrivacyElementInteractions.COOKIES_ALL);
    } else if (selection === CookiePrimarySetting.BLOCK_THIRD_PARTY_INCOGNITO) {
      this.metricsBrowserProxy_.recordSettingsPageHistogram(
          PrivacyElementInteractions.COOKIES_INCOGNITO);
    } else if (selection === CookiePrimarySetting.BLOCK_THIRD_PARTY) {
      this.metricsBrowserProxy_.recordSettingsPageHistogram(
          PrivacyElementInteractions.COOKIES_THIRD);
    } else {  // CookiePrimarySetting.BLOCK_ALL
      this.metricsBrowserProxy_.recordSettingsPageHistogram(
          PrivacyElementInteractions.COOKIES_BLOCK);
    }

    // If this change resulted in the user now blocking 3P cookies where they
    // previously were not, and privacy sandbox APIs are enabled,
    // the privacy sandbox toast should be shown.
    const currentCookieSetting =
        this.getPref('generated.cookie_primary_setting').value;
    const privacySandboxEnabled =
        this.getPref('privacy_sandbox.apis_enabled_v2').value;

    if (privacySandboxEnabled &&
        (currentCookieSetting === CookiePrimarySetting.ALLOW_ALL ||
         currentCookieSetting ===
             CookiePrimarySetting.BLOCK_THIRD_PARTY_INCOGNITO) &&
        (selection === CookiePrimarySetting.BLOCK_THIRD_PARTY ||
         selection === CookiePrimarySetting.BLOCK_ALL)) {
      if (!loadTimeData.getBoolean('isPrivacySandboxRestricted')) {
        this.$.toast.show();
      }
      this.metricsBrowserProxy_.recordAction(
          'Settings.PrivacySandbox.Block3PCookies');
    } else if (
        selection === CookiePrimarySetting.ALLOW_ALL ||
        selection === CookiePrimarySetting.BLOCK_THIRD_PARTY_INCOGNITO) {
      this.$.toast.hide();
    }

    primarySettingGroup.sendPrefChange();
  }

  private onClearOnExitChange_() {
    this.metricsBrowserProxy_.recordSettingsPageHistogram(
        PrivacyElementInteractions.COOKIES_SESSION);
  }

  /**
   * Records changes made to the network prediction setting for logging, the
   * logic of actually changing the setting is taken care of by the
   * net.network_prediction_options pref.
   */
  private onNetworkPredictionChange_() {
    this.metricsBrowserProxy_.recordSettingsPageHistogram(
        PrivacyElementInteractions.NETWORK_PREDICTION);
  }

  private onPreloadingClick_() {
    this.metricsBrowserProxy_.recordSettingsPageHistogram(
        PrivacyElementInteractions.NETWORK_PREDICTION);
    Router.getInstance().navigateTo(routes.PRELOADING);
  }

  private getNetworkPredictionsOptionsLabel_(
      networkPredictionOption: NetworkPredictionOptions): string {
    if (networkPredictionOption === NetworkPredictionOptions.DISABLED) {
      return this.i18n('preloadingPageNoPreloadingTitle');
    }

    if (networkPredictionOption === NetworkPredictionOptions.EXTENDED) {
      return this.i18n('preloadingPageExtendedPreloadingTitle');
    }

    // NetworkPredictionOptions.WIFI_ONLY_DEPRECATED is treated the same as
    // NetworkPredictionOptions.STANDARD.
    // See chrome/browser/prefetch/prefetch_prefs.h.
    return this.i18n('preloadingPageStandardPreloadingTitle');
  }

  private onPrivacySandboxClick_() {
    this.metricsBrowserProxy_.recordAction(
        'Settings.PrivacySandbox.OpenedFromCookiesPageToast');
    this.$.toast.hide();
    // TODO(crbug.com/1378703): Open new privacy sandbox settings page.
    // TODO(crbug/1159942): Replace this with an ordinary OpenWindowProxy call.
    this.shadowRoot!.querySelector<HTMLAnchorElement>(
                        '#privacySandboxLink')!.click();
  }

  private firstPartySetsToggleDisabled_() {
    if (this.isPrivacySandboxSettings4_) {
      return this.getPref('profile.cookie_controls_mode').value !==
          CookieControlsMode.BLOCK_THIRD_PARTY;
    }

    return this.getPref('generated.cookie_primary_setting').value !==
        CookiePrimarySetting.BLOCK_THIRD_PARTY;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-cookies-page': SettingsCookiesPageElement;
  }
}

customElements.define(
    SettingsCookiesPageElement.is, SettingsCookiesPageElement);
