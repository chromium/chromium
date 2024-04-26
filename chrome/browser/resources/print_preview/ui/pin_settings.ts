// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import './print_preview_shared.css.js';
import './settings_section.js';

import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {State} from '../data/state.js';

import {InputMixin} from './input_mixin.js';
import {getTemplate} from './pin_settings.html.js';
import {SettingsMixin} from './settings_mixin.js';

export interface PrintPreviewPinSettingsElement {
  $: {
    pin: CrCheckboxElement,
    pinValue: CrInputElement,
  };
}

const PrintPreviewPinSettingsElementBase =
    WebUiListenerMixin(InputMixin(SettingsMixin(I18nMixin(PolymerElement))));

export class PrintPreviewPinSettingsElement extends
    PrintPreviewPinSettingsElementBase {
  static get is() {
    return 'print-preview-pin-settings';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      state: Number,

      disabled: Boolean,

      checkboxDisabled_: {
        type: Boolean,
        computed: 'computeCheckboxDisabled_(inputValid_, disabled, ' +
            'settings.pin.setByPolicy)',
      },

      pinEnabled_: {
        type: Boolean,
        value: false,
      },

      inputString_: {
        type: String,
        value: '',
        observer: 'onInputChanged_',
      },

      isPinValid: {
        type: Boolean,
        reflectToAttribute: true,
        notify: true,
      },
    };
  }

  static get observers() {
    return [
      'onSettingsChanged_(settings.pin.value, settings.pinValue.value)',
      'changePinValueSetting_(state)',
    ];
  }

  state: State;
  disabled: boolean;
  isPinValid: boolean;
  private checkboxDisabled_: boolean;
  private inputString_: string;
  private pinEnabled_: boolean;

  override ready() {
    super.ready();

    this.addEventListener('input-change', e => this.onInputChange_(e));
  }

  /** @return The cr-input field element for InputMixin. */
  override getInput() {
    return this.$.pinValue;
  }

  private onInputChange_(e: CustomEvent<string>) {
    this.inputString_ = e.detail;
  }

  private onCollapseChanged_() {
    if (this.pinEnabled_) {
      this.$.pinValue.focusInput();
    }
  }

  /**
   * @param inputValid Whether pin value is valid.
   * @param disabled Whether pin setting is disabled.
   * @param managed Whether pin setting is managed.
   * @return Whether pin checkbox should be disabled.
   */
  private computeCheckboxDisabled_(
      inputValid: boolean, disabled: boolean, managed: boolean): boolean {
    return managed || (inputValid && disabled);
  }

  /**
   * @return Whether to disable the pin value input.
   */
  private inputDisabled_(): boolean {
    return !this.pinEnabled_ || (this.isPinValid && this.disabled);
  }

  /**
   * Updates the checkbox state when the setting has been initialized.
   */
  private onSettingsChanged_() {
    const pinEnabled = this.getSetting('pin').value as boolean;
    this.$.pin.checked = pinEnabled;
    this.pinEnabled_ = pinEnabled;
    const pinValue = this.getSetting('pinValue');
    this.inputString_ = pinValue.value as string;
    this.resetString();
  }

  private onPinChange_() {
    this.setSetting('pin', this.$.pin.checked);
    // We need to set validity of pinValue to true to return to READY state
    // after unchecking the pin and to check the validity again after checking
    // the pin.
    if (!this.$.pin.checked) {
      this.isPinValid = true;
    } else {
      this.changePinValueSetting_();
    }
  }

  private onInputChanged_() {
    this.changePinValueSetting_();
  }

  /**
   * Updates pin value setting based on the current value of the pin value
   * input.
   */
  private changePinValueSetting_() {
    if (this.settings === undefined) {
      return;
    }

    // Return early if pinValue is not available; unavailable settings should
    // not be set, but this function observes |state| which may change
    // regardless of pin availability.
    if (!this.settings.pinValue.available) {
      return;
    }

    // If the state is not READY and current pinValue is valid (so it's not the
    // cause of the error) we need to wait until the state will be READY again.
    // It's done because we don't permit multiple simultaneous validation errors
    // in Print Preview and we also don't want to set the value when sticky
    // settings may not yet have been set.
    if (this.state !== State.READY && this.settings.pinValue!.valid) {
      return;
    }
    this.isPinValid = this.computeValid_();

    // We allow to save the empty string as sticky setting value to give users
    // the opportunity to unset their PIN in sticky settings.
    if ((this.isPinValid || this.inputString_ === '') &&
        this.inputString_ !== this.getSettingValue('pinValue')) {
      this.setSetting('pinValue', this.inputString_);
    }
  }

  /**
   * @return Whether input value represented by inputString_ is
   *     valid, so that it can be used to update the setting.
   */
  private computeValid_(): boolean {
    // Make sure value updates first, in case inputString_ was updated by JS.
    this.$.pinValue.value = this.inputString_;
    return this.$.pinValue.validate();
  }

  private getPinErrorMessage_(): string {
    return this.isPinValid ? '' : this.i18n('pinErrorMessage');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-pin-settings': PrintPreviewPinSettingsElement;
  }
}


customElements.define(
    PrintPreviewPinSettingsElement.is, PrintPreviewPinSettingsElement);
