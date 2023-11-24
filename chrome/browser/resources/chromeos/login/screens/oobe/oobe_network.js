// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying network selection OOBE dialog.
 */

import '//resources/polymer/v3_0/paper-styles/color.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/ash/common/network/network_list.js';
import '../../components/buttons/oobe_back_button.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/dialogs/oobe_loading_dialog.js';

import {assert} from '//resources/ash/common/assert.js';
import {NetworkList} from '//resources/ash/common/network/network_list_types.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.js';
import {OobeDialogHostBehavior} from '../../components/behaviors/oobe_dialog_host_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {NetworkSelectLogin} from '../../components/network_select_login.js';


/**
 * UI mode for the screen.
 * @enum {string}
 */
export const NetworkScreenStates = {
  DEFAULT: 'default',
  // This state is only used for quick start flow, but might be extended to
  // the regular OOBE flow as well.
  QUICK_START_CONNECTING: 'quick-start-connecting',
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 */
const NetworkScreenBase = mixinBehaviors(
    [
      OobeI18nBehavior,
      OobeDialogHostBehavior,
      LoginScreenBehavior,
      MultiStepBehavior,
    ],
    PolymerElement);
/**
 * @typedef {{
 *   networkSelectLogin:  NetworkSelectLogin,
 *   networkDialog:  HTMLElement,
 *   nextButton:  HTMLElement,
 * }}
 */
NetworkScreenBase.$;

/**
 * Data that is passed to the screen during onBeforeShow.
 * @typedef {{
 *   ssid: (string|undefined),
 *   useQuickStartSubtitle: (boolean|undefined),
 * }}
 */
let NetworkScreenData;

/**
 * @polymer
 */
class NetworkScreen extends NetworkScreenBase {
  static get is() {
    return 'oobe-network-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Network error message.
       * @type {string}
       * @private
       */
      errorMessage_: {
        type: String,
        value: '',
      },

      /**
       * Whether device is connected to the network.
       * @type {boolean}
       * @private
       */
      isConnected_: {
        type: Boolean,
        value: false,
      },

      /**
       * Controls if periodic background Wi-Fi scans are enabled to update the
       * list of available networks. It is enabled by default so that when user
       * gets to screen networks are already listed, but should be off when user
       * leaves the screen, as scanning can reduce effective bandwidth.
       * @private
       */
      enableWifiScans_: {
        type: Boolean,
        value: true,
      },

      /**
       * Whether Quick start feature is visible. If it's set the quick start
       * button will be shown in the network select login list as first item.
       * @type {boolean}
       * @private
       */
      isQuickStartVisible_: {
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
      useQuickStartSubtitle_: {
        type: Boolean,
        value: false,
      },
    };
  }

  static get observers() {
    return [];
  }

  get EXTERNAL_API() {
    return ['setError', 'setQuickStartVisible'];
  }

  constructor() {
    super();
    this.UI_STEPS = NetworkScreenStates;
  }

  defaultUIStep() {
    return NetworkScreenStates.DEFAULT;
  }

  /**
   * Called when dialog is shown.
   * @param {NetworkScreenData} data Screen init payload.
   */
  onBeforeShow(data) {
    // Right now `ssid` is only set during quick start flow.
    this.ssid = data && 'ssid' in data && data['ssid'];
    if (this.ssid) {
      this.setUIStep(NetworkScreenStates.QUICK_START_CONNECTING);
      return;
    }

    this.useQuickStartSubtitle_ = data && 'useQuickStartSubtitle' in data &&
      data['useQuickStartSubtitle'];

    this.setUIStep(NetworkScreenStates.DEFAULT);
    this.enableWifiScans_ = true;
    this.errorMessage_ = '';
    this.$.networkSelectLogin.onBeforeShow();
    this.show();
  }

  /** Called when dialog is hidden. */
  onBeforeHide() {
    this.$.networkSelectLogin.onBeforeHide();
    this.enableWifiScans_ = false;
  }

  /** @override */
  ready() {
    super.ready();
    this.initializeLoginScreen('NetworkScreen');
    this.updateLocalizedContent();
  }

  /** Shows the dialog. */
  show() {
    this.$.networkDialog.show();
  }

  focus() {
    this.$.networkDialog.focus();
  }

  /** Updates localized elements of the UI. */
  updateLocalizedContent() {
    this.i18nUpdateLocale();
  }

  /**
   * Returns subtitle of the network dialog.
   * @param {string} locale
   * @param {string} errorMessage
   * @return {string}
   * @private
   */
  getSubtitleMessage_(locale, errorMessage) {
    if (errorMessage) {
      return errorMessage;
    }

    if (this.useQuickStartSubtitle_) {
      return this.i18n('quickStartNetworkNeededSubtitle');
    }

    return this.i18n('networkSectionSubtitle');
  }

  /**
   * Sets the network error message.
   * @param {string} message Message to be shown.
   */
  setError(message) {
    this.errorMessage_ = message;
  }

  setQuickStartVisible() {
    this.isQuickStartVisible_ = true;
  }

  /**
   * Returns element of the network list selected by the query.
   * Used to simplify testing.
   * @param {string} query
   * @return {NetworkList.NetworkListItemType}
   */
  getNetworkListItemWithQueryForTest(query) {
    const networkList =
        this.$.networkSelectLogin.shadowRoot.querySelector('#networkSelect')
            .getNetworkListForTest();
    assert(networkList);
    return networkList.querySelector(query);
  }

  /**
   * Returns element of the network list with the given name.
   * Used to simplify testing.
   * @param {string} name
   * @return {?NetworkList.NetworkListItemType}
   */
  getNetworkListItemByNameForTest(name) {
    return this.$.networkSelectLogin.shadowRoot.querySelector('#networkSelect')
        .getNetworkListItemByNameForTest(name);
  }

  /**
   * Called after dialog is shown. Refreshes the list of the networks.
   * @private
   */
  onShown_() {
    this.$.networkSelectLogin.refresh();
    setTimeout(() => {
      if (this.isConnected_) {
        this.$.nextButton.focus();
      } else {
        this.$.networkSelectLogin.focus();
      }
    }, 300);
    // Timeout is a workaround to correctly propagate focus to
    // RendererFrameHostImpl see https://crbug.com/955129 for details.
  }

  /**
   * Quick start button click handler.
   * @private
   */
  onQuickStartClicked_() {
    this.userActed('activateQuickStart');
  }

  /**
   * Back button click handler.
   * @private
   */
  onBackClicked_() {
    this.userActed('back');
  }

  /**
   * Cancels ongoing connection.
   * @private
   */
  onCancelClicked_() {
    this.userActed('cancel');
  }

  /**
   * Called when the network setup is completed. Either by clicking on
   * already connected network in the list or by directly clicking on the
   * next button in the bottom of the screen.
   * @private
   */
  onContinue_() {
    this.userActed('continue');
  }
}

customElements.define(NetworkScreen.is, NetworkScreen);
