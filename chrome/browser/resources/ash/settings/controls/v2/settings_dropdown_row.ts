// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-dropdown-row' is a component which wraps a basic 'settings-row'
 * with a slotted control component `settings-dropdown-v2`. This component can
 * be used with or without prefs.
 *
 * - Usage: without pref
 *   - `value` must be provided and `pref` must not be used.
 *
 *   <settings-dropdown-row
 *       label="$i18n{rowLabel}"
 *       sublabel="$i18n{rowDescription}"
 *       icon="os-settings:display"
 *       options="[[dropdownOptions_]]"
 *       value="[[value]]"
 *       on-change="onDropdownChange_">
 *   <settings-dropdown-row>
 *
 * - Usage: with pref
 *   - `pref` must be provided and `value` must not be used.
 *
 *   <settings-dropdown-row
 *       label="$i18n{rowLabel}"
 *       sublabel="$i18n{rowDescription}"
 *       icon="os-settings:display"
 *       options="[[dropdownOptions_]]"
 *       pref="[[prefs.foo.bar]]">
 *   <settings-dropdown-row>
 */

import './settings_dropdown_v2.js';
import './settings_row.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseRowMixin} from './base_row_mixin.js';
import {PrefControlMixinInternal} from './pref_control_mixin_internal.js';
import {getTemplate} from './settings_dropdown_row.html.js';
import {DropdownOption, DropdownOptionList, SettingsDropdownV2Element} from './settings_dropdown_v2.js';
import {SettingsRowElement} from './settings_row.js';

export interface SettingsDropdownRowElement {
  $: {
    dropdown: SettingsDropdownV2Element,
    internalRow: SettingsRowElement,
  };
}

const SettingsDropdownRowElementBase =
    PrefControlMixinInternal(BaseRowMixin(PolymerElement));

export class SettingsDropdownRowElement extends SettingsDropdownRowElementBase {
  static get is() {
    return 'settings-dropdown-row' as const;
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
    };
  }

  options: DropdownOptionList;
  value?: DropdownOption['value'];

  override focus(): void {
    this.$.dropdown.focus();
  }

  /**
   * Propagates a new change event based on the change event payload dispatched
   * from the settings-dropdown-v2 element.
   */
  private propagateChangeEvent_({detail}: CustomEvent<boolean>): void {
    this.dispatchEvent(new CustomEvent('change', {
      bubbles: true,
      composed: false,  // Event should not pass the shadow DOM boundary.
      detail,
    }));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsDropdownRowElement.is]: SettingsDropdownRowElement;
  }
}

customElements.define(
    SettingsDropdownRowElement.is, SettingsDropdownRowElement);
