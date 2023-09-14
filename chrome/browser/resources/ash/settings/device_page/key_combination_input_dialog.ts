// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import './input_device_settings_shared.css.js';
import '../settings_shared.css.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ButtonRemapping} from './input_device_settings_types.js';
import {getTemplate} from './key_combination_input_dialog.html.js';

/**
 * @fileoverview
 * 'key-combination-input-dialog' is a dialog that pops up after clicking the
 * 'Key combination' choice in the dropdown menu to allow users to input a
 * combination of keyboard keys as a button remapping action.
 */

export interface KeyCombinationInputDialogElement {
  $: {
    keyCombinationInputDialog: CrDialogElement,
  };
}

const KeyCombinationInputDialogElementBase = I18nMixin(PolymerElement);

export class KeyCombinationInputDialogElement extends
    KeyCombinationInputDialogElementBase {
  static get is() {
    return 'key-combination-input-dialog' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      buttonRemappingList: {
        type: Array,
      },

      remappingIndex: {
        type: Number,
      },

      buttonRemapping_: {
        type: Object,
      },

      isOpen: {
        type: Boolean,
        value: false,
      },
    };
  }

  buttonRemappingList: ButtonRemapping[];
  remappingIndex: number;
  isOpen: boolean;
  private buttonRemapping_: ButtonRemapping;

  showModal(): void {
    this.buttonRemapping_ = this.buttonRemappingList[this.remappingIndex];
    const keyCombinationInputDialog = this.$.keyCombinationInputDialog;
    keyCombinationInputDialog.showModal();
    this.isOpen = keyCombinationInputDialog.open;
  }

  close(): void {
    const keyCombinationInputDialog = this.$.keyCombinationInputDialog;
    keyCombinationInputDialog.close();
    this.isOpen = keyCombinationInputDialog.open;
  }

  private cancelDialogClicked_(): void {
    this.close();
  }

  private saveDialogClicked_(): void {
    this.set(
        `buttonRemappingList.${this.remappingIndex}`,
        this.getUpdatedButtonRemapping_());
    this.dispatchEvent(new CustomEvent('button-remapping-changed', {
      bubbles: true,
      composed: true,
    }));
    this.close();
  }

  /**
   * @returns Button remapping with updated remapping action based on
   * users' key combination input.
   */
  private getUpdatedButtonRemapping_(): ButtonRemapping {
    // TODO(yyhyyh@): Implement updated button remapping based on key
    // combination input.
    return this.buttonRemapping_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [KeyCombinationInputDialogElement.is]: KeyCombinationInputDialogElement;
  }
}

customElements.define(
    KeyCombinationInputDialogElement.is, KeyCombinationInputDialogElement);
