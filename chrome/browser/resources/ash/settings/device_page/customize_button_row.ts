// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'keyboard-remap-key-row' contains a key with icon label and dropdown menu to
 * allow users to customize the remapped key.
 */

import 'chrome://resources/cr_components/settings_prefs/prefs.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/cr_elements/md_select.css.js';
import '../settings_shared.css.js';
import '/shared/settings/controls/settings_dropdown_menu.js';
import '../os_settings_icons.html.js';

import {DropdownMenuOptionList} from '/shared/settings/controls/settings_dropdown_menu.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {microTask, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {cast} from '../assert_extras.js';

import {getTemplate} from './customize_button_row.html.js';
import {ActionChoice, ButtonRemapping} from './input_device_settings_types.js';

const NO_REMAPPING_OPTION_LABEL = 'none';
const KEY_COMBINATION_OPTION_LABEL = 'key combination';

const CustomizeButtonRowElementBase = I18nMixin(PolymerElement);

export class CustomizeButtonRowElement extends CustomizeButtonRowElementBase {
  static get is() {
    return 'customize-button-row' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      buttonRemappingList: {
        type: Array,
      },

      buttonRemapping_: {
        type: Object,
      },

      buttonMapTargets_: {
        type: Object,
      },

      remappingIndex: {
        type: Number,
      },

      fakePref_: {
        type: Object,
        value() {
          return {
            key: 'fakeCustomizeKeyPref',
            type: chrome.settingsPrivate.PrefType.STRING,
            value: 0,
          };
        },
      },

      actionList: {
        type: Array,
        observer: 'setUpButtonMapTargets_',
      },

      removeTopBorder: {
        type: Boolean,
        reflectToAttribute: true,
      },

      /**
       * The value of the "None" item.
       */
      noRemappingOptionValue_: {
        type: String,
        value: NO_REMAPPING_OPTION_LABEL,
        readOnly: true,
      },

      /**
       * The value of the "Key combination" item.
       */
      keyCombinationOptionValue_: {
        type: String,
        value: KEY_COMBINATION_OPTION_LABEL,
        readOnly: true,
      },
    };
  }

  static get observers(): string[] {
    return [
      'onSettingsChanged(fakePref_.*)',
      'initializeCustomizeKey(buttonRemappingList, remappingIndex)',
    ];
  }

  buttonRemappingList: ButtonRemapping[];
  remappingIndex: number;
  actionList: ActionChoice[];
  private buttonRemapping_: ButtonRemapping;
  private buttonMapTargets_: DropdownMenuOptionList;
  private fakePref_: chrome.settingsPrivate.PrefObject;
  private noRemappingOptionValue_: string;
  private keyCombinationOptionValue_: string;

  /**
   * Populate dropdown menu choices.
   */
  private setUpButtonMapTargets_(): void {
    this.buttonMapTargets_ = [];
    if (!this.actionList) {
      return;
    }
    // TODO(yyhyyh@): Get buttonMapTargets_ from provider in customization
    // pages, and pass it as a value instead of creating fake data here.
    for (const actionChoice of this.actionList) {
      this.buttonMapTargets_.push({
        value: actionChoice.actionId.toString(),
        name: actionChoice.name,
      });
    }
  }

  /**
   * Initialize the button remapping content and set up fake pref.
   */
  private initializeCustomizeKey(): void {
    if (!this.buttonRemappingList ||
        !this.buttonRemappingList[this.remappingIndex]) {
      return;
    }
    this.buttonRemapping_ = this.buttonRemappingList[this.remappingIndex];
    this.setUpButtonMapTargets_();

    // For accelerator actions, the remappingAction.action value is number.
    // TODO(yyhyyh@): Add the case when remappingAction is none or Keyboard
    // events.
    const action = this.buttonRemapping_.remappingAction!.action;
    if (action !== undefined && !isNaN(action)) {
      const originalAction =
          this.buttonRemapping_.remappingAction!.action!.toString();
      const dropdown = cast(
          this.shadowRoot!.querySelector('#remappingActionDropdown'),
          HTMLSelectElement);

      // Initialize fakePref with the tablet settings mapping.
      this.set('fakePref_.value', originalAction);

      // Initialize dropdown menu selection to match the tablet settings.
      const option = this.buttonMapTargets_.find((dropdownItem) => {
        return dropdownItem.value === originalAction;
      });

      microTask.run(() => {
        dropdown.value =
            option === undefined ? NO_REMAPPING_OPTION_LABEL : originalAction;
      });
    }
  }

  /**
   * Update device settings whenever the pref changes.
   */
  private onSettingsChanged(): void {
    // TODO(yyhyyh@): Update remapping settings.
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [CustomizeButtonRowElement.is]: CustomizeButtonRowElement;
  }
}

customElements.define(CustomizeButtonRowElement.is, CustomizeButtonRowElement);
