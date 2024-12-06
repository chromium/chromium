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

import {I18nBehavior, I18nBehaviorInterface} from '//resources/ash/common/i18n_behavior.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {NetworkType, OncSource} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';

import {CrPolicyNetworkBehaviorMojo, CrPolicyNetworkBehaviorMojoInterface} from './cr_policy_network_behavior_mojo.js';
import {NetworkConfigElementBehavior, NetworkConfigElementBehaviorInterface} from './network_config_element_behavior.js';
import {getTemplate} from './network_password_input.html.js';
import {FAKE_CREDENTIAL} from './onc_mojo.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 * @implements {CrPolicyNetworkBehaviorMojoInterface}
 * @implements {NetworkConfigElementBehaviorInterface}
 */
const NetworkPasswordInputElementBase = mixinBehaviors(
    [
      I18nBehavior,
      CrPolicyNetworkBehaviorMojo,
      NetworkConfigElementBehavior,
    ],
    PolymerElement);

/** @polymer */
class NetworkPasswordInputElement extends NetworkPasswordInputElementBase {
  static get is() {
    return 'network-password-input';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
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
    };
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.tooltipPosition_ =
        window.getComputedStyle(this).direction === 'rtl' ? 'right' : 'left';
  }

  /** @private */
  focus() {
    this.shadowRoot.querySelector('cr-input').focus();

    // If the input has any contents, the should be selected when focus is
    // applied.
    this.shadowRoot.querySelector('cr-input').select();
  }

  /**
   * @return {string}
   * @private
   */
  getInputType_() {
    return this.showPassword ? 'text' : 'password';
  }

  /**
   * @return {boolean}
   * @private
   */
  isShowingPlaceholder_() {
    return this.value === FAKE_CREDENTIAL;
  }

  /**
   * @return {string}
   * @private
   */
  getIconClass_() {
    return this.showPassword ? 'icon-visibility-off' : 'icon-visibility';
  }

  /**
   * @return {string}
   * @private
   */
  getShowPasswordTitle_() {
    return this.showPassword ? this.i18n('hidePassword') :
                               this.i18n('showPassword');
  }

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
  }

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
  }

  /**
   * @param {!Event} event
   * @private
   */
  onKeydown_(event) {
    if (event.target.id === 'input' && event.key === 'Enter') {
      event.stopPropagation();
      this.dispatchEvent(
          new CustomEvent('enter', {bubbles: true, composed: true}));
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
  }

  /**
   * If the fake password is showing, delete the password and focus the input
   * when clicked so the user can enter a new password.
   * @param {!Event} event
   * @private
   */
  onMousedown_(event) {
    if (!this.isShowingPlaceholder_()) {
      return;
    }

    this.value = '';
    if (document.activeElement !== event.target) {
      // Focus the field if not already focused.
      this.focus();
    }
  }
}

customElements.define(
    NetworkPasswordInputElement.is, NetworkPasswordInputElement);
