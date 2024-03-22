// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * `settings-radio-group` wraps cr-radio-group and set of radio-buttons that
 * control a supplied preference.
 *
 * Example:
 *      <settings-radio-group pref="{{prefs.settings.foo}}"
 *          label="Foo Options." buttons="{{fooOptionsList}}">
 *      </settings-radio-group>
 */
import '//resources/ash/common/cr_elements/cr_radio_button/cr_radio_button.js';
import '//resources/ash/common/cr_elements/cr_radio_group/cr_radio_group.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PrefControlMixin} from '/shared/settings/controls/pref_control_mixin.js';
import {prefToString, stringToPrefValue} from '/shared/settings/prefs/pref_util.js';

import {getTemplate} from './settings_radio_group.html.js';

const SettingsRadioGroupElementBase = PrefControlMixin(PolymerElement);

export class SettingsRadioGroupElement extends SettingsRadioGroupElementBase {
  static get is() {
    return 'settings-radio-group';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      disabled: {
        type: Boolean,
        value: false,
      },

      groupAriaLabel: String,

      /**
       * If true, do not automatically set the preference value. This allows the
       * container to confirm the change first then call either sendPrefChange
       * or resetToPrefValue accordingly.
       */
      noSetPref: {
        type: Boolean,
        value: false,
      },

      selected: String,

      selectableElements: {
        type: String,
        value: ['cr-radio-button', 'controlled-radio-button'].join(', '),
      },
    };
  }

  static get observers() {
    return [
      'resetToPrefValue(pref.*)',
    ];
  }

  disabled: boolean;
  groupAriaLabel: string;
  noSetPref: boolean;
  selected: string;
  selectableElements: string;

  override ready(): void {
    super.ready();

    this.setAttribute('role', 'none');
  }

  override focus(): void {
    this.shadowRoot!.querySelector('cr-radio-group')!.focus();
  }

  /** Reset the selected value to match the current pref value. */
  resetToPrefValue(): void {
    this.selected = prefToString(this.pref!);
  }

  /** Update the pref to the current selected value. */
  sendPrefChange(): void {
    if (!this.pref) {
      return;
    }
    this.set('pref.value', stringToPrefValue(this.selected, this.pref));
  }

  private onSelectedChanged_(): void {
    this.selected = this.shadowRoot!.querySelector('cr-radio-group')!.selected;
    if (!this.noSetPref) {
      this.sendPrefChange();
    }
    this.dispatchEvent(
        new CustomEvent('change', {bubbles: true, composed: true}));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-radio-group': SettingsRadioGroupElement;
  }
}

customElements.define(SettingsRadioGroupElement.is, SettingsRadioGroupElement);
