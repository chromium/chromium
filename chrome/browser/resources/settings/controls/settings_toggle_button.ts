// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * `settings-toggle-button` is a toggle that controls a supplied preference.
 */
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/cr_elements/action_link.css.js';
import '//resources/cr_elements/cr_toggle/cr_toggle.js';
import '/shared/settings/controls/cr_policy_pref_indicator.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';

import type {CrToggleElement} from '//resources/cr_elements/cr_toggle/cr_toggle.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {SettingsBooleanControlMixin} from '/shared/settings/controls/settings_boolean_control_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';

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

      ariaShowLabel: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },

      ariaShowSublabel: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },

      elideLabel: {
        type: Boolean,
        reflectToAttribute: true,
      },

      learnMoreUrl: {
        type: String,
        reflectToAttribute: true,
      },

      subLabelWithLink: {
        type: String,
        reflectToAttribute: true,
      },

      learnMoreAriaLabel: {
        type: String,
        value: '',
      },

      icon: String,

      subLabelIcon: String,
    };
  }

  static get observers() {
    return [
      'onDisableOrPrefChange_(disabled, pref.*)',
    ];
  }

  override ariaLabel: string;
  ariaShowLabel: boolean;
  ariaShowSublabel: boolean;
  elideLabel: boolean;
  icon: string;
  learnMoreAriaLabel: string;
  learnMoreUrl: string;
  subLabelWithLink: string;
  subLabelIcon: string;

  override ready() {
    super.ready();

    this.addEventListener('click', this.onHostClick_);
  }

  private fire_(eventName: string, detail?: any) {
    this.dispatchEvent(
        new CustomEvent(eventName, {detail, bubbles: true, composed: true}));
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

  private getLearnMoreAriaLabelledBy_(): string {
    return this.learnMoreAriaLabel ? 'learn-more-aria-label' :
                                     'sub-label-text learn-more';
  }

  getBubbleAnchor() {
    const anchor = this.shadowRoot!.querySelector<HTMLElement>('#control');
    assert(anchor);
    return anchor;
  }

  private onDisableOrPrefChange_() {
    this.toggleAttribute('effectively-disabled_', this.controlDisabled());
  }

  /**
   * Handles non cr-toggle button clicks (cr-toggle handles its own click events
   * which don't bubble).
   */
  private onHostClick_(e: Event) {
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

  /**
   * Set up the contents of sub label with link.
   */
  private getSubLabelWithLinkContent_(contents: string) {
    return sanitizeInnerHtml(contents, {
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

  private onSubLabelTextWithLinkClick_(e: Event) {
    const target = e.target as HTMLElement;
    if (target.tagName === 'A') {
      this.fire_('sub-label-link-clicked', target.id);
      e.preventDefault();
      e.stopPropagation();
    }
  }

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
