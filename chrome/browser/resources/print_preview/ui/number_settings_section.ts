// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import './print_preview_shared.css.js';
import './print_preview_vars.css.js';
import './settings_section.js';

import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
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
    if (e.detail === '') {
      // Set current value first in this case, because if the input was
      // previously invalid, it will become valid in the line below but
      // we do not want to set the setting to the invalid value.
      this.currentValue = '';
    }
    this.inputValid = this.$.userValue.validate();
    this.currentValue = e.detail;
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
    if (this.currentValue === '') {
      this.currentValue = this.defaultValue;
      this.inputValid = this.$.userValue.validate();
    }
    if (this.$.userValue.value === '') {
      this.$.userValue.value = this.defaultValue;
    }
  }

  private onCurrentValueChanged_() {
    if (this.currentValue !== this.$.userValue.value) {
      this.$.userValue.value = this.currentValue;
      this.inputValid = this.$.userValue.validate();
    }
    this.resetString();
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
