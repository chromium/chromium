// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-checkbox-with-policy' is a cr-checkbox which additionally
 * shows a policy icon with a given message when the checkbox is disabled.
 * All properties are the same as <cr-checkbox>, except for an additional
 * `policyTooltip` property.
 *
 * This element is required to ensure that the focus is on the checkbox if it is
 * enabled, and and the policy icon if it is disabled. This also works around an
 * issue with <iron-list>, which causes it to not be able to focus the correct
 * element when using arrow key navigation if the parent repeated element does
 * not have the expected tabIndex AND the element with the expected tabIndex is
 * hidden in a shadow root - which is the case for <cr-checkbox>es.
 *
 * See the definition of `_focusPhysicalItem` in iron-list for more detail
 * about the issue. If `physicalItem` - the parent repeated element - does not
 * have the secret tab index set, it uses a querySelector to find the correct
 * element to focus. This does not work when the element to focus is in a shadow
 * root.
 */
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cr_checkbox_with_policy.html.js';

/** @polymer */
class CrCheckboxWithPolicyElement extends PolymerElement {
  static get is() {
    return 'cr-checkbox-with-policy';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      policyTooltip: String,

      // All properties below are from cr-checkbox.
      checked: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
        notify: true,
      },

      disabled: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      ariaDescription: String,

      tabIndex: {
        type: Number,
        value: 0,
        observer: 'onTabIndexChanged_',
      },
    };
  }

  /**
   * Focuses the correct element (icon if disabled, otherwise checkbox).
   *
   * Overrides focus() defined on HTMLElement.
   * @override
   */
  focus() {
    const elementId = this.disabled ? 'icon' : 'checkbox';
    const element = this.shadowRoot.getElementById(elementId);
    element.focus();
  }

  /** @private */
  onTabIndexChanged_() {
    // :host shouldn't have a tabindex because it's set on the appropriate
    // element instead.
    this.removeAttribute('tabindex');
  }

  /**
   * Returns the correct tab index for the checkbox (-1 if it is disabled).
   * @private
   * @return {number}
   */
  getCheckboxTabIndex_() {
    return this.disabled ? -1 : this.tabIndex;
  }

  /**
   * Returns the correct tab index for the icon (-1 if it is enabled).
   * @private
   * @return {number}
   */
  getIconTabIndex_() {
    return this.disabled ? this.tabIndex : -1;
  }
}

customElements.define(
    CrCheckboxWithPolicyElement.is, CrCheckboxWithPolicyElement);
