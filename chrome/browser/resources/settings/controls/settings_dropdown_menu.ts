// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * 'settings-dropdown-menu' is a control for displaying options
 * in the settings.
 *
 * Example:
 *
 *   <settings-dropdown-menu pref-key="foo.bar">
 *   </settings-dropdown-menu>
 */
import '//resources/cr_elements/cr_shared_vars.css.js';
import '/shared/settings/controls/cr_policy_pref_indicator.js';

import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrPolicyPrefMixinLit} from '/shared/settings/controls/cr_policy_pref_mixin_lit.js';
import {prefToString, stringToPrefValue} from '/shared/settings/prefs/pref_util.js';
import {PrefService} from '/shared/settings/prefs2/pref_service.js';

import {PrefKeyObserverMixinLit} from './pref_key_observer_mixin_lit.js';
import {getCss} from './settings_dropdown_menu.css.js';
import {getHtml} from './settings_dropdown_menu.html.js';

/**
 * |name| is shown in the UI. |value| is used to set or compare with the
 * preference value. |hidden| specifies whether to hide this option from the
 * user.
 */
interface DropdownMenuOption {
  name: string;
  value: number|string;
  hidden?: boolean;
  searchHint?: string;
}

export type DropdownMenuOptionList = DropdownMenuOption[];

export interface SettingsDropdownMenuElement {
  $: {
    dropdownMenu: HTMLSelectElement,
  };
}

const SettingsDropdownMenuElementBase =
    CrPolicyPrefMixinLit(PrefKeyObserverMixinLit(CrLitElement));

export class SettingsDropdownMenuElement extends
    SettingsDropdownMenuElementBase {
  static get is() {
    return 'settings-dropdown-menu';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /** List of options for the drop-down menu. */
      menuOptions: {type: Array},

      /** Whether the dropdown menu should be disabled. */
      disabled: {
        type: Boolean,
        reflect: true,
      },

      pref: {type: Object},

      /**
       * If true, do not automatically set the preference value. This allows the
       * container to confirm the change first then call either sendPrefChange
       * or resetToPrefValue accordingly.
       */
      noSetPref: {type: Boolean},

      notFoundValue: {type: String},

      /** Label for a11y purposes */
      label: {type: String},

      /** The value of the element if not using |pref|. */
      value: {type: String},
    };
  }


  accessor menuOptions: DropdownMenuOption[] = [];
  accessor disabled: boolean = false;
  override accessor pref: chrome.settingsPrivate.PrefObject|undefined =
      undefined;
  accessor noSetPref: boolean = false;
  accessor notFoundValue: string = 'SETTINGS_DROPDOWN_NOT_FOUND_ITEM';
  accessor label: string = '';
  accessor value: string|undefined = undefined;

  override updated(changedProperties: PropertyValues) {
    super.updated(changedProperties as PropertyValues<this>);

    if (changedProperties.has('menuOptions') ||
        changedProperties.has('value') || changedProperties.has('pref')) {
      this.updateSelected_();
    }
  }

  override focus() {
    this.$.dropdownMenu.focus();
  }

  /** Update the pref to the current selected value. */
  sendPrefChange() {
    assert(this.pref);

    const selected = this.$.dropdownMenu.value;
    const prefValue = stringToPrefValue(selected, this.pref);
    if (prefValue !== undefined) {
      PrefService.getInstance().setPrefValue(this.prefKey, prefValue);
    }
  }

  /**
   * Allow access to the selected value without having to go through the shadow
   * dom.
   */
  getSelectedValue(): string {
    return this.$.dropdownMenu.value;
  }

  /**
   * Pass the selection change to the pref value.
   */
  protected onChange_() {
    if (this.$.dropdownMenu.value === this.notFoundValue) {
      return;
    }

    if (!this.noSetPref && this.prefKey) {
      this.sendPrefChange();
    }

    // settings-control-change only fires when the selection is changed to
    // a valid property.
    this.fire('settings-control-change');
  }

  /**
   * Updates the selected item when the pref or menuOptions change.
   */
  private updateSelected_() {
    if (!this.menuOptions.length) {
      return;
    }

    let prefValue: string;
    if (this.value !== undefined) {
      prefValue = this.value;
    } else {
      if (this.pref === undefined) {
        return;
      }
      prefValue = this.prefStringValue_();
    }

    const option = this.menuOptions.find(function(menuItem) {
      return menuItem.value.toString() === prefValue;
    });

    this.$.dropdownMenu.value =
        option === undefined ? this.notFoundValue : prefValue;
  }

  /**
   * Gets the current value of the preference as a string.
   */
  private prefStringValue_(): string {
    assert(this.pref);
    return prefToString(this.pref);
  }

  protected showNotFoundValue_(): boolean {
    if (this.pref === undefined) {
      return false;
    }

    // Don't show "Custom" before the options load.
    if (this.menuOptions.length === 0) {
      return false;
    }

    const option = this.menuOptions.find((menuItem) => {
      return menuItem.value.toString() === this.prefStringValue_();
    });
    return !option;
  }

  protected shouldDisableMenu_(): boolean {
    return this.disabled || this.isPrefEnforced() ||
        this.menuOptions.length === 0;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-dropdown-menu': SettingsDropdownMenuElement;
  }
}

customElements.define(
    SettingsDropdownMenuElement.is, SettingsDropdownMenuElement);
