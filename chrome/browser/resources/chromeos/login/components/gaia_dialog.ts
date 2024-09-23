// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element to handle Gaia authentication (Gaia webview,
 * action buttons, back button events, Gaia dialog beign shown, SAML UI).
 * Encapsulates authenticator.js and SAML notice handling.
 *
 * Events:
 *   identifierentered: Fired after user types their email.
 *   loadabort: Fired on the webview error.
 *   ready: Fired when the webview (not necessarily Gaia) is loaded first time.
 *   showview: Message from Gaia meaning Gaia UI is ready to be shown.
 *   startenrollment: User action to start enterprise enrollment.
 *   closesaml: User closes the dialog on the SAML page.
 *   backcancel: User presses back button when there is no history in Gaia page.
 */

import '//resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/ash/common/cr_elements/icons.html.js';
import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import './buttons/oobe_back_button.js';
import './buttons/oobe_text_button.js';
import './common_styles/oobe_common_styles.css.js';
import './common_styles/oobe_dialog_host_styles.css.js';
import './dialogs/oobe_content_dialog.js';
import './quick_start_entry_point.js';

import {Authenticator, AuthFlow} from '//oobe/gaia_auth_host/authenticator.js';
import {assert} from '//resources/js/assert.js';
import {sendWithPromise} from '//resources/js/cr.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {OobeTextButton} from './buttons/oobe_text_button.js';
import {getTemplate} from './gaia_dialog.html.js';
import {OobeDialogHostMixin} from './mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from './mixins/oobe_i18n_mixin.js';
import {OobeTypes} from './oobe_types.js';

const GaiaDialogBase = OobeDialogHostMixin(OobeI18nMixin(PolymerElement));

const CHROMEOS_GAIA_PASSWORD_METRIC = 'ChromeOS.Gaia.PasswordFlow';

export class GaiaDialog extends GaiaDialogBase {
  static get is() {
    return 'gaia-dialog' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * Whether SAML page uses camera.
       */
      videoEnabled: {
        type: Boolean,
        value: false,
        notify: true,
      },

      /**
       * Current auth flow. See AuthFlow
       */
      authFlow: {
        type: Number,
        value: 0,
        notify: true,
      },

      /**
       * Type of bottom buttons.
       */
      gaiaDialogButtonsType: {
        type: String,
        value: OobeTypes.GaiaDialogButtonsType.DEFAULT,
      },

      /**
       * Whether the dialog can be closed.
       */
      isClosable: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether SAML IdP page is shown
       */
      isSamlSsoVisible: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether default SAML IdP is shown.
       */
      isDefaultSsoProvider: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether to hide back button if form can't go back.
       */
      hideBackButtonIfCantGoBack: {
        type: Boolean,
        value: false,
      },

      /**
       * Used to display SAML notice.
       */
      authDomain: {
        type: String,
        value: '',
        notify: true,
      },

      /**
       * Controls navigation buttons enable state.
       */
      navigationEnabled: {
        type: Boolean,
        value: true,
        notify: true,
      },

      /**
       * Controls navigation buttons visibility.
       */
      navigationHidden: {
        type: Boolean,
        value: false,
      },

      /* Defines name of the webview. Useful for tests. To find Guestview for
       * the JSChecker.
       */
      webviewName: {
        type: String,
      },

      /**
       * Controls label on the primary action button.
       */
      primaryActionButtonLabel: {
        type: String,
        value: null,
      },

      /**
       * Controls availability of the primary action button.
       */
      primaryActionButtonEnabled: {
        type: Boolean,
        value: false,
      },

      /**
       * Controls label on the secondary action button.
       */
      secondaryActionButtonLabel: {
        type: String,
        value: null,
      },

      /**
       * Controls availability of the secondary action button.
       */
      secondaryActionButtonEnabled: {
        type: Boolean,
        value: false,
      },

      /**
       * True if Gaia indicates that it can go back (e.g. on the password page)
       */
      canGoBack: {
        type: Boolean,
        value: false,
        notify: true,
      },

      /**
       * Whether a pop-up overlay should be shown. This overlay is necessary
       * when GAIA shows an overlay within their iframe. It covers the parts
       * of the screen that would otherwise not show an overlay.
       */
      isPopUpOverlayVisible: {
        type: Boolean,
        computed: 'showOverlay(navigationEnabled, isSamlSsoVisible)',
      },

      samlBackButtonHidden: {
        type: Boolean,
        computed: 'isSamlBackButtonHidden(isDefaultSsoProvider, isClosable)',
      },

      /**
       * Whether Quick start feature is enabled. If it's enabled the quick start
       * button will be shown in the signin screen.
       */
      isQuickStartEnabled: Boolean,
    };
  }

  private videoEnabled: boolean;
  private authFlow: number;
  private gaiaDialogButtonsType: OobeTypes.GaiaDialogButtonsType;
  private isClosable: boolean;
  private isSamlSsoVisible: boolean;
  private isDefaultSsoProvider: boolean;
  private hideBackButtonIfCantGoBack: boolean;
  private authDomain: string;
  private navigationEnabled: boolean;
  private navigationHidden: boolean;
  private webviewName: string;
  private primaryActionButtonLabel: string;
  private primaryActionButtonEnabled: boolean;
  private secondaryActionButtonLabel: string;
  private secondaryActionButtonEnabled: boolean;
  private canGoBack: boolean;
  private isPopUpOverlayVisible: boolean;
  private samlBackButtonHidden: boolean;
  isQuickStartEnabled: boolean;
  private clickPrimaryActionButtonForTesting: boolean;
  private authenticator: Authenticator|undefined;

  constructor() {
    super();
    /**
     * Emulate click on the primary action button when it is visible and
     * enabled.
     */
    this.clickPrimaryActionButtonForTesting = false;

    this.authenticator = undefined;

    this.isQuickStartEnabled = false;
  }

  getAuthenticator(): Authenticator|undefined {
    return this.authenticator;
  }

  override ready(): void {
    super.ready();
    const webview = this.getFrame();
    this.authenticator = new Authenticator(webview);
    /**
     * Event listeners for the events triggered by the authenticator.
     */
    const authenticatorEventListeners: Record<string, (e: any) => void> = {
      // Note for the lowercase of fired events.
      'identifierEntered': (e: CustomEvent) => {
        this.dispatchEvent(new CustomEvent(
            'identifierentered',
            {bubbles: true, composed: true, detail: e.detail}));
      },
      'loadAbort': (e: CustomEvent) => {
        this.dispatchEvent(new CustomEvent(
            'webviewerror', {bubbles: true, composed: true, detail: e.detail}));
      },
      'ready': () => {
        this.dispatchEvent(
            new CustomEvent('ready', {bubbles: true, composed: true}));
      },
      'showView': () => {
        this.dispatchEvent(
            new CustomEvent('showview', {bubbles: true, composed: true}));
      },
      'menuItemClicked': (e: CustomEvent) => {
        if (e.detail === 'ee') {
          this.dispatchEvent(new CustomEvent(
              'startenrollment', {bubbles: true, composed: true}));
        }
      },
      'backButton': (e: CustomEvent) => {
        this.canGoBack = !!e.detail;
        this.getFrame().focus();
      },

      'setPrimaryActionEnabled': (e: CustomEvent) => {
        this.primaryActionButtonEnabled = e.detail;
        this.maybeClickPrimaryActionButtonForTesting();
      },
      'setPrimaryActionLabel': (e: CustomEvent) => {
        this.primaryActionButtonLabel = e.detail;
        this.maybeClickPrimaryActionButtonForTesting();
      },
      'setSecondaryActionEnabled': (e: CustomEvent) => {
        this.secondaryActionButtonEnabled = e.detail;
      },
      'setSecondaryActionLabel': (e: CustomEvent) => {
        this.secondaryActionButtonLabel = e.detail;
      },
      'setAllActionsEnabled': (e: CustomEvent) => {
        this.primaryActionButtonEnabled = e.detail;
        this.secondaryActionButtonEnabled = e.detail;
        this.maybeClickPrimaryActionButtonForTesting();
      },
      'videoEnabledChange': (e: CustomEvent) => {
        this.videoEnabled = e.detail.newValue;
      },
      'authFlowChange': (e: CustomEvent) => {
        this.authFlow = e.detail.newValue;
      },
      'authDomainChange': (e: CustomEvent) => {
        this.authDomain = e.detail.newValue;
      },
      'dialogShown': () => {
        this.navigationEnabled = false;
        chrome.send('enableShelfButtons', [false]);
      },
      'dialogHidden': () => {
        this.navigationEnabled = true;
        chrome.send('enableShelfButtons', [true]);
      },
      'exit': () => {
        this.dispatchEvent(
            new CustomEvent('exit', {bubbles: true, composed: true}));
      },
      'removeUserByEmail': (e: CustomEvent) => {
        this.dispatchEvent(new CustomEvent(
            'removeuserbyemail',
            {bubbles: true, composed: true, detail: e.detail}));
      },
      'apiPasswordAdded': () => {
        // Only record the metric for Gaia flow without 3rd-party SAML IdP.
        if (this.authFlow !== AuthFlow.DEFAULT) {
          return;
        }
        chrome.send(
            'metricsHandler:recordBooleanHistogram',
            [CHROMEOS_GAIA_PASSWORD_METRIC, false]);
        chrome.send('passwordEntered');
      },
      'authCompleted': (e: CustomEvent) => {
        // Only record the metric for Gaia flow without 3rd-party SAML IdP.
        if (this.authFlow === AuthFlow.DEFAULT) {
          chrome.send(
              'metricsHandler:recordBooleanHistogram',
              [CHROMEOS_GAIA_PASSWORD_METRIC, true]);
        }
        this.dispatchEvent(new CustomEvent(
            'authcompleted',
            {bubbles: true, composed: true, detail: e.detail}));
      },
    };

    for (const eventName in authenticatorEventListeners) {
      this.authenticator.addEventListener(
          eventName, authenticatorEventListeners[eventName].bind(this));
    }

    sendWithPromise('getIsSshConfigured')
        .then(this.updateSshWarningVisibility.bind(this));
  }

  private updateSshWarningVisibility(show: boolean): void {
    const sshWarning = this.shadowRoot?.querySelector('#sshWarning');
    if (sshWarning instanceof HTMLElement) {
      sshWarning.hidden = !show;
    }
  }

  show(): void {
    this.navigationEnabled = true;
    chrome.send('enableShelfButtons', [true]);
    this.getFrame().focus();
  }

  getFrame(): chrome.webviewTag.WebView {
    const frame = this.shadowRoot?.querySelector('#signin-frame');
    assert(!!frame);
    return frame as chrome.webviewTag.WebView;
  }

  clickPrimaryButtonForTesting(): void {
    this.clickPrimaryActionButtonForTesting = true;
    this.maybeClickPrimaryActionButtonForTesting();
  }

  maybeClickPrimaryActionButtonForTesting(): void {
    if (!this.clickPrimaryActionButtonForTesting) {
      return;
    }

    const button = this.shadowRoot!.querySelector<OobeTextButton>(
        '#primary-action-button')!;
    if (button.hidden || button.disabled) {
      return;
    }

    this.clickPrimaryActionButtonForTesting = false;
    button.click();
  }

  private getSamlNoticeMessage(
      locale: string, videoEnabled: boolean, authDomain: string): string {
    if (videoEnabled) {
      return this.i18nDynamic(locale, 'samlNoticeWithVideo', authDomain);
    }
    return this.i18nDynamic(locale, 'samlNotice', authDomain);
  }

  private close(): void {
    this.dispatchEvent(
        new CustomEvent('closesaml', {bubbles: true, composed: true}));
  }

  private onChangeSigninProviderClicked(): void {
    this.dispatchEvent(new CustomEvent(
        'changesigninprovider', {bubbles: true, composed: true}));
  }

  private onBackButtonClicked(): void {
    if (this.canGoBack) {
      this.getFrame().back();
      return;
    }
    this.dispatchEvent(
        new CustomEvent('backcancel', {bubbles: true, composed: true}));
  }

  /**
   * Handles clicks on Quick start button.
   */
  private onQuickStartClicked(): void {
    this.dispatchEvent(new CustomEvent(
        'quick-start-clicked', {bubbles: true, composed: true}));
  }

  /**
   * Handles clicks on "PrimaryAction" button.
   */
  private onPrimaryActionButtonClicked(): void {
    assert(this.authenticator);
    this.authenticator.sendMessageToWebview('primaryActionHit');
  }

  /**
   * Handles clicks on "SecondaryAction" button.
   */
  private onSecondaryActionButtonClicked(): void {
    assert(this.authenticator);
    this.authenticator.sendMessageToWebview('secondaryActionHit');
  }

  /**
   * Handles clicks on Kiosk enrollment button.
   */
  private onKioskButtonClicked(): void {
    this.setLicenseType(OobeTypes.LicenseType.KIOSK);
    this.onPrimaryActionButtonClicked();
  }

  /**
   * Handles clicks on Kiosk enrollment button.
   */
  private onEnterpriseButtonClicked(): void {
    this.setLicenseType(OobeTypes.LicenseType.ENTERPRISE);
    this.onPrimaryActionButtonClicked();
  }

  /**
   * @param licenseType - license to use.
   */
  private setLicenseType(licenseType: OobeTypes.LicenseType): void {
    this.dispatchEvent(new CustomEvent(
        'licensetypeselected',
        {bubbles: true, composed: true, detail: licenseType}));
  }

  /**
   * Whether the button is enabled.
   * @param navigationEnabled - whether navigation in general is
   * enabled.
   * @param buttonEnabled - whether a specific button is enabled.
   */
  private isButtonEnabled(navigationEnabled: boolean, buttonEnabled: boolean):
      boolean {
    return navigationEnabled && buttonEnabled;
  }

  /**
   * Whether the back button is hidden.
   * @param navigationHidden - whether navigation in general is hidden
   * @param hideBackButtonIfCantGoBack - whether it should be hidden.
   * @param canGoBack - whether the form can go back.
   */
  private isBackButtonHidden(
      navigationHidden: boolean, hideBackButtonIfCantGoBack: boolean,
      canGoBack: boolean): boolean {
    return navigationHidden || (hideBackButtonIfCantGoBack && !canGoBack);
  }

  /**
   * Whether the back button on SAML screen is hidden.
   * @param isDefaultSsoProvider - whether it is default SAML page.
   * @param isClosable - whether the form can be closed.
   */
  private isSamlBackButtonHidden(
      isDefaultSsoProvider: boolean, isClosable: boolean): boolean {
    return isDefaultSsoProvider && !isClosable;
  }

  /**
   * Whether popup overlay should be open.
   */
  private showOverlay(navigationEnabled: boolean, isSamlSsoVisible: boolean):
      boolean {
    return !navigationEnabled || isSamlSsoVisible;
  }

  /**
   * Whether default navigation (original, as gaia has) is shown.
   */
  private isDefaultNavigationShown(
      canGoBack: boolean,
      gaiaDialogButtonsType: OobeTypes.GaiaDialogButtonsType): boolean {
    return !canGoBack ||
        gaiaDialogButtonsType === OobeTypes.GaiaDialogButtonsType.DEFAULT;
  }

  /**
   * Whether Enterprise navigation is shown. Two buttons: primary for
   * Enterprise enrollment and secondary for Kiosk enrollment.
   */
  private isEnterpriseNavigationShown(
      canGoBack: boolean,
      gaiaDialogButtonsType: OobeTypes.GaiaDialogButtonsType): boolean {
    return canGoBack &&
        gaiaDialogButtonsType ===
        OobeTypes.GaiaDialogButtonsType.ENTERPRISE_PREFERRED;
  }

  /**
   * Whether Kiosk navigation is shown. Two buttons: primary for
   * Kiosk enrollment and secondary for Enterprise enrollment.
   */
  private isKioskNavigationShown(
      canGoBack: boolean,
      gaiaDialogButtonsType: OobeTypes.GaiaDialogButtonsType): boolean {
    return canGoBack &&
        gaiaDialogButtonsType ===
        OobeTypes.GaiaDialogButtonsType.KIOSK_PREFERRED;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [GaiaDialog.is]: GaiaDialog;
  }
}

customElements.define(GaiaDialog.is, GaiaDialog);
