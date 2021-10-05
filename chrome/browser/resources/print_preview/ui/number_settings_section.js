// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import './print_preview_shared_css.js';
import './print_preview_vars_css.js';
import './settings_section.js';

import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {InputBehavior, InputBehaviorInterface} from './input_behavior.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {InputBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const PrintPreviewNumberSettingsSectionElementBase =
    mixinBehaviors([InputBehavior, WebUIListenerBehavior], PolymerElement);

/** @polymer */
export class PrintPreviewNumberSettingsSectionElement extends
    PrintPreviewNumberSettingsSectionElementBase {
  static get is() {
    return 'print-preview-number-settings-section';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private {string} */
      inputString_: {
        type: String,
        notify: true,
        observer: 'onInputStringChanged_',
      },

      /** @type {boolean} */
      inputValid: {
        type: Boolean,
        notify: true,
        reflectToAttribute: true,
        value: true,
      },

      /** @type {string} */
      currentValue: {
        type: String,
        notify: true,
        observer: 'onCurrentValueChanged_',
      },

      defaultValue: String,

      maxValue: Number,

      minValue: Number,

      inputLabel: String,

      inputAriaLabel: String,

      hintMessage: String,

      disabled: Boolean,

      /** @private */
      errorMessage_: {
        type: String,
        computed: 'computeErrorMessage_(hintMessage, inputValid)',
      },

    };
  }

  ready() {
    super.ready();

    this.addEventListener(
        'input-change',
        e => this.onInputChangeEvent_(/** @type {!CustomEvent<string>} */ (e)));
  }

  /** @return {!CrInputElement} The cr-input field element for InputBehavior. */
  getInput() {
    return /** @type {!CrInputElement} */ (this.$.userValue);
  }

  /**
   * @param {!CustomEvent<string>} e Contains the new input value.
   * @private
   */
  onInputChangeEvent_(e) {
    this.inputString_ = e.detail;
  }

  /**
   * @return {boolean} Whether the input should be disabled.
   * @private
   */
  getDisabled_() {
    return this.disabled && this.inputValid;
  }

  /**
   * @param {!KeyboardEvent} e The keyboard event
   * @private
   */
  onKeydown_(e) {
    if (['.', 'e', 'E', '-', '+'].includes(e.key)) {
      e.preventDefault();
      return;
    }

    if (e.key === 'Enter') {
      this.onBlur_();
    }
  }

  /** @private */
  onBlur_() {
    if (this.inputString_ === '') {
      this.set('inputString_', this.defaultValue);
    }
    if (this.$.userValue.value === '') {
      this.$.userValue.value = this.defaultValue;
    }
  }

  /** @private */
  onInputStringChanged_() {
    this.inputValid = this.computeValid_();
    this.currentValue = this.inputString_;
  }

  /** @private */
  onCurrentValueChanged_() {
    this.inputString_ = this.currentValue;
    this.resetString();
  }

  /**
   * @return {boolean} Whether input value represented by inputString_ is
   *     valid and non-empty, so that it can be used to update the setting.
   * @private
   */
  computeValid_() {
    // Make sure value updates first, in case inputString_ was updated by JS.
    this.$.userValue.value = this.inputString_;
    return !this.$.userValue.invalid;
  }

  /**
   * @return {string}
   * @private
   */
  computeErrorMessage_() {
    return this.inputValid ? '' : this.hintMessage;
  }
}

customElements.define(
    PrintPreviewNumberSettingsSectionElement.is,
    PrintPreviewNumberSettingsSectionElement);
