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

import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrPolicyNetworkBehaviorMojo, CrPolicyNetworkBehaviorMojoInterface} from './cr_policy_network_behavior_mojo.js';
import {NetworkConfigElementBehavior, NetworkConfigElementBehaviorInterface} from './network_config_element_behavior.js';
import {getTemplate} from './network_config_input.html.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {CrPolicyNetworkBehaviorMojoInterface}
 * @implements {NetworkConfigElementBehaviorInterface}
 */
const NetworkConfigInputElementBase = mixinBehaviors(
    [
      CrPolicyNetworkBehaviorMojo,
      NetworkConfigElementBehavior,
    ],
    PolymerElement);

/** @polymer */
class NetworkConfigInputElement extends NetworkConfigInputElementBase {
  static get is() {
    return 'network-config-input';
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
    };
  }

  focus() {
    this.shadowRoot.querySelector('cr-input').focus();
  }

  /**
   * @param {!Event} event
   * @private
   */
  onKeypress_(event) {
    if (event.key !== 'Enter') {
      return;
    }
    event.stopPropagation();
    this.dispatchEvent(
        new CustomEvent('enter', {bubbles: true, composed: true}));
  }
}

customElements.define(NetworkConfigInputElement.is, NetworkConfigInputElement);
