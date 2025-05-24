// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for network configuration toggle.
 */
import '//resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import '//resources/ash/common/cr_elements/cr_shared_vars.css.js';
import './cr_policy_network_indicator_mojo.js';
import './network_shared.css.js';

import {assert} from '//resources/js/assert.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {CrPolicyNetworkBehaviorMojoInterface} from './cr_policy_network_behavior_mojo.js';
import {CrPolicyNetworkBehaviorMojo} from './cr_policy_network_behavior_mojo.js';
import type {NetworkConfigElementBehaviorInterface} from './network_config_element_behavior.js';
import {NetworkConfigElementBehavior} from './network_config_element_behavior.js';
import {getTemplate} from './network_config_toggle.html.js';

const NetworkConfigToggleElementBase = mixinBehaviors(
                                           [
                                             CrPolicyNetworkBehaviorMojo,
                                             NetworkConfigElementBehavior,
                                           ],
                                           PolymerElement) as {
  new (): PolymerElement & CrPolicyNetworkBehaviorMojoInterface &
      NetworkConfigElementBehaviorInterface,
};

export class NetworkConfigToggleElement extends NetworkConfigToggleElementBase {
  static get is() {
    return 'network-config-toggle' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      label: String,

      subLabel: String,

      checked: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
        notify: true,
      },

      /**
       * Uses Settings styling when true (policy icon is left of the toggle)
       */
      policyOnLeft: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
    };
  }

  checked: boolean;
  label: string;
  policyOnLeft: boolean;
  subLabel: string;

  override ready() {
    super.ready();

    this.addEventListener('click', this.onHostTap_.bind(this));
  }

  override focus() {
    const toggleElement = this.shadowRoot!.querySelector('cr-toggle');
    assert(!!toggleElement);
    toggleElement.focus();
  }

  /**
   * Handles non cr-toggle button clicks (cr-toggle handles its own click events
   * which don't bubble).
   */
  private onHostTap_(e: Event) {
    e.stopPropagation();
    if (this.getDisabled_(this.disabled, this.property)) {
      return;
    }
    this.checked = !this.checked;
    this.dispatchEvent(
        new CustomEvent('change', {bubbles: true, composed: true}));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [NetworkConfigToggleElement.is]: NetworkConfigToggleElement;
  }
}

customElements.define(
    NetworkConfigToggleElement.is, NetworkConfigToggleElement);
