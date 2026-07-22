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
import '/shared/settings/controls/cr_policy_pref_indicator.js';

import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
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
      ariaLabel: {
        type: String,
        reflectToAttribute: false,  // Handled by #control.
        observer: 'onAriaLabelSet_',
        value: '',
      },

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

      /** Additional (optional) sub-label that has a link. */
      subLabelWithLink: {
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

  declare ariaLabel: string;
  declare label: string;
  declare subLabel: string;
  declare subLabelWithLink: string;
  declare checked: boolean;
  declare disabled: boolean;
  declare changeRequiresValidation: boolean;
  declare noExtensionIndicator: boolean;
  declare pref: chrome.settingsPrivate.PrefObject;

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

  private onSubLabelTextWithLinkClick_(e: Event) {
    const anchor = (e.target as HTMLElement).closest('a');
    if (anchor) {
      e.preventDefault();
      e.stopPropagation();
      if (anchor.href) {
        OpenWindowProxyImpl.getInstance().openUrl(anchor.href);
      }
    }
  }

  private hasSubLabel_(subLabel: string, subLabelWithLink: string): boolean {
    return !!subLabel || !!subLabelWithLink;
  }

  private getSubLabelWithLinkContent_(subLabelWithLink: string): TrustedHTML {
    return sanitizeInnerHtml(subLabelWithLink, {
      attrs: [
        'id',
        'is',
        'aria-description',
        'aria-hidden',
        'aria-label',
        'aria-labelledby',
        'tabindex',
      ],
    });
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

  /**
   * Removes the aria-label attribute if it's added by $i18n{...}.
   */
  private onAriaLabelSet_() {
    if (this.hasAttribute('aria-label')) {
      const ariaLabel = this.ariaLabel;
      this.removeAttribute('aria-label');
      this.ariaLabel = ariaLabel;
    }
  }

  private getAriaLabel_(label: string, subLabel: string, ariaLabel: string):
      string {
    if (ariaLabel) {
      return ariaLabel;
    }
    if (!subLabel) {
      return label;
    }
    return [label, subLabel].join('. ');
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
