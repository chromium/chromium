// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for network configuration input fields.
 */
import '//resources/ash/common/cr_elements/cr_input/cr_input.js';
import '//resources/ash/common/cr_elements/cr_shared_vars.css.js';
import './cr_policy_network_indicator_mojo.js';
import './network_shared.css.js';

import {assert} from '//resources/js/assert.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrPolicyNetworkBehaviorMojo, CrPolicyNetworkBehaviorMojoInterface} from './cr_policy_network_behavior_mojo.js';
import {NetworkConfigElementBehavior, NetworkConfigElementBehaviorInterface} from './network_config_element_behavior.js';
import {getTemplate} from './network_config_input.html.js';

const NetworkConfigInputElementBase = mixinBehaviors(
    [
      CrPolicyNetworkBehaviorMojo,
      NetworkConfigElementBehavior,
    ],
    PolymerElement);

export class NetworkConfigInputElement extends NetworkConfigInputElementBase {
  static get is() {
    return 'network-config-input' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      label: String,

      hidden: {
        type: Boolean,
        reflectToAttribute: true,
      },

      invalid: {
        type: Boolean,
        value: false,
      },

      readonly: {
        type: Boolean,
        value: false,
      },

      value: String,
    };
  }

  label: string;
  override hidden: boolean;
  invalid: boolean;
  readonly: boolean;
  value: string;

  override focus() {
    const input = this.shadowRoot!.querySelector('cr-input');
    assert(input);
    input.focus();
  }

  private onKeypress_(event: KeyboardEvent): void {
    if (event.key !== 'Enter') {
      return;
    }
    event.stopPropagation();
    this.dispatchEvent(
        new CustomEvent('enter', {bubbles: true, composed: true}));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [NetworkConfigInputElement.is]: NetworkConfigInputElement;
  }
}

customElements.define(NetworkConfigInputElement.is, NetworkConfigInputElement);
