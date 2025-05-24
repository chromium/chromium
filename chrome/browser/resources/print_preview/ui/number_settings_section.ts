// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import './settings_section.js';

import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {InputMixin} from './input_mixin.js';
import {getCss} from './number_settings_section.css.js';
import {getHtml} from './number_settings_section.html.js';

export interface PrintPreviewNumberSettingsSectionElement {
  $: {
    userValue: CrInputElement,
  };
}

const PrintPreviewNumberSettingsSectionElementBase =
    WebUiListenerMixinLit(InputMixin(CrLitElement));

export class PrintPreviewNumberSettingsSectionElement extends
    PrintPreviewNumberSettingsSectionElementBase {
  static get is() {
    return 'print-preview-number-settings-section';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      inputValid: {
        type: Boolean,
        notify: true,
        reflect: true,
      },

      currentValue: {
        type: String,
        notify: true,
      },

      defaultValue: {type: String},
      maxValue: {type: Number},
      minValue: {type: Number},
      inputLabel: {type: String},
      inputAriaLabel: {type: String},
      hintMessage: {type: String},
      disabled: {type: Boolean},
      errorMessage_: {type: String},
    };
  }

  accessor currentValue: string = '';
  accessor defaultValue: string = '';
  accessor disabled: boolean = false;
  accessor hintMessage: string = '';
  accessor inputAriaLabel: string = '';
  accessor inputLabel: string = '';
  accessor inputValid: boolean = true;
  accessor minValue: number|undefined;
  accessor maxValue: number|undefined;
  protected accessor errorMessage_: string = '';

  override firstUpdated() {
    this.addEventListener('input-change', e => this.onInputChangeEvent_(e));
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('hintMessage') ||
        changedProperties.has('inputValid')) {
      this.errorMessage_ = this.inputValid ? '' : this.hintMessage;
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('currentValue')) {
      this.onCurrentValueChanged_();
    }
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
  protected getDisabled_(): boolean {
    return this.disabled && this.inputValid;
  }

  protected onKeydown_(e: KeyboardEvent) {
    if (['.', 'e', 'E', '-', '+'].includes(e.key)) {
      e.preventDefault();
      return;
    }

    if (e.key === 'Enter') {
      this.onBlur_();
    }
  }

  protected onBlur_() {
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
}

export type NumberSettingsSectionElement =
    PrintPreviewNumberSettingsSectionElement;

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-number-settings-section':
        PrintPreviewNumberSettingsSectionElement;
  }
}

customElements.define(
    PrintPreviewNumberSettingsSectionElement.is,
    PrintPreviewNumberSettingsSectionElement);
