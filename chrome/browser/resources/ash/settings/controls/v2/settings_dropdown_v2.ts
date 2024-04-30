// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-dropdown-v2' is an element displaying a list of options. Works with
 * or without a pref object.
 */

import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/ash/common/cr_elements/cros_color_overrides.css.js';
import 'chrome://resources/ash/common/cr_elements/md_select.css.js';
import 'chrome://resources/ash/common/cr_elements/policy/cr_policy_pref_indicator.js';

import {prefToString as prefValueToString} from '/shared/settings/prefs/pref_util.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertExists} from '../../assert_extras.js';

import {PrefControlMixinInternal} from './pref_control_mixin_internal.js';
import {getTemplate} from './settings_dropdown_v2.html.js';

/**
 * - `label` is shown in the UI.
 * - `value` is the underlying value for the option.
 * - `hidden` specifies whether to hide this option in the UI.
 */
interface DropdownOption {
  label: string;
  value: number|string;
  hidden?: boolean;
}

export type DropdownOptionList = DropdownOption[];

export interface SettingsDropdownV2Element {
  $: {
    select: HTMLSelectElement,
  };
}

const SettingsDropdownV2ElementBase = PrefControlMixinInternal(PolymerElement);

export class SettingsDropdownV2Element extends SettingsDropdownV2ElementBase {
  static get is() {
    return 'settings-dropdown-v2' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * List of options for the dropdown.
       */
      options: {
        type: Array,
        value: () => {
          return [];
        },
      },

      /**
       * Note: This property should not be set if `pref` is defined.
       * When this component is used without a pref object, `value` represents
       * the current value of the dropdown. Setting `value` from parent elements
       * via downward data binding will update the selected option accordingly.
       */
      value: {
        type: String,
      },

      /**
       * Label for a11y purposes.
       */
      ariaLabel: {
        type: String,
      },

      /**
       * The value of the "not found" dropdown option. This option and value are
       * used when there is no matching option for the dropdown's value. In this
       * case, the selected value will appear blank in the UI.
       */
      notFoundValue: {
        type: String,
        value: 'SETTINGS_DROPDOWN_NOT_FOUND',
        readOnly: true,
      },

      /**
       * Determines if the "not found" option is selected or not.
       */
      isNotFoundOptionSelected_: {
        type: Boolean,
        computed: 'computeIsNotFoundOptionSelected_(options, pref.*, value)',
      },
    };
  }

  readonly notFoundValue: string;
  options: DropdownOptionList;
  override validPrefTypes: chrome.settingsPrivate.PrefType[] = [
    chrome.settingsPrivate.PrefType.NUMBER,
    chrome.settingsPrivate.PrefType.STRING,
  ];
  value?: DropdownOption['value'];

  override focus(): void {
    this.$.select.focus();
  }

  /**
   * Event handler for when a menu item is selected by user action. Dispatches a
   * `change` event containing the selected value.
   */
  private onChange_(): void {
    const selectedOption = this.findMatchingOption_(this.$.select.value);
    assertExists(selectedOption);
    const newValue = selectedOption.value;

    if (this.pref) {
      this.updatePrefValueFromUserAction(newValue);
    } else {
      this.value = newValue;
    }

    this.dispatchEvent(new CustomEvent(
        'change', {bubbles: true, composed: true, detail: newValue}));
  }

  /**
   * Returns a matching option from `options` based on the given `value`. Else,
   * returns `undefined` if no matching option.
   */
  private findMatchingOption_(value: string): DropdownOption|undefined {
    return this.options.find(option => option.value.toString() === value);
  }

  /**
   * Determines if the internal select element should be disabled. It should be
   * disabled if there are no menu items.
   */
  private isSelectDisabled_(): boolean {
    return this.disabled || this.options.length === 0;
  }

  /**
   * Determines if the given `option` is selected based on its value.
   */
  private isOptionSelected_(option: DropdownOption): boolean {
    if (this.pref) {
      return prefValueToString(this.pref) === option.value.toString();
    }

    if (this.value !== undefined) {
      return this.value.toString() === option.value.toString();
    }

    return false;
  }

  /**
   * Computes if the "not found" option is selected. This option should be
   * selected only if no other options are selected.
   */
  private computeIsNotFoundOptionSelected_(): boolean {
    return !this.options.some((option) => this.isOptionSelected_(option));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsDropdownV2Element.is]: SettingsDropdownV2Element;
  }
}

customElements.define(SettingsDropdownV2Element.is, SettingsDropdownV2Element);
