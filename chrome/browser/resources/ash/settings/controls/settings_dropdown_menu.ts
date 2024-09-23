// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * 'settings-dropdown-menu' is a control for displaying options
 * in the settings.
 *
 * Example:
 *
 *   <settings-dropdown-menu pref="{{prefs.foo}}">
 *   </settings-dropdown-menu>
 */
import '//resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '//resources/ash/common/cr_elements/md_select.css.js';
import '//resources/ash/common/cr_elements/policy/cr_policy_pref_indicator.js';
import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';

import {assert} from '//resources/js/assert.js';
import {microTask, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CrPolicyPrefMixin} from '/shared/settings/controls/cr_policy_pref_mixin.js';
import {PrefControlMixin} from '/shared/settings/controls/pref_control_mixin.js';
import {prefToString, stringToPrefValue} from '/shared/settings/prefs/pref_util.js';

import {getTemplate} from './settings_dropdown_menu.html.js';

/**
 * |name| is shown in the UI. |value| is used to set or compare with the
 * preference value. |hidden| specifies whether to hide this option from the
 * user.
 */
interface DropdownMenuOption {
  name: string;
  value: number|string;
  hidden?: boolean;
}

export type DropdownMenuOptionList = DropdownMenuOption[];

export interface SettingsDropdownMenuElement {
  $: {
    dropdownMenu: HTMLSelectElement,
  };
}

const SettingsDropdownMenuElementBase =
    CrPolicyPrefMixin(PrefControlMixin(PolymerElement));

export class SettingsDropdownMenuElement extends
    SettingsDropdownMenuElementBase {
  static get is() {
    return 'settings-dropdown-menu';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * List of options for the drop-down menu.
       */
      menuOptions: Array,

      /** Whether the dropdown menu should be disabled. */
      disabled: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },

      /**
         If this is a dictionary pref, this is the key for the item
          we are interested in.
       */
      prefKey: {
        type: String,
        value: null,
      },

      /**
       * The value of the "custom" item.
       */
      notFoundValue: {
        type: String,
        value: 'SETTINGS_DROPDOWN_NOT_FOUND_ITEM',
        readOnly: true,
      },

      /** Label for a11y purposes */
      label: String,
    };
  }

  static get observers() {
    return [
      'updateSelected_(menuOptions, pref.value.*, prefKey)',
    ];
  }

  menuOptions: DropdownMenuOptionList;
  disabled: boolean;
  prefKey: string|null;
  notFoundValue: string;
  label: string;

  override focus(): void {
    this.$.dropdownMenu.focus();
  }

  /**
   * Pass the selection change to the pref value.
   */
  private onChange_(): void {
    const selected = this.$.dropdownMenu.value;

    if (selected === this.notFoundValue) {
      return;
    }

    assert(this.pref);
    if (this.prefKey) {
      this.set(`pref.value.${this.prefKey}`, selected);
    } else {
      const prefValue = stringToPrefValue(selected, this.pref);
      if (prefValue !== undefined) {
        this.set('pref.value', prefValue);
      }
    }

    // settings-control-change only fires when the selection is changed to
    // a valid property.
    this.dispatchEvent(new CustomEvent(
        'settings-control-change', {bubbles: true, composed: true}));
  }

  /**
   * Updates the selected item when the pref or menuOptions change.
   */
  private updateSelected_(): void {
    if (this.menuOptions === undefined || this.pref === undefined ||
        this.prefKey === undefined) {
      return;
    }

    if (!this.menuOptions.length) {
      return;
    }

    const prefValue = this.prefStringValue_();
    const option = this.menuOptions.find(menuItem => {
      return menuItem.value.toString() === prefValue;
    });

    // Wait for the dom-repeat to populate the <select> before setting
    // <select>#value so the correct option gets selected.
    microTask.run(() => {
      this.$.dropdownMenu.value =
          option === undefined ? this.notFoundValue : prefValue;
    });
  }

  /**
   * Gets the current value of the preference as a string.
   */
  private prefStringValue_(): string {
    if (this.prefKey) {
      // Dictionary pref, values are always strings.
      return this.pref!.value[this.prefKey];
    } else {
      assert(this.pref);
      return prefToString(this.pref);
    }
  }

  private showNotFoundValue_(
      menuOptions: DropdownMenuOptionList|null|undefined,
      prefValue: string): boolean {
    if (menuOptions === undefined || prefValue === undefined) {
      return false;
    }

    // Don't show "Custom" before the options load.
    if (menuOptions === null || menuOptions.length === 0) {
      return false;
    }

    const option = menuOptions.find((menuItem) => {
      return menuItem.value.toString() === this.prefStringValue_();
    });
    return !option;
  }

  private shouldDisableMenu_(): boolean {
    return this.disabled || this.isPrefEnforced() ||
        this.menuOptions === undefined || this.menuOptions.length === 0;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-dropdown-menu': SettingsDropdownMenuElement;
  }
}

customElements.define(
    SettingsDropdownMenuElement.is, SettingsDropdownMenuElement);
