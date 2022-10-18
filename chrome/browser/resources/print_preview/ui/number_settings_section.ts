// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import './print_preview_shared.css.js';
import './print_preview_vars.css.js';
import './settings_section.js';

import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {InputMixin} from './input_mixin.js';
import {getTemplate} from './number_settings_section.html.js';

export interface PrintPreviewNumberSettingsSectionElement {
  $: {
    userValue: CrInputElement,
  };
}

const PrintPreviewNumberSettingsSectionElementBase =
    WebUiListenerMixin(InputMixin(PolymerElement));

export class PrintPreviewNumberSettingsSectionElement extends
    PrintPreviewNumberSettingsSectionElementBase {
  static get is() {
    return 'print-preview-number-settings-section';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      inputString_: {
        type: String,
        notify: true,
        observer: 'onInputStringChanged_',
      },

      inputValid: {
        type: Boolean,
        notify: true,
        reflectToAttribute: true,
        value: true,
      },

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

      errorMessage_: {
        type: String,
        computed: 'computeErrorMessage_(hintMessage, inputValid)',
      },
    };
  }

  currentValue: string;
  defaultValue: string;
  disabled: boolean;
  hintMessage: string;
  inputAriaLabel: string;
  inputLabel: string;
  inputValid: boolean;
  minValue: number;
  maxValue: number;
  private inputString_: string;
  private errorMessage_: string;

  override ready() {
    super.ready();

    this.addEventListener('input-change', e => this.onInputChangeEvent_(e));
  }

  /** @return The cr-input field element for InputBehavior. */
  override getInput() {
    return this.$.userValue;
  }

  /**
   * @param e Contains the new input value.
   */
  private onInputChangeEvent_(e: CustomEvent<string>) {
    this.inputString_ = e.detail!;
  }

  /**
   * @return Whether the input should be disabled.
   */
  private getDisabled_(): boolean {
    return this.disabled && this.inputValid;
  }

  private onKeydown_(e: KeyboardEvent) {
    if (['.', 'e', 'E', '-', '+'].includes(e.key)) {
      e.preventDefault();
      return;
    }

    if (e.key === 'Enter') {
      this.onBlur_();
    }
  }

  private onBlur_() {
    if (this.inputString_ === '') {
      this.set('inputString_', this.defaultValue);
    }
    if (this.$.userValue.value === '') {
      this.$.userValue.value = this.defaultValue;
    }
  }

  private onInputStringChanged_() {
    this.inputValid = this.computeValid_();
    this.currentValue = this.inputString_;
  }

  private onCurrentValueChanged_() {
    this.inputString_ = this.currentValue;
    this.resetString();
  }

  /**
   * @return Whether input value represented by inputString_ is
   *     valid and non-empty, so that it can be used to update the setting.
   */
  private computeValid_(): boolean {
    // Make sure value updates first, in case inputString_ was updated by JS.
    this.$.userValue.value = this.inputString_;
    return !this.$.userValue.invalid;
  }

  private computeErrorMessage_(): string {
    return this.inputValid ? '' : this.hintMessage;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-number-settings-section':
        PrintPreviewNumberSettingsSectionElement;
  }
}

customElements.define(
    PrintPreviewNumberSettingsSectionElement.is,
    PrintPreviewNumberSettingsSectionElement);
