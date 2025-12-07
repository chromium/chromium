// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import './number_settings_section.js';

import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {CopiesCapability} from '../data/cdd.js';

import {getCss} from './copies_settings.css.js';
import {getHtml} from './copies_settings.html.js';
import {SettingsMixin} from './settings_mixin.js';

/**
 * Maximum number of copies supported by the printer if not explicitly
 * specified.
 */
export const DEFAULT_MAX_COPIES: number = 999;

export interface PrintPreviewCopiesSettingsElement {
  $: {
    collate: CrCheckboxElement,
  };
}

const PrintPreviewCopiesSettingsElementBase = SettingsMixin(CrLitElement);

export class PrintPreviewCopiesSettingsElement extends
    PrintPreviewCopiesSettingsElementBase {
  static get is() {
    return 'print-preview-copies-settings';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      capability: {type: Object},
      copiesMax_: {type: Number},
      currentValue_: {type: String},
      inputValid_: {type: Boolean},
      disabled: {type: Boolean},
      collateAvailable_: {type: Boolean},
    };
  }

  accessor capability: CopiesCapability|null = null;
  accessor disabled: boolean = false;
  protected accessor copiesMax_: number = DEFAULT_MAX_COPIES;
  protected accessor currentValue_: string = '';
  protected accessor inputValid_: boolean = false;
  private accessor collateAvailable_: boolean = false;

  override connectedCallback() {
    super.connectedCallback();

    this.addSettingObserver('copies.value', () => this.onSettingsChanged_());
    this.addSettingObserver('collate.*', () => this.onSettingsChanged_());
    this.onSettingsChanged_();

    this.addSettingObserver('collate.available', (value: boolean) => {
      this.collateAvailable_ = value;
    });
    this.collateAvailable_ = this.getSetting('collate').available;
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('capability')) {
      this.copiesMax_ = this.computeCopiesMax_();
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('currentValue_')) {
      this.onInputChanged_();
    }
  }

  /**
   * @return The maximum number of copies this printer supports.
   */
  private computeCopiesMax_(): number {
    return (this.capability && this.capability.max) ? this.capability.max :
                                                      DEFAULT_MAX_COPIES;
  }

  /**
   * @return The message to show as hint.
   */
  protected getHintMessage_(): string {
    return loadTimeData.getStringF('copiesInstruction', this.copiesMax_);
  }

  /**
   * Updates the input string when the setting has been initialized.
   */
  private onSettingsChanged_() {
    const copies = this.getSetting('copies');
    if (this.inputValid_) {
      this.currentValue_ = (copies.value as number).toString();
    }
    const collate = this.getSetting('collate');
    this.$.collate.checked = collate.value as boolean;
  }

  /**
   * Updates model.copies and model.copiesInvalid based on the validity
   * and current value of the copies input.
   */
  private onInputChanged_() {
    if (this.currentValue_ !== '' &&
        this.currentValue_ !== this.getSettingValue('copies').toString()) {
      this.setSetting(
          'copies', this.inputValid_ ? parseInt(this.currentValue_, 10) : 1);
    }
    this.setSettingValid('copies', this.inputValid_);
  }

  /**
   * @return Whether collate checkbox should be hidden.
   */
  protected collateHidden_(): boolean {
    return !this.collateAvailable_ || !this.inputValid_ ||
        this.currentValue_ === '' || parseInt(this.currentValue_, 10) === 1;
  }

  protected onCollateChange_() {
    this.setSetting('collate', this.$.collate.checked);
  }

  protected onCurrentValueChanged_(e: CustomEvent<{value: string}>) {
    this.currentValue_ = e.detail.value;
  }

  protected onInputValidChanged_(e: CustomEvent<{value: boolean}>) {
    this.inputValid_ = e.detail.value;
  }
}

export type CopiesSettingsElement = PrintPreviewCopiesSettingsElement;

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-copies-settings': PrintPreviewCopiesSettingsElement;
  }
}

customElements.define(
    PrintPreviewCopiesSettingsElement.is, PrintPreviewCopiesSettingsElement);
