// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * `settings-toggle-button` is a toggle that controls a supplied preference.
 */
import '//resources/cr_elements/cr_toggle/cr_toggle.js';
import '/shared/settings/controls/cr_policy_pref_indicator.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';

import type {CrToggleElement} from '//resources/cr_elements/cr_toggle/cr_toggle.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {assert} from 'chrome://resources/js/assert.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';

import {SettingsBooleanControlMixin} from './settings_boolean_control_mixin.js';
import {getCss} from './settings_toggle_button.css.js';
import {getHtml} from './settings_toggle_button.html.js';


export interface SettingsToggleButtonElement {
  $: {
    control: CrToggleElement,
    labelWrapper: HTMLElement,
  };
}

const SettingsToggleButtonElementBase =
    SettingsBooleanControlMixin(CrLitElement);

export class SettingsToggleButtonElement extends
    SettingsToggleButtonElementBase {
  static get is() {
    return 'settings-toggle-button';
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
      ariaShowLabel: {type: Boolean},
      ariaShowSublabel: {type: Boolean},

      elideLabel: {
        type: Boolean,
        reflect: true,
      },

      learnMoreUrl: {type: String},
      subLabelWithLink: {type: String},
      learnMoreAriaLabel: {type: String},
      icon: {type: String},
      subLabelIcon: {type: String},

      /**
       * If true, the host element does not get a click event handler and the
       * client is responsible for determining their own click logic. Thus when
       * true, clicking on the setting row does not toggle the setting pref.
       * Note, this boolean is only used on firstUpdated() callback, and any
       * changes after that have no effect.
       */
      noToggleOnHostClick: {type: Boolean},
    };
  }

  override accessor ariaLabel: string = '';
  accessor ariaShowLabel: boolean = false;
  accessor ariaShowSublabel: boolean = false;
  accessor elideLabel: boolean = false;
  accessor icon: string = '';
  accessor learnMoreAriaLabel: string = '';
  accessor learnMoreUrl: string = '';
  accessor subLabelWithLink: string = '';
  accessor subLabelIcon: string = '';
  accessor noToggleOnHostClick: boolean = false;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('ariaLabel')) {
      this.onAriaLabelSet_();
    }
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);

    // If the settings toggle is noToggleOnHostClick then do not update the
    // setting pref on click. Instead let parent code use a custom click
    // handler as needed.
    if (!this.noToggleOnHostClick) {
      this.addEventListener('click', this.onHostClick_);
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('disabled') || changedProperties.has('pref')) {
      this.toggleAttribute('effectively-disabled_', this.controlDisabled());
    }
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

  protected getAriaLabel_(): string {
    return this.ariaLabel || this.label;
  }

  protected getLearnMoreAriaLabelledBy_(): string {
    return this.learnMoreAriaLabel ? 'learn-more-aria-label' :
                                     'sub-label-text learn-more';
  }

  getBubbleAnchor(): HTMLElement {
    return this.$.control;
  }

  /**
   * Handles non cr-toggle button clicks (cr-toggle handles its own click events
   * which don't bubble).
   */
  private onHostClick_(e: Event) {
    assert(!this.noToggleOnHostClick);
    e.stopPropagation();
    if (this.controlDisabled()) {
      return;
    }
    this.updateCheckedAndNotify_(!this.checked);
  }

  protected onLearnMoreClick_(e: Event) {
    e.stopPropagation();
    this.fire('learn-more-clicked');
  }

  /**
   * Set up the contents of sub label with link.
   */
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

  protected onSubLabelTextWithLinkClick_(e: Event) {
    const target = e.target as HTMLElement;
    if (target.tagName === 'A') {
      this.fire('sub-label-link-clicked', target.id);
      e.preventDefault();
      e.stopPropagation();
    }
  }

  protected onChange_(e: CustomEvent<boolean>) {
    // Prevent cr-toggle's change event from propagating to the parent since
    // this element fires its own 'change' event.
    e.stopPropagation();
    this.updateCheckedAndNotify_(e.detail);
  }

  private updateCheckedAndNotify_(checked: boolean) {
    this.checked = checked;
    this.notifyChangedByUserInteraction();
    this.fire('change', this.checked);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-toggle-button': SettingsToggleButtonElement;
  }
}

customElements.define(
    SettingsToggleButtonElement.is, SettingsToggleButtonElement);
