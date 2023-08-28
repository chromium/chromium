// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'customize-buttons-subsection' contains a list of 'customize-button-row'
 * elements that allow users to remap buttons to actions or key combinations.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import '../settings_shared.css.js';
import './customize_button_row.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ShowRenamingDialogEvent} from './customize_button_row.js';
import {getTemplate} from './customize_buttons_subsection.html.js';
import {ActionChoice, ButtonRemapping} from './input_device_settings_types.js';


declare global {
  interface HTMLElementEventMap {
    'show-renaming-dialog': ShowRenamingDialogEvent;
  }
}

const CustomizeButtonsSubsectionElementBase = I18nMixin(PolymerElement);

export class CustomizeButtonsSubsectionElement extends
    CustomizeButtonsSubsectionElementBase {
  static get is() {
    return 'customize-buttons-subsection' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      actionList: {
        type: Array,
      },

      buttonRemappingList: {
        type: Array,
      },

      selectedButton_: {
        type: Object,
      },

      shouldShowRenamingDialog_: {
        type: Boolean,
        value: false,
      },

      selectedButtonName_: {
        type: String,
        value: '',
      },
    };
  }

  buttonRemappingList: ButtonRemapping[];
  actionList: ActionChoice[];
  private selectedButton_: ButtonRemapping;
  private shouldShowRenamingDialog_: boolean;
  private selectedButtonName_: string;

  override connectedCallback(): void {
    super.connectedCallback();
    this.addEventListener('show-renaming-dialog', this.showRenamingDialog_);
  }

  private showRenamingDialog_(e: ShowRenamingDialogEvent): void {
    const selectedIndex = e.detail.buttonIndex;
    this.selectedButton_ = this.buttonRemappingList[selectedIndex];
    this.selectedButtonName_ = this.selectedButton_.name;
    this.shouldShowRenamingDialog_ = true;
  }

  private cancelRenamingDialogClicked_(): void {
    this.shouldShowRenamingDialog_ = false;
  }

  private saveRenamingDialogClicked_(): void {
    if (this.selectedButton_.name !== this.selectedButtonName_) {
      this.onSettingsChanged();
    }
    this.selectedButtonName_ = '';
    this.shouldShowRenamingDialog_ = false;
  }

  onSettingsChanged(): void {
    // TODO(yyhyyh@): Update changed buttonRemapping.
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [CustomizeButtonsSubsectionElement.is]: CustomizeButtonsSubsectionElement;
  }
}

customElements.define(
    CustomizeButtonsSubsectionElement.is, CustomizeButtonsSubsectionElement);
