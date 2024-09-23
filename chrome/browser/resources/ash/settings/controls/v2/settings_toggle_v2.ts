// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * `settings-toggle-v2` wraps the cr-toggle element. Works with or
 * without a pref object.
 *
 * - Usage: without pref
 *   - 'checked' is false by default
 *
 *   <settings-toggle-v2
 *       checked="[[checked]]"
 *       on-change="onToggleChange_">
 *   <settings-toggle-v2>
 *
 * - Usage: with pref
 *   - 'pref' must be provided
 *
 *   <settings-toggle-v2
 *       pref="[[prefs.foo.bar]]"
 *       on-change="onToggleChange_">
 *   <settings-toggle-v2>
 *
 * - Usage: no-set-pref
 *   - 'pref' must be provided
 *   - If no-set-pref is passed, the pref value will not change based on the
 *     toggle change. The property 'checked' changes with the user's click,
 *     even when no-set-pref is true.
 *
 *     Example: A use-case is when changing the toggle triggers a dialog to
 *     open where a user must confirm or cancel the toggle change.
 *     - Invoking 'resetToPrefValue()' will change the toggle value to the
 *       pref's value.
 *     - Invoking 'commitPrefChange()' will change the pref value to the
 *       toggle's 'checked' value.
 *
 *   <settings-toggle-v2
 *       pref="[[prefs.foo.bar]]"
 *       no-set-pref
 *       on-change="openToggleDialog_">
 *   <settings-toggle-v2>
 *
 * - Usage: inverted
 *   - 'pref' must be provided
 *   - The checked value will be the opposite of the pref's value
 *
 *     Example: Suppose we have multiple functionalities, such as fA and fB,
 *     but only one of them can be enabled at any given time. fA’s value is
 *     tied to the preference prefA. We want the toggle for fB to display the
 *     inverse value of prefA. In other words, when fA is enabled, the toggle
 *     for fB will show an unchecked value (OFF), and vice versa.
 *
 *   <settings-toggle-v2
 *       pref="[[prefs.foo.bar]]"
 *       on-change="onToggleChange_"
 *       inverted>
 *   <settings-toggle-v2>
 */

import 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/ash/common/cr_elements/cros_color_overrides.css.js';
import 'chrome://resources/ash/common/cr_elements/policy/cr_policy_pref_indicator.js';

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

      /** Whether the control should represent the inverted pref value. */
      inverted: {
        type: Boolean,
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
      'setToPrefValue_(pref.*)',
    ];
  }

  checked: boolean;
  inverted: boolean;
  noSetPref: boolean;
  override validPrefTypes: chrome.settingsPrivate.PrefType[] = [
    chrome.settingsPrivate.PrefType.BOOLEAN,
  ];

  override focus(): void {
    this.$.control.focus();
  }

  /**
   * Handle downward data binding from pref to update the toggle accordingly.
   */
  private setToPrefValue_(): void {
    const currentPrefValue = this.pref!.value;
    this.checked = this.inverted ? !currentPrefValue : currentPrefValue;
  }

  /**
   * Event handler for when toggle has been toggled by user action. Dispatches a
   * `change` event containing the checked value.
   */
  private onChange_(): void {
    if (this.disabled) {
      return;
    }

    this.checked = !this.checked;

    if (this.pref && !this.noSetPref) {
      this.commitPrefChange();
    }

    this.dispatchEvent(new CustomEvent('change', {
      bubbles: true,
      composed: false,  // Event should not pass the shadow DOM boundary.
      detail: this.checked,
    }));
  }

  /**
   * Updates the pref value to the `checked` property value.
   */
  commitPrefChange(): void {
    // updatePrefValueFromUserAction() will ensure that the pref is defined
    // before committing the change.
    this.updatePrefValueFromUserAction(
        this.inverted ? !this.checked : this.checked);
  }

  /**
   * Reset the control element’s value to match the pref’s current value.
   */
  resetToPrefValue(): void {
    assert(this.pref, 'resetToPrefValue() requires pref to be defined.');

    this.setToPrefValue_();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsToggleV2Element.is]: SettingsToggleV2Element;
  }
}

customElements.define(SettingsToggleV2Element.is, SettingsToggleV2Element);
