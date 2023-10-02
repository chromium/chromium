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
import '../../components/quick_start_entry_point.js';

import {assert} from '//resources/ash/common/assert.js';
import {NetworkList} from '//resources/ash/common/network/network_list_types.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {OobeDialogHostBehavior} from '../../components/behaviors/oobe_dialog_host_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {NetworkSelectLogin} from '../../components/network_select_login.js';


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 */
const NetworkScreenBase = mixinBehaviors(
    [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],
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
       * Whether Quick start feature is enabled. If it's enabled the quick start
       * button will be shown in the network screen.
       * @type {boolean}
       * @private
       */
      isQuickStartEnabled_: {
        type: Boolean,
        value: false,
      },
    };
  }

  static get observers() {
    return [];
  }

  get EXTERNAL_API() {
    return ['setError', 'setQuickStartEnabled'];
  }

  /** Called when dialog is shown. */
  onBeforeShow(data) {
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
    return this.i18n('networkSectionSubtitle');
  }

  /**
   * Sets the network error message.
   * @param {string} message Message to be shown.
   */
  setError(message) {
    this.errorMessage_ = message;
  }

  setQuickStartEnabled() {
    this.isQuickStartEnabled_ = true;
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
