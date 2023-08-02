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
import '../settings_shared.css.js';
import '/shared/settings/controls/settings_dropdown_menu.js';
import '../os_settings_icons.html.js';

import {DropdownMenuOptionList} from '/shared/settings/controls/settings_dropdown_menu.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './customize_button_row.html.js';
import {ButtonRemapping} from './input_device_settings_types.js';

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

      removeTopBorder: {
        type: Boolean,
        reflectToAttribute: true,
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
  private buttonRemapping_: ButtonRemapping;
  private buttonMapTargets_: DropdownMenuOptionList;
  private fakePref_: chrome.settingsPrivate.PrefObject;

  override ready() {
    super.ready();

    this.setUpButtonMapTargets_();
  }

  /**
   * Populate dropdown menu choices.
   */
  private setUpButtonMapTargets_(): void {
    // TODO(yyhyyh@): Get buttonMapTargets_ from provider in customization
    // pages, and pass it as a value instead of creating fake data here.
    this.buttonMapTargets_ = [
      {
        value: 'None',
        name: 'None',
      },
      {
        value: '0',
        name: 'Brightness Down',
      },
      {
        value: '1',
        name: 'Brightness Up',
      },
      {
        value: '2',
        name: 'Back',
      },
      {
        value: '3',
        name: 'Forward',
      },
    ];
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

    // For accelerator actions, the remappingAction.action value is number.
    // TODO(yyhyyh@): Add the case when remappingAction is none or Keyboard
    // events.
    const action = this.buttonRemapping_.remappingAction!.action;
    if (action !== undefined && !isNaN(action)) {
      this.set(
          'fakePref_.value',
          this.buttonRemapping_.remappingAction!.action!.toString());
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
