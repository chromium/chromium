// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for network password input fields.
 */

import '//resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/ash/common/cr_elements/cr_icons.css.js';
import '//resources/ash/common/cr_elements/cr_input/cr_input.js';
import '//resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '//resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
import './cr_policy_network_indicator_mojo.js';
import './network_shared.css.js';

import {I18nBehavior} from '//resources/ash/common/i18n_behavior.js';
import {Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {NetworkType, OncSource} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';

import {CrPolicyNetworkBehaviorMojo} from './cr_policy_network_behavior_mojo.js';
import {NetworkConfigElementBehavior} from './network_config_element_behavior.js';
import {getTemplate} from './network_password_input.html.js';
import {FAKE_CREDENTIAL} from './onc_mojo.js';

Polymer({
  _template: getTemplate(),
  is: 'network-password-input',

  behaviors: [
    I18nBehavior,
    CrPolicyNetworkBehaviorMojo,
    NetworkConfigElementBehavior,
  ],

  properties: {
    label: {
      type: String,
      reflectToAttribute: true,
    },

    ariaLabel: {
      type: String,
    },

    showPassword: {
      type: Boolean,
      value: false,
    },

    invalid: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether an errorMessage can be shown beneath the input.
     */
    allowErrorMessage: {
      type: Boolean,
      value: false,
    },

    /**
     * Error message shown beneath input (only shown if allowErrorMessage is
     * true).
     */
    errorMessage: {
      type: String,
      value: '',
    },

    /** {?ManagedProperties} */
    managedProperties: {
      type: Object,
      value: null,
    },

    /** @private */
    tooltipPosition_: {
      type: String,
      value: '',
    },

    /** @private */
    showPolicyIndicator_: {
      type: Boolean,
      value: false,
      computed: 'getDisabled_(disabled, property)',
    },
  },

  /** @override */
  attached() {
    this.tooltipPosition_ =
        window.getComputedStyle(this).direction === 'rtl' ? 'right' : 'left';
  },

  /** @private */
  focus() {
    this.$$('cr-input').focus();

    // If the input has any contents, the should be selected when focus is
    // applied.
    this.$$('cr-input').select();
  },

  /**
   * @return {string}
   * @private
   */
  getInputType_() {
    return this.showPassword ? 'text' : 'password';
  },

  /**
   * @return {boolean}
   * @private
   */
  isShowingPlaceholder_() {
    return this.value === FAKE_CREDENTIAL;
  },

  /**
   * @return {string}
   * @private
   */
  getIconClass_() {
    return this.showPassword ? 'icon-visibility-off' : 'icon-visibility';
  },

  /**
   * @return {string}
   * @private
   */
  getShowPasswordTitle_() {
    return this.showPassword ? this.i18n('hidePassword') :
                               this.i18n('showPassword');
  },

  /**
   * TODO(b/328633844): Update this function to make the "show password" button
   * visible for configured WiFi networks.
   * Used to control whether the Show Password button is visible.
   * @return {boolean}
   * @private
   */
  showPasswordIcon_() {
    return !this.showPolicyIndicator_ &&
        (!this.managedProperties ||
         this.managedProperties.source === OncSource.kNone);
  },

  /**
   * @param {!Event} event
   * @private
   */
  onShowPasswordTap_(event) {
    if (event.type === 'touchend' && event.cancelable) {
      // Prevent touch from producing secondary mouse events
      // that may cause the tooltip to appear unnecessarily.
      event.preventDefault();
    }

    if (this.isShowingPlaceholder_()) {
      // Never show the actual placeholder, clear the field instead.
      this.value = '';
      this.focus();
    }

    this.showPassword = !this.showPassword;
    event.stopPropagation();
  },

  /**
   * @param {!Event} event
   * @private
   */
  onKeydown_(event) {
    if (event.target.id === 'input' && event.key === 'Enter') {
      event.stopPropagation();
      this.fire('enter');
      return;
    }

    if (!this.isShowingPlaceholder_()) {
      return;
    }

    if (event.key.indexOf('Arrow') < 0 && event.key !== 'Home' &&
        event.key !== 'End') {
      return;
    }

    // Prevent cursor navigation keys from working when the placeholder password
    // is displayed. This prevents using the arrows or home/end keys to
    // remove or change the selection.
    if (event.cancelable) {
      event.preventDefault();
    }
  },

  /**
   * @param {!Event} event
   * @private
   */
  onMousedown_(event) {
    if (!this.isShowingPlaceholder_()) {
      return;
    }

    if (document.activeElement !== event.target) {
      // Focus the field and select the placeholder text if not already focused.
      this.focus();
    }

    // Prevent using the mouse or touchscreen to move the cursor or change the
    // selection when the placeholder password is displayed.  This prevents
    // the user from modifying the placeholder, only allows it to be left alone
    // or completely removed.
    if (event.cancelable) {
      event.preventDefault();
    }
  },
});
