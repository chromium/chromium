// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import './advanced_settings_dialog.js';
import './settings_section.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {Destination} from '../data/destination.js';

import {getCss} from './advanced_options_settings.css.js';
import {getHtml} from './advanced_options_settings.html.js';

interface PrintPreviewAdvancedOptionsSettingsElement {
  $: {
    button: CrButtonElement,
  };
}

class PrintPreviewAdvancedOptionsSettingsElement extends CrLitElement {
  static get is() {
    return 'print-preview-advanced-options-settings';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      disabled: {type: Boolean},
      destination: {type: Object},
      showAdvancedDialog_: {type: Boolean},
    };
  }

  accessor disabled: boolean = false;
  accessor destination: Destination|null = null;
  protected accessor showAdvancedDialog_: boolean = false;

  protected onButtonClick_() {
    this.showAdvancedDialog_ = true;
  }

  protected onDialogClose_() {
    this.showAdvancedDialog_ = false;
    this.$.button.focus();
  }
}

export type AdvancedOptionsSettingsElement =
    PrintPreviewAdvancedOptionsSettingsElement;

customElements.define(
    PrintPreviewAdvancedOptionsSettingsElement.is,
    PrintPreviewAdvancedOptionsSettingsElement);
