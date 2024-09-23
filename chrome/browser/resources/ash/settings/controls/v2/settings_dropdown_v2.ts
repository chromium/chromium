// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-dropdown-v2' is an element displaying a list of options. Works with
 * or without a pref object.
 *
 * - Usage: without pref
 *   - `value` must be provided and `pref` must not be used.
 *
 *   <settings-dropdown-v2
 *       options="[[dropdownOptions_]]"
 *       value="[[value]]"
 *       on-change="onDropdownChange_">
 *   <settings-dropdown-v2>
 *
 * - Usage: with pref
 *   - `pref` must be provided and `value` must not be used.
 *
 *   <settings-dropdown-v2
 *       options="[[dropdownOptions_]]"
 *       pref="[[prefs.foo.bar]]"
 *       on-change="onDropdownChange_">
 *   <settings-dropdown-v2>
 */

import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/ash/common/cr_elements/cros_color_overrides.css.js';
import 'chrome://resources/ash/common/cr_elements/md_select.css.js';
import 'chrome://resources/ash/common/cr_elements/policy/cr_policy_pref_indicator.js';

import {microTask, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertExists} from '../../assert_extras.js';

import {PrefControlMixinInternal} from './pref_control_mixin_internal.js';
import {getTemplate} from './settings_dropdown_v2.html.js';

/**
 * - `label` is shown in the UI.
 * - `value` is the underlying value for the option.
 * - `hidden` specifies whether to hide this option in the UI.
 */
export interface DropdownOption {
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
       * Note: This property should not be set via downward data-binding from
       * parent elements if `pref` is defined.
       * Represents the value of the dropdown. When `pref` is specified, the
       * pref object's value is synced to this property. When `pref` is not
       * specified, this property can be updated via downward data-binding.
       */
      value: {
        type: String,
        value: undefined,
      },

      // A11y properties added since they are data-bound in HTML.
      ariaLabel: {
        type: String,
        reflectToAttribute: false,
        observer: 'onAriaLabelSet_',
      },

      ariaDescription: {
        type: String,
        reflectToAttribute: false,
        observer: 'onAriaDescriptionSet_',
      },
    };
  }

  static get observers() {
    return [
      'syncPrefChangeToValue_(pref.*)',
      'setSelectedOption_(options, value)',
    ];
  }

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
    const optionIndex = this.findMatchingOptionIndex_(this.$.select.value);
    const selectedOption = this.options[optionIndex];
    assertExists(selectedOption);
    this.value = selectedOption.value;

    if (this.pref) {
      this.updatePrefValueFromUserAction(this.value);
    }

    this.dispatchEvent(new CustomEvent('change', {
      bubbles: true,
      composed: false,  // Event should not pass the shadow DOM boundary.
      detail: this.value,
    }));
  }

  /**
   * This observer watches changes to `pref` and syncs it to the `value`
   * property.
   */
  private syncPrefChangeToValue_(): void {
    if (this.pref) {
      this.value = this.pref.value;
    }
  }

  /**
   * This observer watches changes to the `options` list or `value`.
   * Programmatically sets the <select>'s value via the index of the selected
   * option. An index of -1 means that no option is selected and the <select>
   * should appear blank.
   */
  private setSelectedOption_(): void {
    const optionIndex = this.findMatchingOptionIndex_(this.value);

    // Wait for the dom-repeat to populate the <select> options before setting
    // the value.
    microTask.run(() => {
      this.$.select.selectedIndex = optionIndex;
    });
  }

  /**
   * Returns the index of an option with the same value as `value`. Returns -1
   * if no options match.
   */
  private findMatchingOptionIndex_(value: DropdownOption['value']|
                                   undefined): number {
    if (value === undefined) {
      return -1;
    }

    return this.options.findIndex((option) => {
      return option.value.toString() === value.toString();
    });
  }

  /**
   * Determines if the internal select element should be disabled. It should be
   * disabled if there are no menu items.
   */
  private isSelectDisabled_(): boolean {
    return this.disabled || this.options.length === 0;
  }

  /**
   * Manually remove the aria-label attribute from the host node since it is
   * applied to the internal select. `reflectToAttribute=false` does not resolve
   * this issue. This prevents the aria-label from being duplicated by
   * screen readers.
   */
  private onAriaLabelSet_(): void {
    const ariaLabel = this.getAttribute('aria-label');
    this.removeAttribute('aria-label');
    if (ariaLabel) {
      this.ariaLabel = ariaLabel;
    }
  }

  /**
   * Manually remove the aria-description attribute from the host node since it
   * is applied to the internal select. `reflectToAttribute=false` does not
   * resolve this issue. This prevents the aria-description from being
   * duplicated by screen readers.
   */
  private onAriaDescriptionSet_(): void {
    const ariaDescription = this.getAttribute('aria-description');
    this.removeAttribute('aria-description');
    if (ariaDescription) {
      this.ariaDescription = ariaDescription;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsDropdownV2Element.is]: SettingsDropdownV2Element;
  }
}

customElements.define(SettingsDropdownV2Element.is, SettingsDropdownV2Element);
