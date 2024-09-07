// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying network selection OOBE dialog.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/ash/common/network/network_list.js';
import '../../components/buttons/oobe_back_button.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/dialogs/oobe_loading_dialog.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {NetworkList} from 'chrome://resources/ash/common/network/network_list_types.js';
import {NetworkSelectElement} from 'chrome://resources/ash/common/network/network_select.js';
import {assert} from 'chrome://resources/js/assert.js';

import {OobeAdaptiveDialog} from '../../components/dialogs/oobe_adaptive_dialog.js';
import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeDialogHostMixin} from '../../components/mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';
import {NetworkSelectLogin} from '../../components/network_select_login.js';

import {getTemplate} from './oobe_network.html.js';

export enum NetworkScreenStates {
  DEFAULT = 'default',
  // This state is only used for quick start flow, but might be extended to
  // the regular OOBE flow as well.
  QUICK_START_CONNECTING = 'quick-start-connecting',
}

const NetworkScreenBase = OobeDialogHostMixin(
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement))));

interface NetworkScreenData {
  ssid: string|undefined;
  useQuickStartSubtitle: boolean|undefined;
  useQuickStartWiFiErrorStrings: boolean | undefined;
}

/**
 * @polymer
 */
class NetworkScreen extends NetworkScreenBase {
  static get is() {
    return 'oobe-network-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * Network error message.
       */
      errorMessage: {
        type: String,
        value: '',
      },

      /**
       * Whether device is connected to the network.
       */
      isNetworkConnected: {
        type: Boolean,
        value: false,
      },

      /**
       * Controls if periodic background Wi-Fi scans are enabled to update the
       * list of available networks. It is enabled by default so that when user
       * gets to screen networks are already listed, but should be off when
       * user leaves the screen, as scanning can reduce effective bandwidth.
       */
      enableWifiScans: {
        type: Boolean,
        value: true,
      },

      /**
       * Whether Quick start feature is visible. If it's set the quick start
       * button will be shown in the network select login list as first item.
       */
      isQuickStartVisible: {
        type: Boolean,
        value: false,
      },

      // SSID (WiFi Network Name) used during the QuickStart step.
      ssid: {
        type: String,
        value: '',
      },

      // Whether the QuickStart subtitle should be shown while showing the
      // network list
      useQuickStartSubtitle: {
        type: Boolean,
        value: false,
      },

      // Whether to use a title and subtitle telling the user that there was
      // an error during QuickStart. This is only true when the QuickStart flow
      // is aborted while showing the network screen.
      useQuickStartWiFiErrorStrings: {
        type: Boolean,
        value: false,
      },

      // Whether the QuickStart 'Cancel' button is visible.
      quickStartCancelButtonVisible: {
        type: Boolean,
        value: true,
      },
    };
  }

  static get observers() {
    return [];
  }

  override get EXTERNAL_API() {
    return ['setError', 'setQuickStartEntryPointVisibility'];
  }

  private errorMessage: string;
  private isNetworkConnected: boolean;
  private ssid: string;
  private enableWifiScans: boolean;
  private isQuickStartVisible: boolean;
  private useQuickStartSubtitle: boolean;
  private useQuickStartWiFiErrorStrings: boolean;
  private quickStartCancelButtonVisible: boolean;

  constructor() {
    super();
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep() {
    return NetworkScreenStates.DEFAULT;
  }

  override get UI_STEPS() {
    return NetworkScreenStates;
  }

  private getNetworkSelectLogin(): NetworkSelectLogin {
    const networkSelectLogin =
        this.shadowRoot?.querySelector<NetworkSelectLogin>(
            '#networkSelectLogin');
    assert(networkSelectLogin instanceof NetworkSelectLogin);
    return networkSelectLogin;
  }

  /**
   * Called when dialog is shown.
   * @param data Screen init payload.
   */
  override onBeforeShow(data: NetworkScreenData): void {
    super.onBeforeShow(data);
    // Right now `ssid` is only set during quick start flow.
    if (data && 'ssid' in data && data['ssid']) {
      this.ssid = data['ssid'];
    } else {
      this.ssid = '';
    }
    if (this.ssid) {
      this.setUIStep(NetworkScreenStates.QUICK_START_CONNECTING);
      this.quickStartCancelButtonVisible = true;
      return;
    }

    this.useQuickStartSubtitle = data?.useQuickStartSubtitle ?? false;
    this.useQuickStartWiFiErrorStrings =
      data?.useQuickStartWiFiErrorStrings ?? false;

    this.setUIStep(NetworkScreenStates.DEFAULT);
    this.enableWifiScans = true;
    this.errorMessage = '';
    this.getNetworkSelectLogin().onBeforeShow();
    this.show();
  }

  /** Called when dialog is hidden. */
  override onBeforeHide() {
    super.onBeforeHide();
    this.getNetworkSelectLogin().onBeforeHide();
    this.enableWifiScans = false;
    this.isQuickStartVisible = false;
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('NetworkScreen');
    this.updateLocalizedContent();
  }

  private getNetworkDialog(): OobeAdaptiveDialog {
    const networkDialog =
        this.shadowRoot?.querySelector<OobeAdaptiveDialog>('#networkDialog');
    assert(networkDialog instanceof OobeAdaptiveDialog);
    return networkDialog;
  }

  /** Shows the dialog. */
  show() {
    this.getNetworkDialog().show();
  }

  override focus(): void {
    this.getNetworkDialog().focus();
  }

  /** Updates localized elements of the UI. */
  override updateLocalizedContent() {
    this.i18nUpdateLocale();
  }

  /**
   * Returns subtitle of the network dialog.
   */
  private getSubtitleMessage(
      locale: string, errorMessage: string,
    useQuickStartSubtitle: string,
    useQuickStartWiFiErrorStrings: string): string {
    if (errorMessage) {
      return errorMessage;
    }

    if (useQuickStartSubtitle) {
      return this.i18nDynamic(locale, 'quickStartNetworkNeededSubtitle');
    }

    if (useQuickStartWiFiErrorStrings) {
      return this.i18nDynamic(locale, 'networkScreenQuickStartWiFiErrorSubtitle');
    }

    return this.i18nDynamic(locale, 'networkSectionSubtitle');
  }

  /**
   * Sets the network error message.
   * @param message Message to be shown.
   */
  setError(message: string) {
    this.errorMessage = message;
    // Reset QuickStart WiFi error message
    this.useQuickStartWiFiErrorStrings = false;
  }

  setQuickStartEntryPointVisibility(visible: boolean): void {
    this.isQuickStartVisible = visible;
  }

  /**
   * Returns element of the network list with the given name.
   * Used to simplify testing.
   */
  getNetworkListItemByNameForTest(name: string): null
      |NetworkList.NetworkListItemType {
    const item =
        this.getNetworkSelectLogin()
            ?.shadowRoot?.querySelector<NetworkSelectElement>('#networkSelect')
            ?.getNetworkListItemByNameForTest(name);
    if (item !== undefined) {
      return item;
    }
    return null;
  }

  /**
   * Called after dialog is shown. Refreshes the list of the networks.
   */
  private onShown() {
    const networkSelectLogin = this.getNetworkSelectLogin();
    networkSelectLogin.refresh();

    setTimeout(() => {
      if (this.isNetworkConnected) {
        const nextButton =
            this.shadowRoot?.querySelector<HTMLElement>('#nextButton');
        assert(nextButton instanceof HTMLElement);
        nextButton.focus();
      } else {
        networkSelectLogin.focus();
      }
    }, 300);
    // Timeout is a workaround to correctly propagate focus to
    // RendererFrameHostImpl see https://crbug.com/955129 for details.
  }

  /**
   * Quick start button click handler.
   */
  private onQuickStartClicked() {
    this.userActed('activateQuickStart');
  }

  /**
   * Back button click handler.
   */
  private onBackClicked() {
    this.userActed('back');
  }

  /**
   * Cancels ongoing connection with the phone for QuickStart.
   */
  private onCancelClicked() {
    this.quickStartCancelButtonVisible = false;
    this.userActed('cancel');
  }

  /**
   * Called when the network setup is completed. Either by clicking on
   * already connected network in the list or by directly clicking on the
   * next button in the bottom of the screen.
   */
  private onContinue() {
    this.userActed('continue');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [NetworkScreen.is]: NetworkScreen;
  }
}

customElements.define(NetworkScreen.is, NetworkScreen);
