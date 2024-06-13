// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../controls/v2/settings_toggle_v2.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';

import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsToggleV2Element} from '../../controls/v2/settings_toggle_v2.js';

import {getTemplate} from './settings_toggle_v2_storybook.html.js';

export class SettingsToggleV2Storybook extends PolymerElement {
  static get is() {
    return 'settings-toggle-v2-storybook' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      checkedValue_: {
        type: Boolean,
        value: true,
      },

      basicToggleDisabled_: {
        type: Boolean,
        value: false,
      },

      prefCheckedValue_: {
        type: Boolean,
        value: true,
      },

      showBasicDialog_: {
        type: Boolean,
        value: false,
      },

      virtualManagedPref_: {
        type: Object,
        value: {
          key: 'virtual_managed_pref',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
          enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
          controlledBy: chrome.settingsPrivate.ControlledBy.DEVICE_POLICY,
        },
      },

      virtualPref_: {
        type: Boolean,
        value: {
          key: 'virtual_pref',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
        },
      },
    };
  }

  private basicToggleDisabled_: boolean;
  private checkedValue_: boolean;
  private prefCheckedValue_: boolean;
  private showBasicDialog_: boolean;
  private virtualManagedPref_: chrome.settingsPrivate.PrefObject<boolean>;
  private virtualPref_: chrome.settingsPrivate.PrefObject<boolean>;

  private onCheckedValueChange_(event: CustomEvent<boolean>): void {
    this.checkedValue_ = event.detail;
  }

  private onInvertedToggleChanged_(event: CustomEvent<boolean>): void {
    this.set('virtualPref_.value', !event.detail);
  }

  private handlePrefChange_(event: CustomEvent<{value: boolean}>): void {
    this.set('virtualPref_.value', event.detail.value);
  }

  private enableDialog_(): void {
    this.showBasicDialog_ = true;
  }

  private getToggle_(): SettingsToggleV2Element {
    const toggle = this.shadowRoot!.querySelector<SettingsToggleV2Element>(
        '#noSetPrefToggle');
    assert(toggle);
    return toggle;
  }

  private closeDialog_(): void {
    const dialog = this.shadowRoot!.querySelector('cr-dialog');
    assert(dialog);
    dialog.close();

    this.showBasicDialog_ = false;
  }

  private onCancelButtonClicked_(): void {
    this.closeDialog_();

    const toggle = this.getToggle_();
    toggle.resetToPrefValue();
  }

  private onConfirmButtonClicked_(): void {
    this.closeDialog_();

    const toggle = this.getToggle_();
    toggle.commitPrefChange();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsToggleV2Storybook.is]: SettingsToggleV2Storybook;
  }
}

customElements.define(SettingsToggleV2Storybook.is, SettingsToggleV2Storybook);
