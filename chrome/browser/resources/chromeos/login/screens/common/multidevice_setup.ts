// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview MultiDevice setup screen for login/OOBE.
 */
import '//resources/ash/common/multidevice_setup/mojo_api.js';
import '//resources/ash/common/multidevice_setup/multidevice_setup_shared.css.js';
import '//resources/ash/common/multidevice_setup/multidevice_setup.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/buttons/oobe_text_button.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/throbber_notice.js';

import {MultiDeviceSetup} from '//resources/ash/common/multidevice_setup/multidevice_setup.js';
import {MultiDeviceSetupDelegate} from '//resources/ash/common/multidevice_setup/multidevice_setup_delegate.js';
import {WebUIListenerBehavior} from '//resources/ash/common/web_ui_listener_behavior.js';
import {assert} from '//resources/js/assert.js';
import {PrivilegedHostDeviceSetter, PrivilegedHostDeviceSetterRemote} from '//resources/mojo/chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom-webui.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenMixin, LoginScreenMixinInterface} from '../../components/mixins/login_screen_mixin.js';
import {OobeI18nMixin, OobeI18nMixinInterface} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './multidevice_setup.html.js';

// OOBE screen that wraps MultiDevice setup flow when displayed during the
// user's onboarding on this Chromebook. Note that this flow is slightly
// different from the post-OOBE flow ( `c/b/r/chromeos/multidevice_setup/` )
// in 3 ways:
//  (1) During onboarding, the user has just entered their password, so we
//      do not prompt the user to enter a password before continuing.
//  (2) During onboarding, once the user selects a host device, we continue to
//      the next OOBE/login task; in the post-OOBE mode, there is a "success"
//      screen.
//  (3) During onboarding, buttons are styled with custom OOBE buttons.

export class MultiDeviceSetupScreenDelegate implements MultiDeviceSetupDelegate{

  private remote: PrivilegedHostDeviceSetterRemote|null;

  constructor() {
    this.remote = null;
  }

  isPasswordRequiredToSetHost(): boolean {
    return false;
  }

  setHostDevice(hostInstanceIdOrLegacyDeviceId: string,
      optAuthToken: string|undefined) {
    // An authentication token is not expected since a password is not
    // required.
    assert(!optAuthToken);

    if (!this.remote) {
      this.remote = PrivilegedHostDeviceSetter.getRemote();
    }

    return /** @type {!Promise<{success: boolean}>} */ (
        this.remote.setHostDevice(hostInstanceIdOrLegacyDeviceId));
  }

  shouldExitSetupFlowAfterSettingHost(): boolean {
    return true;
  }

  getStartSetupCancelButtonTextId(): string {
    return 'noThanks';
  }
}

const MultiDeviceSetupScreenBase =
    mixinBehaviors(
        [WebUIListenerBehavior],
        LoginScreenMixin(OobeI18nMixin(PolymerElement))) as {
      new (): PolymerElement & OobeI18nMixinInterface &
          LoginScreenMixinInterface & WebUIListenerBehavior,
    };

export class MultiDeviceSetupScreen extends MultiDeviceSetupScreenBase {
  static get is() {
    return 'multidevice-setup-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      delegate: Object,

      /**
       * ID of loadTimeData string to be shown on the forward navigation button.
       */
      forwardButtonTextId: {
        type: String,
      },

      /**
       * Whether the forward button should be disabled.
       */
      forwardButtonDisabled: {
        type: Boolean,
        value: false,
      },

      /**
       * ID of loadTimeData string to be shown on the cancel button.
       */
      cancelButtonTextId: {
        type: String,
      },

      /** Whether the webview overlay should be hidden. */
      webviewOverlayHidden: {
        type: Boolean,
        value: true,
      },

      /** Whether the webview is currently loading. */
      isWebviewLoading: {
        type: Boolean,
        value: false,
      },

      /**
       * URL for the webview to display.
       */
      webviewSrc: {
        type: String,
        value: '',
      },
    };
  }

  private delegate: MultiDeviceSetupDelegate;
  private forwardButtonTextId: string;
  private forwardButtonDisabled: boolean;
  private cancelButtonTextId: string;
  webviewOverlayHidden: boolean;
  isWebviewLoading: boolean;
  private webviewSrc: string;

  constructor() {
    super();
    this.delegate = new MultiDeviceSetupScreenDelegate();
  }

  override connectedCallback() {
    super.connectedCallback();
    const webview = this.shadowRoot?.
      querySelector('#multideviceHelpOverlayWebview');
    assert(!!webview);
    (webview as chrome.webviewTag.WebView).addEventListener(
      'contentload', () => {
        this.isWebviewLoading = false;
      });
  }

  override ready() {
    super.ready();
    this.initializeLoginScreen('MultiDeviceSetupScreen');
    this.updateLocalizedContent();
  }

  override updateLocalizedContent(): void {
    this.i18nUpdateLocale();
    const element = this.shadowRoot?.querySelector('#multideviceSetup');
    if(element instanceof MultiDeviceSetup) {
      element.updateLocalizedContent();
    }
  }

  onForwardButtonFocusRequested(): void {
    const nextButton = this.shadowRoot?.querySelector('#nextButton')!;
    if (nextButton instanceof HTMLElement) {
      nextButton.focus();
    }
  }

  private onExitRequested(
      event: CustomEvent<{didUserCompleteSetup: boolean}>): void {
    if (event.detail.didUserCompleteSetup) {
      chrome.send(
          'login.MultiDeviceSetupScreen.userActed', ['setup-accepted']);
    } else {
      chrome.send(
          'login.MultiDeviceSetupScreen.userActed', ['setup-declined']);
    }
  }

  private hideWebviewOverlay(): void {
    this.webviewOverlayHidden = true;
  }

  private onOpenLearnMoreWebviewRequested(event: CustomEvent<string>): void {
    this.isWebviewLoading = true;
    this.webviewSrc = event.detail;
    this.webviewOverlayHidden = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [MultiDeviceSetupScreen.is]: MultiDeviceSetupScreen;
  }
}

customElements.define(MultiDeviceSetupScreen.is, MultiDeviceSetupScreen);
