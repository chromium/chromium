// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * `pref-toggle-button` is a toggle that controls a supplied preference.
 */
import '//resources/cr_elements/cr_actionable_row_style.css.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/cr_elements/cr_toggle/cr_toggle.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '/shared/settings/controls/cr_policy_pref_indicator.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './pref_toggle_button.html.js';

export class PrefToggleButtonElement extends PolymerElement {
  static get is() {
    return 'pref-toggle-button';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** The main label. */
      label: {
        type: String,
        value: '',
      },

      /** Additional (optional) sub-label. */
      subLabel: {
        type: String,
        value: '',
      },

      /** Whether the control is checked. */
      checked: {
        type: Boolean,
        value: false,
        notify: true,
        reflectToAttribute: true,
      },

      /**
       * Whether the control is disabled, for example due to an extension
       * managing the preference.
       */
      disabled: {
        type: Boolean,
        value: false,
      },

      /**
       * If true, do not automatically set the preference value on user click.
       * Confirm the change first then call either sendPrefChange or
       * resetToPrefValue accordingly.
       */
      changeRequiresValidation: {
        type: Boolean,
        value: false,
      },

      noExtensionIndicator: Boolean,

      pref: Object,
    };
  }

  static get observers() {
    return [
      'prefValueChanged_(pref.value)',
      'prefEnforcementChanged_(pref.enforcement)',
    ];
  }

  label: string;
  subLabel: string;
  checked: boolean;
  disabled: boolean;
  changeRequiresValidation: boolean;
  noExtensionIndicator: boolean;
  pref: chrome.settingsPrivate.PrefObject;

  override ready() {
    super.ready();

    this.addEventListener('click', this.onClick_);
  }

  /**
   * Handles non cr-toggle button clicks (cr-toggle handles its own click events
   * which don't bubble).
   */
  private onClick_(e: Event) {
    e.stopPropagation();
    if (this.disabled) {
      return;
    }

    if (this.changeRequiresValidation) {
      this.dispatchEvent(new CustomEvent(
          'validate-and-change-pref', {bubbles: true, composed: true}));
      return;
    }

    this.checked = !this.checked;
    this.updatePrefValue_();
  }

  private onToggleClick_() {
    if (this.changeRequiresValidation) {
      this.checked = !this.checked;
      this.dispatchEvent(new CustomEvent(
          'validate-and-change-pref', {bubbles: true, composed: true}));
      return;
    }
    this.updatePrefValue_();
  }

  private prefValueChanged_(prefValue: boolean) {
    this.checked = prefValue;
  }

  private prefEnforcementChanged_(enforcement:
                                      chrome.settingsPrivate.Enforcement|null) {
    this.disabled =
        (enforcement === chrome.settingsPrivate.Enforcement.ENFORCED);
    // Ensure the `cr-actionable-row-style` is informed of the state of the
    // control.
    this.toggleAttribute('effectively-disabled_', this.disabled);
  }

  /** Update the pref to the current |checked| value. */
  private updatePrefValue_() {
    this.set('pref.value', this.checked);
  }

  private getAriaLabel_(): string {
    if (!this.subLabel) {
      return this.label;
    }
    return [this.label, this.subLabel].join('. ');
  }

  private isPrefEnforced_(): boolean {
    return !!this.pref &&
        this.pref.enforcement === chrome.settingsPrivate.Enforcement.ENFORCED;
  }

  private hasPrefPolicyIndicator_(): boolean {
    if (!this.pref) {
      return false;
    }
    if (this.noExtensionIndicator &&
        this.pref.controlledBy ===
            chrome.settingsPrivate.ControlledBy.EXTENSION) {
      return false;
    }
    return this.isPrefEnforced_() ||
        chrome.settingsPrivate.Enforcement.RECOMMENDED ===
        this.pref.enforcement;
  }

  private controlDisabled_(): boolean {
    return this.disabled || this.isPrefEnforced_() ||
        !!(this.pref && this.pref.userControlDisabled);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'pref-toggle-button': PrefToggleButtonElement;
  }
}

customElements.define(PrefToggleButtonElement.is, PrefToggleButtonElement);
