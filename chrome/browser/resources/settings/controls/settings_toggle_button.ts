// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * `settings-toggle-button` is a toggle that controls a supplied preference.
 */
import '//resources/cr_elements/shared_vars_css.m.js';
import '//resources/cr_elements/cr_toggle/cr_toggle.m.js';
import '//resources/cr_elements/policy/cr_policy_pref_indicator.m.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../settings_shared.css.js';

import {CrToggleElement} from '//resources/cr_elements/cr_toggle/cr_toggle.m.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// <if expr="chromeos_ash">
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.m.js';

// </if>

import {SettingsBooleanControlMixin} from './settings_boolean_control_mixin.js';
import {getTemplate} from './settings_toggle_button.html.js';


export interface SettingsToggleButtonElement {
  $: {
    control: CrToggleElement,
    labelWrapper: HTMLElement,
  };
}

const SettingsToggleButtonElementBase =
    SettingsBooleanControlMixin(PolymerElement);

export class SettingsToggleButtonElement extends
    SettingsToggleButtonElementBase {
  static get is() {
    return 'settings-toggle-button';
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

      elideLabel: {
        type: Boolean,
        reflectToAttribute: true,
      },

      learnMoreUrl: {
        type: String,
        reflectToAttribute: true,
      },

      // <if expr="chromeos_ash">
      subLabelWithLink: {
        type: String,
        reflectToAttribute: true,
      },
      // </if>

      subLabelIcon: String,
    };
  }

  static get observers() {
    return [
      'onDisableOrPrefChange_(disabled, pref.*)',
    ];
  }

  override ariaLabel: string;
  elideLabel: boolean;
  learnMoreUrl: string;

  // <if expr="chromeos_ash">
  subLabelWithLink: string;
  // </if>

  subLabelIcon: string;

  override ready() {
    super.ready();

    this.addEventListener('click', this.onHostTap_);
  }

  private fire_(eventName: string) {
    this.dispatchEvent(
        new CustomEvent(eventName, {bubbles: true, composed: true}));
  }

  override focus() {
    this.$.control.focus();
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

  private getAriaLabel_(): string {
    return this.ariaLabel || this.label;
  }

  private onDisableOrPrefChange_() {
    this.toggleAttribute('effectively-disabled_', this.controlDisabled());
  }

  /**
   * Handles non cr-toggle button clicks (cr-toggle handles its own click events
   * which don't bubble).
   */
  private onHostTap_(e: Event) {
    e.stopPropagation();
    if (this.controlDisabled()) {
      return;
    }

    this.checked = !this.checked;
    this.notifyChangedByUserInteraction();
    this.fire_('change');
  }

  private onLearnMoreClick_(e: CustomEvent<boolean>) {
    e.stopPropagation();
    this.fire_('learn-more-clicked');
  }

  // <if expr="chromeos_ash">
  /**
   * Set up the contents of sub label with link.
   */
  private getSubLabelWithLinkContent_(contents: string) {
    return sanitizeInnerHtml(
        contents,
        {attrs: ['id', 'aria-hidden', 'aria-labelledby', 'tabindex']});
  }

  private onSubLabelTextWithLinkClick_(e: Event) {
    if ((e.target as HTMLElement).tagName === 'A') {
      this.fire_('sub-label-link-clicked');
      e.preventDefault();
      e.stopPropagation();
    }
  }
  // </if>

  private onChange_(e: CustomEvent<boolean>) {
    this.checked = e.detail;
    this.notifyChangedByUserInteraction();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-toggle-button': SettingsToggleButtonElement;
  }
}

customElements.define(
    SettingsToggleButtonElement.is, SettingsToggleButtonElement);
