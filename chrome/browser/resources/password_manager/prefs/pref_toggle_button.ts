// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * `pref-toggle-button` is a toggle that controls a supplied preference.
 */
import '//resources/cr_elements/cr_toggle/cr_toggle.js';
import '/shared/settings/controls/cr_policy_pref_indicator.js';

import {PrefKeyObserverMixinLit} from '/shared/settings/prefs2/pref_key_observer_mixin_lit.js';
import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './pref_toggle_button.css.js';
import {getHtml} from './pref_toggle_button.html.js';

const PrefToggleButtonElementBase = PrefKeyObserverMixinLit(CrLitElement);

export class PrefToggleButtonElement extends PrefToggleButtonElementBase {
  static get is() {
    return 'pref-toggle-button';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      ariaLabel: {type: String},

      /** The main label. */
      label: {type: String},

      /** Additional (optional) sub-label. */
      subLabel: {type: String},

      /** Additional (optional) sub-label that has a link. */
      subLabelWithLink: {type: String},

      /** Whether the control is checked. */
      checked: {
        type: Boolean,
        notify: true,
        reflect: true,
      },

      /**
       * Whether the control is disabled, for example due to an extension
       * managing the preference.
       */
      disabled: {type: Boolean},

      /**
       * If true, do not automatically set the preference value on user click.
       * Confirm the change first then call either sendPrefChange or
       * resetToPrefValue accordingly.
       */
      changeRequiresValidation: {type: Boolean},

      noExtensionIndicator: {type: Boolean},
    };
  }

  override accessor ariaLabel: string = '';
  accessor label: string = '';
  accessor subLabel: string = '';
  accessor subLabelWithLink: string = '';
  accessor checked: boolean = false;
  accessor disabled: boolean = false;
  accessor changeRequiresValidation: boolean = false;
  accessor noExtensionIndicator: boolean = false;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('ariaLabel')) {
      this.onAriaLabelSet_();
    }

    if (changedProperties.has('pref') && this.pref) {
      this.checked = this.pref.value;
      this.disabled =
          this.pref.enforcement === chrome.settingsPrivate.Enforcement.ENFORCED;
    }
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    this.addEventListener('click', this.onClick_);
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('disabled')) {
      // Ensure the `cr-actionable-row-style` is informed of the state of the
      // control.
      this.toggleAttribute('effectively-disabled_', this.disabled);
    }
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
      this.fire('validate-and-change-pref');
      return;
    }

    this.checked = !this.checked;
    this.updatePrefValue_();
  }

  protected onSubLabelTextWithLinkClick_(e: Event) {
    const anchor = (e.target as HTMLElement).closest('a');
    if (anchor) {
      e.preventDefault();
      e.stopPropagation();
      if (anchor.href) {
        OpenWindowProxyImpl.getInstance().openUrl(anchor.href);
      }
    }
  }

  protected hasSubLabel_(): boolean {
    return !!this.subLabel || !!this.subLabelWithLink;
  }

  protected getSubLabelWithLinkContent_(): TrustedHTML {
    return sanitizeInnerHtml(this.subLabelWithLink, {
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

  protected onCheckedChanged_(e: CustomEvent<{value: boolean}>) {
    this.checked = e.detail.value;
  }

  protected onToggleChange_() {
    if (this.changeRequiresValidation) {
      this.checked = !this.checked;
      this.fire('validate-and-change-pref');
      return;
    }
    this.updatePrefValue_();
  }

  /** Update the pref to the current |checked| value. */
  private updatePrefValue_() {
    PrefService.getInstance().setPrefValue(this.prefKey, this.checked);
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

  protected getAriaLabel_(): string {
    if (this.ariaLabel) {
      return this.ariaLabel;
    }
    if (!this.subLabel) {
      return this.label;
    }
    return [this.label, this.subLabel].join('. ');
  }

  private isPrefEnforced_(): boolean {
    return !!this.pref &&
        this.pref.enforcement === chrome.settingsPrivate.Enforcement.ENFORCED;
  }

  protected hasPrefPolicyIndicator_(): boolean {
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

  protected controlDisabled_(): boolean {
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
