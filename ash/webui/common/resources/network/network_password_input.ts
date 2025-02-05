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

import {assert} from '//resources/js/assert.js';
import type {ManagedProperties} from '//resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {NetworkType, OncSource} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';

import {CrPolicyNetworkBehaviorMojo, CrPolicyNetworkBehaviorMojoInterface} from './cr_policy_network_behavior_mojo.js';
import {NetworkConfigElementBehavior, NetworkConfigElementBehaviorInterface} from './network_config_element_behavior.js';
import {getTemplate} from './network_password_input.html.js';
import {FAKE_CREDENTIAL} from './onc_mojo.js';

const NetworkPasswordInputElementBase = mixinBehaviors(
    [
      CrPolicyNetworkBehaviorMojo,
      NetworkConfigElementBehavior,
    ],
    I18nMixin(PolymerElement));

export class NetworkPasswordInputElement extends
    NetworkPasswordInputElementBase {
  static get is() {
    return 'network-password-input' as const;
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

      managedProperties: {
        type: Object,
        value: null,
      },

      disabled: {
        type: Boolean,
      },

      readonly: {
        type: Boolean,
      },

      value: {
        type: String,
      },

      tooltipPosition_: {
        type: String,
        value: '',
      },

      showPolicyIndicator_: {
        type: Boolean,
        value: false,
        computed: 'getDisabled_(disabled, property)',
      },
    };
  }

  label: string;
  showPassword: boolean;
  invalid: boolean;
  allowErrorMessage: boolean;
  errorMessage: string;
  managedProperties: ManagedProperties;
  disabled: boolean = false;
  readonly: boolean = false;
  value: string;
  override ariaLabel: string;
  private tooltipPosition_: string;
  private showPolicyIndicator_: boolean;

  override connectedCallback() {
    super.connectedCallback();

    this.tooltipPosition_ =
        window.getComputedStyle(this).direction === 'rtl' ? 'right' : 'left';
  }

  override focus(): void {
    const input = this.shadowRoot!.querySelector('cr-input');
    assert(input);

    input.focus();

    // If the input has any contents, the should be selected when focus is
    // applied.
    input.select();
  }

  private getInputType_(): string {
    return this.showPassword ? 'text' : 'password';
  }

  private isShowingPlaceholder_(): boolean {
    return this.value === FAKE_CREDENTIAL;
  }

  private getIconClass_(): string {
    return this.showPassword ? 'icon-visibility-off' : 'icon-visibility';
  }

  private getShowPasswordTitle_(): string {
    return this.showPassword ? this.i18n('hidePassword') :
                               this.i18n('showPassword');
  }

  /**
   * TODO(b/328633844): Update this function to make the "show password" button
   * visible for configured WiFi networks.
   * Used to control whether the Show Password button is visible.
   */
  private showPasswordIcon_(): boolean {
    return !this.showPolicyIndicator_ &&
        (!this.managedProperties ||
         this.managedProperties.source === OncSource.kNone);
  }

  private onShowPasswordTap_(event: Event) {
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

  private onKeydown_(event: KeyboardEvent) {
    const target = event.target as HTMLElement;
    assert(target);
    if (target.id === 'input' && event.key === 'Enter') {
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
   */
  private onMousedown_(event: Event) {
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

declare global {
  interface HTMLElementTagNameMap {
    [NetworkPasswordInputElement.is]: NetworkPasswordInputElement;
  }
}

customElements.define(
    NetworkPasswordInputElement.is, NetworkPasswordInputElement);
