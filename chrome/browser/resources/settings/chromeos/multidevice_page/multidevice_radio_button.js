// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button_style.css.js';
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.js';
import '../../settings_shared.css.js';

import {CrRadioButtonMixin} from 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button_mixin.js';
import {PaperRippleBehavior} from 'chrome://resources/polymer/v3_0/paper-behaviors/paper-ripple-behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

class PaperRippleBehaviorInterface {
  constructor() {
    /**
     * @type {?Element}
     * @protected
     */
    this._rippleContainer;
  }

  /** @return {!HTMLElement} */
  getRipple() {}
}

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {CrRadioButtonMixinInterface}
 * @implements {PaperRippleBehaviorInterface}
 */
const MultideviceRadioButtonElementBase =
    mixinBehaviors([PaperRippleBehavior], CrRadioButtonMixin(PolymerElement));

/** @polymer */
class MultideviceRadioButtonElement extends MultideviceRadioButtonElementBase {
  static get is() {
    return 'multidevice-radio-button';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      ariaChecked: {
        type: String,
        notify: true,
        reflectToAttribute: true,
        computed: 'getAriaChecked_(checked)',
      },
      ariaDisabled: {
        type: String,
        notify: true,
        reflectToAttribute: true,
        computed: 'getAriaDisabled_(disabled)',

      },
      ariaLabel: {
        type: String,
        notify: true,
        reflectToAttribute: true,
        computed: 'getLabel_(label)',
      },
    };
  }

  ready() {
    super.ready();
    this.setAttribute('role', 'radio');
  }

  // Overridden from CrRadioButtonMixin
  /** @override */
  getPaperRipple() {
    return this.getRipple();
  }

  // Overridden from PaperRippleBehavior
  /* eslint-disable-next-line @typescript-eslint/naming-convention */
  _createRipple() {
    this._rippleContainer = this.shadowRoot.querySelector('.disc-wrapper');
    const ripple = super._createRipple();
    ripple.id = 'ink';
    ripple.setAttribute('recenters', '');
    ripple.classList.add('circle', 'toggle-ink');
    return ripple;
  }

  getLabel_(label) {
    return label;
  }

  /**
   * Prevents on-click handles on the control from being activated when the
   * indicator is clicked.
   * @param {!Event} e The click event.
   * @private
   */
  onIndicatorTap_(e) {
    e.preventDefault();
    e.stopPropagation();
  }
}

customElements.define(
    MultideviceRadioButtonElement.is, MultideviceRadioButtonElement);
