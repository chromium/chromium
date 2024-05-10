// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * `settings-toggle-v2` wraps the cr-toggle element. Works with or
 * without a pref object.
 */
import 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/ash/common/cr_elements/cros_color_overrides.css.js';

import {CrToggleElement} from 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PrefControlMixinInternal} from './pref_control_mixin_internal.js';
import {getTemplate} from './settings_toggle_v2.html.js';

export interface SettingsToggleV2Element {
  $: {
    control: CrToggleElement,
  };
}

const SettingsToggleV2ElementBase = PrefControlMixinInternal(PolymerElement);

export class SettingsToggleV2Element extends SettingsToggleV2ElementBase {
  static get is() {
    return 'settings-toggle-v2' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Used to manually set the toggle on or off.
       */
      checked: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },

      /**
       * If true, changing the control’s value will not update the pref
       * automatically. This allows the container to confirm the change first
       * then call either sendPrefChange or resetToPrefValue accordingly.
       */
      noSetPref: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },
    };
  }

  static get observers() {
    return [
      'prefChanged_(pref.*)',
    ];
  }

  checked: boolean;
  noSetPref: boolean;
  override validPrefTypes: chrome.settingsPrivate.PrefType[] = [
    chrome.settingsPrivate.PrefType.BOOLEAN,
  ];

  override ready(): void {
    super.ready();

    this.addEventListener('click', this.onClick_);
  }

  override focus(): void {
    this.$.control.focus();
  }

  /**
   * Handle downward data binding from pref to update the toggle accordingly.
   */
  private prefChanged_(): void {
    this.checked = this.pref!.value;
  }

  /**
   * Event handler for when toggle has been toggled by user action. Dispatches a
   * `change` event containing the checked value.
   */
  private onClick_(): void {
    if (this.disabled) {
      return;
    }

    this.checked = !this.checked;

    if (this.pref && !this.noSetPref) {
      this.commitPrefChange(this.checked);
    }

    this.dispatchEvent(new CustomEvent('change', {
      bubbles: true,
      composed: true,
      detail: this.checked,
    }));
  }

  /**
   * Updates the pref to the control element’s current value.
   */
  commitPrefChange(newChecked: boolean): void {
    // updatePrefValueFromUserAction() will ensure that the pref is defined
    // before committing the change.
    this.updatePrefValueFromUserAction(newChecked);
  }

  /**
   * Reset the control element’s value to match the pref’s current value.
   */
  resetToPrefValue(): void {
    assert(this.pref, 'resetToPrefValue() requires pref to be defined.');

    this.checked = this.pref.value;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsToggleV2Element.is]: SettingsToggleV2Element;
  }
}

customElements.define(SettingsToggleV2Element.is, SettingsToggleV2Element);
