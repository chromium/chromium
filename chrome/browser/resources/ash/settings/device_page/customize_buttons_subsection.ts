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
import './key_combination_input_dialog.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ShowKeyCustomizationDialogEvent, ShowRenamingDialogEvent} from './customize_button_row.js';
import {getTemplate} from './customize_buttons_subsection.html.js';
import {DragAndDropManager, OnDropCallback} from './drag_and_drop_manager.js';
import {ActionChoice, ButtonRemapping} from './input_device_settings_types.js';
import {KeyCombinationInputDialogElement} from './key_combination_input_dialog.js';

export interface CustomizeButtonsSubsectionElement {
  $: {
    keyCombinationInputDialog: KeyCombinationInputDialogElement,
  };
}

declare global {
  interface HTMLElementEventMap {
    'show-renaming-dialog': ShowRenamingDialogEvent;
    'show-key-combination-dialog': ShowKeyCustomizationDialogEvent;
  }
}

const MAX_BUTTON_NAME_INPUT_LENGTH = 32;

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
        observer: 'onNameInputChanged_',
      },

      selectedButtonIndex_: {
        type: Number,
      },

      buttonNameInvalid_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      isSaveButtonDisabled_: {
        type: Boolean,
        value: false,
      },

      duplicateButtonName_: {
        type: Boolean,
        value: false,
      },
    };
  }

  buttonRemappingList: ButtonRemapping[];
  actionList: ActionChoice[];
  private selectedButton_: ButtonRemapping;
  private selectedButtonIndex_: number;
  private shouldShowRenamingDialog_: boolean;
  private selectedButtonName_: string;
  private dragAndDropManager: DragAndDropManager = new DragAndDropManager();
  private buttonNameInvalid_: boolean;
  private isSaveButtonDisabled_: boolean;
  private duplicateButtonName_: boolean;

  override connectedCallback(): void {
    super.connectedCallback();
    this.addEventListener('show-renaming-dialog', this.showRenamingDialog_);
    this.addEventListener(
        'show-key-combination-dialog', this.showKeyCombinationDialog_);
    this.dragAndDropManager.init(this, this.onDrop_.bind(this));
  }

  override disconnectedCallback(): void {
    this.dragAndDropManager.destroy();
  }

  private showRenamingDialog_(e: ShowRenamingDialogEvent): void {
    this.selectedButtonIndex_ = e.detail.buttonIndex;
    this.selectedButton_ = this.buttonRemappingList[this.selectedButtonIndex_];
    this.selectedButtonName_ = this.selectedButton_.name;
    this.buttonNameInvalid_ = false;
    this.isSaveButtonDisabled_ = false;
    this.duplicateButtonName_ = false;
    this.shouldShowRenamingDialog_ = true;
  }

  /**
   * Returns a formatted string containing the current number of characters
   * entered in the input compared to the maximum number of characters allowed.
   */
  private getInputCountString_(buttonName: string): string {
    // minimumIntegerDigits is 2 because we want to show a leading zero if
    // length is less than 10.
    return this.i18n(
        'buttonRenamingDialogInputCharCount',
        buttonName.length.toLocaleString(
            /*locales=*/ undefined, {minimumIntegerDigits: 2}),
        MAX_BUTTON_NAME_INPUT_LENGTH.toLocaleString());
  }

  private showKeyCombinationDialog_(e: ShowKeyCustomizationDialogEvent): void {
    this.selectedButtonIndex_ = e.detail.buttonIndex;
    this.$.keyCombinationInputDialog.showModal();
  }

  private cancelRenamingDialogClicked_(): void {
    this.shouldShowRenamingDialog_ = false;
  }

  private saveRenamingDialogClicked_(): void {
    if (this.isSaveButtonDisabled_) {
      return;
    }

    if (this.sameButtonNameExists_()) {
      this.buttonNameInvalid_ = true;
      this.duplicateButtonName_ = true;
      return;
    }

    this.updateButtonName_();
    this.shouldShowRenamingDialog_ = false;
  }

  private onKeyDownInRenamingDialog_(event: KeyboardEvent): void {
    if (event.key === 'Enter') {
      this.saveRenamingDialogClicked_();
    }
  }

  private onNameInputChanged_(_newValue: string, oldValue: string): void {
    // If oldValue.length > MAX_BUTTON_NAME_INPUT_LENGTH, the user attempted
    // to enter more than the max limit, this method was called and it was
    // truncated, and then this method was called one more time.
    this.buttonNameInvalid_ =
        !!oldValue && oldValue.length > MAX_BUTTON_NAME_INPUT_LENGTH;
    this.duplicateButtonName_ = false;
    // Truncate the name to maxInputLength.
    this.selectedButtonName_ =
        this.selectedButtonName_.substring(0, MAX_BUTTON_NAME_INPUT_LENGTH);
    this.isSaveButtonDisabled_ = this.selectedButtonName_ === '';
  }

  /**
   * Button names within one device should be unique.
   */
  private sameButtonNameExists_(): boolean {
    for (const button of this.buttonRemappingList) {
      if (button.name !== this.selectedButton_.name &&
          button.name === this.selectedButtonName_) {
        return true;
      }
    }

    return false;
  }

  private updateButtonName_(): void {
    if (!!this.selectedButtonName_ &&
        this.selectedButton_.name !== this.selectedButtonName_) {
      this.set(
          `buttonRemappingList.${this.selectedButtonIndex_}.name`,
          this.selectedButtonName_);
      this.dispatchEvent(new CustomEvent('button-remapping-changed', {
        bubbles: true,
        composed: true,
      }));
    }
    this.selectedButtonName_ = '';
  }

  private onDrop_: OnDropCallback =
      (originIndex: number, destinationIndex: number) => {
        if (originIndex < 0 || originIndex >= this.buttonRemappingList.length ||
            destinationIndex < 0 ||
            destinationIndex >= this.buttonRemappingList.length) {
          return;
        }

        // Move the item in this.buttonRemappingList from originIndex
        // to destinationIndex.
        const movedItem = this.buttonRemappingList[originIndex];
        // Remove item at origin index
        this.splice('buttonRemappingList', originIndex, 1);
        // Add item at destination index
        this.splice('buttonRemappingList', destinationIndex, 0, movedItem);

        this.dispatchEvent(new CustomEvent('button-remapping-changed', {
          bubbles: true,
          composed: true,
        }));
      };
}

declare global {
  interface HTMLElementTagNameMap {
    [CustomizeButtonsSubsectionElement.is]: CustomizeButtonsSubsectionElement;
  }
}

customElements.define(
    CustomizeButtonsSubsectionElement.is, CustomizeButtonsSubsectionElement);
