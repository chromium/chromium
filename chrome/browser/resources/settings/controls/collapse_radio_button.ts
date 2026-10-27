// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import '/shared/settings/controls/cr_policy_pref_indicator.js';

import {PrefKeyObserverMixinLit} from '/shared/settings/prefs2/pref_key_observer_mixin_lit.js';
import type {CrExpandButtonElement} from 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import {CrRadioButtonMixinLit} from 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button_mixin_lit.js';
import {CrRippleMixin} from 'chrome://resources/cr_elements/cr_ripple/cr_ripple_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './collapse_radio_button.css.js';
import {getHtml} from './collapse_radio_button.html.js';

export interface SettingsCollapseRadioButtonElement {
  $: {
    expandButton: CrExpandButtonElement,
  };
}

const SettingsCollapseRadioButtonElementBase =
    PrefKeyObserverMixinLit(CrRippleMixin(CrRadioButtonMixinLit(CrLitElement)));

export class SettingsCollapseRadioButtonElement extends
    SettingsCollapseRadioButtonElementBase {
  static get is() {
    return 'settings-collapse-radio-button';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      expanded: {
        type: Boolean,
        notify: true,
      },

      noAutomaticCollapse: {type: Boolean},

      noCollapse: {type: Boolean},

      indicatorAriaLabel: {type: String},

      icon: {type: String},

      subLabel: {type: String},

      /*
       * The aria-label attribute associated with the expand button. Used by
       * screen readers when announcing the expand button.
       */
      expandAriaLabel: {type: String},
    };
  }

  accessor expanded: boolean = false;
  accessor noAutomaticCollapse: boolean = false;
  accessor noCollapse: boolean = false;
  accessor indicatorAriaLabel: string = '';
  accessor icon: string = '';
  accessor subLabel: string = '';
  accessor expandAriaLabel: string = '';
  private pendingUpdateCollapsed_: boolean = false;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('checked')) {
      this.onCheckedChanged_();
    }

    if (changedProperties.has('pref') && this.pref) {
      this.onPrefChanged_();
    }
  }

  // Overridden from CrRippleMixin
  override createRipple() {
    this.rippleContainer = this.shadowRoot.querySelector('.disc-wrapper');
    const ripple = super.createRipple();
    ripple.setAttribute('recenters', '');
    ripple.classList.add('circle');
    return ripple;
  }

  /**
   * Updates the collapsed status of this radio button to reflect
   * the user selection actions.
   */
  async updateCollapsed() {
    await this.updateComplete;
    if (this.pendingUpdateCollapsed_) {
      this.pendingUpdateCollapsed_ = false;
      this.expanded = this.checked;
    }
  }

  getBubbleAnchor() {
    const anchor = this.shadowRoot.querySelector<HTMLElement>('#button');
    assert(anchor);
    return anchor;
  }

  private onCheckedChanged_() {
    this.pendingUpdateCollapsed_ = true;
    if (!this.noAutomaticCollapse) {
      this.updateCollapsed();
    }
  }

  private onPrefChanged_() {
    // If the preference has been set, and is managed, this control should be
    // disabled. Unless the value associated with this control is present in
    // |pref.userSelectableValues|. This will override the disabled set on the
    // element externally.
    this.disabled = !!this.pref &&
        this.pref.enforcement === chrome.settingsPrivate.Enforcement.ENFORCED &&
        !(!!this.pref.userSelectableValues &&
          this.pref.userSelectableValues.includes(this.name));
  }

  protected onExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.expanded = e.detail.value;
  }

  protected onExpandClick_() {
    this.fire('expand-clicked');
  }

  protected onRadioFocus_() {
    this.getRipple().showAndHoldDown();
  }

  /**
   * Clear the ripple associated with the radio button when the expand button
   * is focused. Stop propagation to prevent the ripple being re-created.
   */
  protected onNonRadioFocus_(e: Event) {
    this.getRipple().clear();
    e.stopPropagation();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-collapse-radio-button': SettingsCollapseRadioButtonElement;
  }
}

customElements.define(
    SettingsCollapseRadioButtonElement.is, SettingsCollapseRadioButtonElement);
