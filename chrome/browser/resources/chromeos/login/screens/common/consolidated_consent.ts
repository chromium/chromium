// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview consolidated consent screen implementation.
 */

import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import '//resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import '//resources/js/action_link.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/buttons/oobe_back_button.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/buttons/oobe_text_button.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/oobe_icons.html.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/dialogs/oobe_loading_dialog.js';

import {assert} from '//resources/js/assert.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';

import {OobeModalDialog} from '../../components/dialogs/oobe_modal_dialog.js';
import {OobeUiState} from '../../components/display_manager_types.js';
import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';
import {ContentType, WebViewHelper} from '../../components/web_view_helper.js';
import {WebViewLoader} from '../../components/web_view_loader.js';
import {Oobe} from '../../cr_ui.js';

import {getTemplate} from './consolidated_consent.html.js';


// Enum that describes the current state of the consolidated consent screen
enum ConsolidatedConsentScreenState {
  LOADING = 'loading',
  LOADED = 'loaded',
  PLAY_LOAD_ERROR = 'play-load-error',
  GOOGLE_EULA = 'google-eula',
  CROS_EULA = 'cros-eula',
  ARC = 'arc',
  PRIVACY = 'privacy',
}

/**
 * URL to use when online page is not available.
 */
const GOOGLE_EULA_TERMS_URL = 'chrome://terms';
const PRIVACY_POLICY_URL = 'chrome://terms/arc/privacy_policy';

/**
 * Timeout to load online ToS.
 */
const CONSOLIDATED_CONSENT_ONLINE_LOAD_TIMEOUT_IN_MS = 10000;

/**
 * This enum is tied directly to a UMA enum defined in
 * //tools/metrics/histograms/enums.xml, and should always reflect it (do not
 * change one without changing the other).
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 */
enum ConsolidatedConsentUserAction {
  ACCEPT_BUTTON = 0,
  BACK_DEMO_BUTTON = 1,
  GOOGLE_EULA_LINK = 2,
  CROS_EULA_LINK = 3,
  ARC_TOS_LINK = 4,
  PRIVACY_POLICY_LINK = 5,
  USAGE_OPTIN_LEARN_MORE = 6,
  BACKUP_OPTIN_LEARN_MORE = 7,
  LOCATION_OPTIN_LEARN_MORE = 8,
  FOOTER_LEARN_MORE = 9,
  ERROR_STEP_RETRY_BUTTON = 10,
  MAX = 11,
}

const ConsolidatedConsentScreenElementBase =
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement)));

/**
 * Data that is passed to the screen during onBeforeShow.
 */
interface ConsolidatedConsentScreenData {
  isPrivacyHubLocationEnabled: boolean;
  isArcEnabled: boolean;
  isDemo: boolean;
  isChildAccount: boolean;
  isTosHidden: boolean;
  showRecoveryOption: boolean;
  recoveryOptionDefault: boolean;
  googleEulaUrl: string;
  crosEulaUrl: string;
  arcTosUrl: string;
  privacyPolicyUrl: string;
}

export class ConsolidatedConsent extends ConsolidatedConsentScreenElementBase {
  static get is() {
    return 'consolidated-consent-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      isPrivacyHubLocationEnabled: {
        type: Boolean,
        value: false,
      },

      isArcEnabled: {
        type: Boolean,
        value: true,
      },

      isDemo: {
        type: Boolean,
        value: false,
      },

      isChildAccount: {
        type: Boolean,
        value: false,
      },

      isTosHidden: {
        type: Boolean,
        value: false,
      },

      usageManaged: {
        type: Boolean,
        value: false,
      },

      usageOptinHidden: {
        type: Boolean,
        value: false,
      },

      usageOptinHiddenLoading: {
        type: Boolean,
        value: true,
      },

      backupManaged: {
        type: Boolean,
        value: false,
      },

      locationManaged: {
        type: Boolean,
        value: false,
      },

      usageChecked: {
        type: Boolean,
        value: true,
      },

      backupChecked: {
        type: Boolean,
        value: true,
      },

      locationChecked: {
        type: Boolean,
        value: true,
      },

      recoveryVisible: {
        type: Boolean,
        value: false,
      },

      recoveryChecked: {
        type: Boolean,
        value: false,
      },
    };
  }

  private isPrivacyHubLocationEnabled: boolean;
  private isArcEnabled: boolean;
  private isDemo: boolean;
  private isChildAccount: boolean;
  private isTosHidden: boolean;
  private usageManaged: boolean;
  private usageOptinHidden: boolean;
  private usageOptinHiddenLoading: boolean;
  private backupManaged: boolean;
  private locationManaged: boolean;
  private usageChecked: boolean;
  private backupChecked: boolean;
  private locationChecked: boolean;
  private recoveryVisible: boolean;
  private recoveryChecked: boolean;

  private areWebviewsInitialized: boolean;
  private configurationApplied: boolean;
  private arcTosContent: string;
  private googleEulaUrl: string;
  private crosEulaUrl: string;
  private arcTosUrl: string;
  private privacyPolicyUrl: string;

  constructor() {
    super();

    this.areWebviewsInitialized = false;

    // Text displayed in the Arc Terms of Service webview.
    this.arcTosContent = '';

    // Flag that ensures that OOBE configuration is applied only once.
    this.configurationApplied = false;

    // Online URLs
    this.googleEulaUrl = '';
    this.crosEulaUrl = '';
    this.arcTosUrl = '';
    this.privacyPolicyUrl = '';
  }

  override get EXTERNAL_API(): string[] {
    return [
      'setUsageMode',
      'setBackupMode',
      'setLocationMode',
      'setUsageOptinHidden',
    ];
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep(): ConsolidatedConsentScreenState {
    // The initial step of the screen is `LOADING` until `setUsageOptinHidden()`
    // method is called. `setUsageOptinHidden()` is called after ownership
    // status is retrieved asynchronously.
    return ConsolidatedConsentScreenState.LOADING;
  }

  override get UI_STEPS() {
    return ConsolidatedConsentScreenState;
  }

  /** Initial UI State for screen */
  // eslint-disable-next-line @typescript-eslint/naming-convention
  override getOobeUIInitialState(): OobeUiState {
    return OobeUiState.ONBOARDING;
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('ConsolidatedConsentScreen');
    this.updateLocalizedContent();
  }

  override onBeforeShow(data: ConsolidatedConsentScreenData): void {
    super.onBeforeShow(data);
    window.setTimeout(this.applyOobeConfiguration);

    this.isPrivacyHubLocationEnabled = data['isPrivacyHubLocationEnabled'];
    this.isArcEnabled = data['isArcEnabled'];
    this.isDemo = data['isDemo'];
    this.isChildAccount = data['isChildAccount'];
    this.isTosHidden = data['isTosHidden'];

    this.recoveryVisible = data['showRecoveryOption'];
    this.recoveryChecked = data['recoveryOptionDefault'];

    if (this.isDemo) {
      this.usageOptinHidden = false;
      this.usageOptinHiddenLoading = false;
    }

    this.googleEulaUrl = data['googleEulaUrl'];
    this.crosEulaUrl = data['crosEulaUrl'];
    this.arcTosUrl = data['arcTosUrl'];
    this.privacyPolicyUrl = data['privacyPolicyUrl'];

    // If the ToS section is hidden, apply the remove the top border of the
    // first opt-in.
    if (this.isTosHidden) {
      const useageStatsDiv =
          this.shadowRoot?.querySelector<HTMLDivElement>('#usageStats');
      if (useageStatsDiv instanceof HTMLDivElement) {
        useageStatsDiv.classList.add('first-optin-no-tos');
      }
    }

    const loadedContentDiv =
        this.shadowRoot?.querySelector<HTMLDivElement>('#loadedContent');
    if (loadedContentDiv instanceof HTMLDivElement) {
      if (this.isArcOptInsHidden(this.isArcEnabled, this.isDemo)) {
        loadedContentDiv.classList.remove('landscape-vertical-centered');
        loadedContentDiv.classList.add('landscape-header-aligned');
      } else {
        loadedContentDiv.classList.remove('landscape-header-aligned');
        loadedContentDiv.classList.add('landscape-vertical-centered');
      }
    }
    // Call updateLocalizedContent() to ensure that the listeners of the click
    // events on the ToS links are added.
    this.updateLocalizedContent();
  }

  private applyOobeConfiguration(): void {
    if (this.configurationApplied) {
      return;
    }

    const configuration = Oobe.getInstance().getOobeConfiguration();
    if (!configuration) {
      return;
    }

    if (configuration.eulaSendStatistics) {
      this.usageChecked = true;
    }

    if (configuration.eulaAutoAccept && configuration.arcTosAutoAccept) {
      this.onAcceptClick();
    }
  }

  // If ARC is disabled, don't show ARC ToS.
  private shouldShowArcTos(isTosHidden: boolean, isArcEnabled: boolean):
      boolean {
    return !isTosHidden && isArcEnabled;
  }

  private preventNewWindows(webview: chrome.webviewTag.WebView): void {
    webview.addEventListener('newwindow', (event: Event) => {
      event.preventDefault();
    });
  }

  private initializeTosWebivews(): void {
    if (this.areWebviewsInitialized) {
      return;
    }

    this.areWebviewsInitialized = true;

    const googleEulaWebview =
        this.shadowRoot?.querySelector<chrome.webviewTag.WebView>(
            '#consolidatedConsentGoogleEulaWebview');
    assert(googleEulaWebview);
    this.preventNewWindows(googleEulaWebview);

    const crosEulaWebview =
        this.shadowRoot?.querySelector<chrome.webviewTag.WebView>(
            '#consolidatedConsentCrosEulaWebview');
    assert(crosEulaWebview);
    this.preventNewWindows(crosEulaWebview);

    const arcTosWebview =
        this.shadowRoot?.querySelector<chrome.webviewTag.WebView>(
            '#consolidatedConsentArcTosWebview');
    assert(arcTosWebview);
    this.preventNewWindows(arcTosWebview);

    const privacyPolicyWebview =
        this.shadowRoot?.querySelector<chrome.webviewTag.WebView>(
            '#consolidatedConsentPrivacyPolicyWebview');
    assert(privacyPolicyWebview);
    this.preventNewWindows(privacyPolicyWebview);
  }

  private showGoogleEula(): void {
    this.initializeTosWebivews();
    this.setUIStep(ConsolidatedConsentScreenState.LOADING);

    const googleEulaWebview =
        this.shadowRoot?.querySelector<chrome.webviewTag.WebView>(
            '#consolidatedConsentGoogleEulaWebview');
    assert(googleEulaWebview);
    this.loadEulaWebview(
        googleEulaWebview, this.googleEulaUrl, false /* clearAnchors */);
  }

  private loadEulaWebview(
      webview: chrome.webviewTag.WebView, onlineTosUrl: string,
      clearAnchors: boolean): void {
    const loadFailureCallback = () => {
      WebViewHelper.loadUrlContentToWebView(
          webview, GOOGLE_EULA_TERMS_URL, ContentType.HTML);
    };

    const tosLoader = new WebViewLoader(
        webview, CONSOLIDATED_CONSENT_ONLINE_LOAD_TIMEOUT_IN_MS,
        loadFailureCallback, clearAnchors, true /* inject_css */);
    tosLoader.setUrl(onlineTosUrl);
  }

  private onGoogleEulaContentLoad(): void {
    this.setUIStep(ConsolidatedConsentScreenState.GOOGLE_EULA);
  }

  private showCrosEula(): void {
    this.initializeTosWebivews();
    this.setUIStep(ConsolidatedConsentScreenState.LOADING);

    const crosEulaWebview =
        this.shadowRoot?.querySelector<chrome.webviewTag.WebView>(
            '#consolidatedConsentCrosEulaWebview');
    assert(crosEulaWebview);
    this.loadEulaWebview(
        crosEulaWebview, this.crosEulaUrl, true /* clearAnchors */);
  }

  private onCrosEulaContentLoad(): void {
    this.setUIStep(ConsolidatedConsentScreenState.CROS_EULA);
  }

  private showArcTos(): void {
    this.initializeTosWebivews();
    this.setUIStep(ConsolidatedConsentScreenState.LOADING);
    this.loadArcTosWebview(this.arcTosUrl);
  }

  private loadArcTosWebview(onlineTosUrl: string): void {
    const loadFailureCallback = () => {
      this.setUIStep(ConsolidatedConsentScreenState.PLAY_LOAD_ERROR);
    };

    const arcTosWebview =
        this.shadowRoot?.querySelector<chrome.webviewTag.WebView>(
            '#consolidatedConsentArcTosWebview');
    assert(arcTosWebview);
    const tosLoader = new WebViewLoader(
        arcTosWebview, CONSOLIDATED_CONSENT_ONLINE_LOAD_TIMEOUT_IN_MS,
        loadFailureCallback, false /* clearAnchors */, false /* inject_css */);
    tosLoader.setUrl(onlineTosUrl);
  }

  private onArcTosContentLoad(): void {
    // In demo mode, consents are not recorded, so no need to store the ToS
    // Content.
    if (!this.isDemo) {
      const arcTosWebview =
          this.shadowRoot?.querySelector<chrome.webviewTag.WebView>(
              '#consolidatedConsentArcTosWebview');
      assert(arcTosWebview);

      arcTosWebview.executeScript(
          {code: 'document.body.innerHTML;'}, (results) => {
            if (results && results.length === 1 &&
                typeof results[0] === 'string') {
              this.arcTosContent = results[0];
            }
          });
    }

    this.setUIStep(ConsolidatedConsentScreenState.ARC);
  }

  private showPrivacyPolicy(): void {
    this.initializeTosWebivews();
    this.setUIStep(ConsolidatedConsentScreenState.LOADING);
    this.loadPrivacyPolicyWebview(this.privacyPolicyUrl);
  }

  private loadPrivacyPolicyWebview(onlineTosUrl: string): void {
    const privacyPolicyWebview =
        this.shadowRoot?.querySelector<chrome.webviewTag.WebView>(
            '#consolidatedConsentPrivacyPolicyWebview');
    assert(privacyPolicyWebview);

    const loadFailureCallback = () => {
      if (this.isDemo) {
        WebViewHelper.loadUrlContentToWebView(
            privacyPolicyWebview, PRIVACY_POLICY_URL, ContentType.PDF);
      }
    };

    const tosLoader = new WebViewLoader(
        privacyPolicyWebview, CONSOLIDATED_CONSENT_ONLINE_LOAD_TIMEOUT_IN_MS,
        loadFailureCallback, false /* clearAnchors */, false /* inject_css */);
    tosLoader.setUrl(onlineTosUrl);
  }

  private onPrivacyPolicyContentLoad(): void {
    this.setUIStep(ConsolidatedConsentScreenState.PRIVACY);
  }

  override updateLocalizedContent(): void {
    const privacyPolicyLink =
        this.shadowRoot?.querySelector<HTMLAnchorElement>('#privacyPolicyLink');
    assert(privacyPolicyLink);
    privacyPolicyLink.onclick = () => this.onPrivacyPolicyLinkClick();

    const googleEulaLink =
        this.shadowRoot?.querySelector<HTMLAnchorElement>('#googleEulaLink');
    assert(googleEulaLink);
    googleEulaLink.onclick = () => this.onGoogleEulaLinkClick();

    const googleEulaLinkArcDisabled =
        this.shadowRoot?.querySelector<HTMLAnchorElement>(
            '#googleEulaLinkArcDisabled');
    assert(googleEulaLinkArcDisabled);
    googleEulaLinkArcDisabled.onclick = () => this.onGoogleEulaLinkClick();

    const crosEulaLink =
        this.shadowRoot?.querySelector<HTMLAnchorElement>('#crosEulaLink');
    assert(crosEulaLink);
    crosEulaLink.onclick = () => this.onCrosEulaLinkClick();

    const crosEulaLinkArcDisabled =
        this.shadowRoot?.querySelector<HTMLAnchorElement>(
            '#crosEulaLinkArcDisabled');
    assert(crosEulaLinkArcDisabled);
    crosEulaLinkArcDisabled.onclick = () => this.onCrosEulaLinkClick();

    const arcTosLink =
        this.shadowRoot?.querySelector<HTMLAnchorElement>('#arcTosLink');
    assert(arcTosLink);
    arcTosLink.onclick = () => this.onArcTosLinkClick();
  }

  private getSubtitleArcEnabled(locale: string): TrustedHTML {
    const subtitle = document.createElement('div');
    subtitle.innerHTML = this.i18nAdvancedDynamic(
        locale, 'consolidatedConsentSubheader', {attrs: ['id']});

    const privacyPolicyLink = subtitle.querySelector('#privacyPolicyLink');
    assert(privacyPolicyLink);
    privacyPolicyLink.setAttribute('is', 'action-link');
    privacyPolicyLink.classList.add('oobe-local-link');
    return sanitizeInnerHtml(
        subtitle.innerHTML, {tags: ['a'], attrs: ['id', 'is', 'class']});
  }

  private getTermsDescriptionArcEnabled(locale: string): TrustedHTML {
    const description = document.createElement('div');
    description.innerHTML = this.i18nAdvancedDynamic(
        locale, 'consolidatedConsentTermsDescription', {attrs: ['id']});

    const googleEulaLink = description.querySelector('#googleEulaLink');
    assert(googleEulaLink);
    googleEulaLink.setAttribute('is', 'action-link');
    googleEulaLink.classList.add('oobe-local-link');

    const crosEulaLink = description.querySelector('#crosEulaLink');
    assert(crosEulaLink);
    crosEulaLink.setAttribute('is', 'action-link');
    crosEulaLink.classList.add('oobe-local-link');

    const arcTosLink = description.querySelector('#arcTosLink');
    assert(arcTosLink);
    arcTosLink.setAttribute('is', 'action-link');
    arcTosLink.classList.add('oobe-local-link');

    return sanitizeInnerHtml(
        description.innerHTML, {tags: ['a'], attrs: ['id', 'is', 'class']});
  }

  private getTermsDescriptionArcDisabled(locale: string): TrustedHTML {
    const description = document.createElement('div');
    description.innerHTML = this.i18nAdvancedDynamic(
        locale, 'consolidatedConsentTermsDescriptionArcDisabled',
        {attrs: ['id']});

    const googleEulaLink =
        description.querySelector('#googleEulaLinkArcDisabled');
    assert(googleEulaLink);
    googleEulaLink.setAttribute('is', 'action-link');
    googleEulaLink.classList.add('oobe-local-link');

    const crosEulaLink = description.querySelector('#crosEulaLinkArcDisabled');
    assert(crosEulaLink);
    crosEulaLink.setAttribute('is', 'action-link');
    crosEulaLink.classList.add('oobe-local-link');

    return sanitizeInnerHtml(
        description.innerHTML, {tags: ['a'], attrs: ['id', 'is', 'class']});
  }

  private getTitle(locale: string, isTosHidden: boolean,
      isChildAccount: boolean): TrustedHTML {
    if (isTosHidden) {
      return this.i18nAdvancedDynamic(
          locale, 'consolidatedConsentHeaderManaged');
    }

    if (isChildAccount) {
      return this.i18nAdvancedDynamic(locale, 'consolidatedConsentHeaderChild');
    }

    return this.i18nAdvancedDynamic(locale, 'consolidatedConsentHeader');
  }

  private getUsageLearnMoreText(locale: string, isChildAccount: boolean,
      isArcEnabled: boolean, isDemo: boolean): TrustedHTML {
    if (this.isArcOptInsHidden(isArcEnabled, isDemo)) {
      if (isChildAccount) {
        return this.i18nAdvancedDynamic(
            locale, 'consolidatedConsentUsageOptInLearnMoreArcDisabledChild');
      }
      return this.i18nAdvancedDynamic(
          locale, 'consolidatedConsentUsageOptInLearnMoreArcDisabled');
    }
    if (isChildAccount) {
      return this.i18nAdvancedDynamic(
          locale, 'consolidatedConsentUsageOptInLearnMoreChild');
    }
    return this.i18nAdvancedDynamic(
        locale, 'consolidatedConsentUsageOptInLearnMore');
  }

  private getBackupLearnMoreText(locale: string, isChildAccount: boolean):
      TrustedHTML {
    if (isChildAccount) {
      return this.i18nAdvancedDynamic(
          locale, 'consolidatedConsentBackupOptInLearnMoreChild');
    }
    return this.i18nAdvancedDynamic(
        locale, 'consolidatedConsentBackupOptInLearnMore');
  }

  private getLocationLearnMoreText(locale: string, isChildAccount: boolean):
      TrustedHTML {
    if (isChildAccount) {
      return this.i18nAdvancedDynamic(
          locale, 'consolidatedConsentLocationOptInLearnMoreChild');
    }
    return this.i18nAdvancedDynamic(
        locale, 'consolidatedConsentLocationOptInLearnMore');
  }

  private isArcOptInsHidden(isArcEnabled: boolean, isDemo: boolean): boolean {
    return !isArcEnabled || isDemo;
  }

  private isArcBackupOptInHidden(isArcEnabled: boolean, isDemo: boolean):
      boolean {
    return this.isArcOptInsHidden(isArcEnabled, isDemo);
  }

  private isLocationOptInHidden(isArcEnabled: boolean, isDemo: boolean):
      boolean {
    if (this.isPrivacyHubLocationEnabled) {
      // Skip ToS in Demo mode.
      if (isDemo) {
        return true;
      }
      return false;
    }

    return this.isArcOptInsHidden(isArcEnabled, isDemo);
  }

  private setUsageMode(enabled: boolean, managed: boolean): void {
    this.usageChecked = enabled;
    this.usageManaged = managed;
  }

  /**
   * Sets the hidden property of the usage opt-in.
   * @param hidden Defines the value used for the hidden property of
   *     the usage opt-in.
   */
  private setUsageOptinHidden(hidden: boolean): void {
    this.usageOptinHidden = hidden;
    this.usageOptinHiddenLoading = false;
    this.setUIStep(ConsolidatedConsentScreenState.LOADED);
  }

  /**
   * Sets current backup and restore mode.
   * @param enabled Defines the state of backup opt in.
   * @param managed Defines whether this setting is set by policy.
   */
  private setBackupMode(enabled: boolean, managed: boolean): void {
    this.backupChecked = enabled;
    this.backupManaged = managed;
  }

  /**
   * Sets current usage of location service opt in mode.
   * @param enabled Defines the state of location service opt in.
   * @param managed Defines whether this setting is set by policy.
   */
  private setLocationMode(enabled: boolean, managed: boolean): void {
    this.locationChecked = enabled;
    this.locationManaged = managed;
  }

  private onGoogleEulaLinkClick(): void {
    this.recordUmaHistogramForUserActions(
        ConsolidatedConsentUserAction.GOOGLE_EULA_LINK);
    this.showGoogleEula();
  }

  private onCrosEulaLinkClick(): void {
    this.recordUmaHistogramForUserActions(
        ConsolidatedConsentUserAction.CROS_EULA_LINK);
    this.showCrosEula();
  }

  private onArcTosLinkClick(): void {
    this.recordUmaHistogramForUserActions(
        ConsolidatedConsentUserAction.ARC_TOS_LINK);
    this.showArcTos();
  }

  private onPrivacyPolicyLinkClick(): void {
    this.recordUmaHistogramForUserActions(
        ConsolidatedConsentUserAction.PRIVACY_POLICY_LINK);
    this.showPrivacyPolicy();
  }

  private onTermsStepOkClick(): void {
    this.setUIStep(ConsolidatedConsentScreenState.LOADED);
  }

  private onErrorDoneClick(): void {
    this.setUIStep(ConsolidatedConsentScreenState.LOADED);
  }

  private onUsageLearnMoreClick(): void {
    this.recordUmaHistogramForUserActions(
        ConsolidatedConsentUserAction.USAGE_OPTIN_LEARN_MORE);
    const usageLearnMorePopUp =
        this.shadowRoot?.querySelector<OobeModalDialog>('#usageLearnMorePopUp');
    if (usageLearnMorePopUp instanceof OobeModalDialog) {
      usageLearnMorePopUp.showDialog();
    }
  }

  private onBackupLearnMoreClick(): void {
    this.recordUmaHistogramForUserActions(
        ConsolidatedConsentUserAction.BACKUP_OPTIN_LEARN_MORE);
    const backupLearnMorePopUp =
        this.shadowRoot?.querySelector<OobeModalDialog>(
            '#backupLearnMorePopUp');
    if (backupLearnMorePopUp instanceof OobeModalDialog) {
      backupLearnMorePopUp.showDialog();
    }
  }

  private onLocationLearnMoreClick(): void {
    this.recordUmaHistogramForUserActions(
        ConsolidatedConsentUserAction.LOCATION_OPTIN_LEARN_MORE);
    const locationLearnMorePopUp =
        this.shadowRoot?.querySelector<OobeModalDialog>(
            '#locationLearnMorePopUp');
    if (locationLearnMorePopUp instanceof OobeModalDialog) {
      locationLearnMorePopUp.showDialog();
    }
  }

  private onFooterLearnMoreClick(): void {
    this.recordUmaHistogramForUserActions(
        ConsolidatedConsentUserAction.FOOTER_LEARN_MORE);
    const footerLearnMorePopUp =
        this.shadowRoot?.querySelector<OobeModalDialog>(
            '#footerLearnMorePopUp');
    if (footerLearnMorePopUp instanceof OobeModalDialog) {
      footerLearnMorePopUp.showDialog();
    }
  }

  private onAcceptClick(): void {
    this.recordUmaHistogramForUserActions(
        ConsolidatedConsentUserAction.ACCEPT_BUTTON);

    this.userActed([
      'tos-accept',
      this.usageChecked,
      this.backupChecked,
      this.locationChecked,
      this.arcTosContent,
      this.recoveryChecked,
    ]);
  }

  /**
   * On-tap event handler for Retry button.
   */
  private onRetryClick(): void {
    this.recordUmaHistogramForUserActions(
        ConsolidatedConsentUserAction.ERROR_STEP_RETRY_BUTTON);
    this.showArcTos();
  }

  /**
   * On-tap event handler for Back button.
   */
  private onBack(): void {
    this.recordUmaHistogramForUserActions(
        ConsolidatedConsentUserAction.BACK_DEMO_BUTTON);
    this.userActed('back');
  }

  private recordUmaHistogramForUserActions(
      result: ConsolidatedConsentUserAction): void {
    chrome.send('metricsHandler:recordInHistogram', [
      'OOBE.ConsolidatedConsentScreen.UserActions',
      result,
      ConsolidatedConsentUserAction.MAX,
    ]);
  }
}


declare global {
  interface HTMLElementTagNameMap {
    [ConsolidatedConsent.is]: ConsolidatedConsent;
  }
}

customElements.define(ConsolidatedConsent.is, ConsolidatedConsent);
