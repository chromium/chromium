// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * `settings-radio-group` wraps cr-radio-group and set of radio-buttons that
 * control a supplied preference.
 *
 * Example:
 *      <settings-radio-group pref-key="settings.foo"
 *          label="Foo Options." buttons="{{fooOptionsList}}">
 *      </settings-radio-group>
 */
import '//resources/cr_elements/cr_radio_button/cr_radio_button.js';
import '//resources/cr_elements/cr_radio_group/cr_radio_group.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {prefToString, stringToPrefValue} from '/shared/settings/prefs/pref_util.js';
import {PrefKeyObserverMixinLit} from '/shared/settings/prefs2/pref_key_observer_mixin_lit.js';
import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import {assert} from 'chrome://resources/js/assert.js';

import {getCss} from './settings_radio_group.css.js';
import {getHtml} from './settings_radio_group.html.js';

const SettingsRadioGroupElementBase = PrefKeyObserverMixinLit(CrLitElement);

export class SettingsRadioGroupElement extends SettingsRadioGroupElementBase {
  static get is() {
    return 'settings-radio-group';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      groupAriaLabel: {type: String},

      /**
       * If true, do not automatically set the preference value. This allows the
       * container to confirm the change first then call either sendPrefChange
       * or resetToPrefValue accordingly.
       */
      noSetPref: {type: Boolean},

      selected: {type: String},

      selectableElements: {type: String},

      nestedSelectable: {type: Boolean},
    };
  }

  accessor groupAriaLabel: string = '';
  accessor noSetPref: boolean = false;
  accessor selected: string|undefined = undefined;
  accessor selectableElements: string =
      ['cr-radio-button', 'controlled-radio-button'].join(', ');
  accessor nestedSelectable: boolean = false;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('pref') && this.pref) {
      this.resetToPrefValue();
    }
  }

  override firstUpdated() {
    this.setAttribute('role', 'none');
  }

  override focus() {
    this.shadowRoot.querySelector('cr-radio-group')!.focus();
  }

  /** Reset the selected value to match the current pref value. */
  resetToPrefValue() {
    this.selected = prefToString(this.pref!);
  }

  /** Update the pref to the current selected value. */
  sendPrefChange() {
    assert(this.pref);
    PrefService.getInstance().setPrefValue(
        this.prefKey, stringToPrefValue(this.selected || '', this.pref));
  }

  protected onSelectedChanged_(e: CustomEvent<{value: string}>) {
    if (this.selected === e.detail.value) {
      return;
    }
    this.selected = e.detail.value;
    if (!this.noSetPref) {
      this.sendPrefChange();
    }
    this.fire('change');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-radio-group': SettingsRadioGroupElement;
  }
}

customElements.define(SettingsRadioGroupElement.is, SettingsRadioGroupElement);
