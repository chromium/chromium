// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/shared/settings/controls/cr_policy_pref_indicator.js';

import {CrRadioButtonMixinLit} from '//resources/cr_elements/cr_radio_button/cr_radio_button_mixin_lit.js';
import {CrRippleMixin} from '//resources/cr_elements/cr_ripple/cr_ripple_mixin.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {prefToString} from '/shared/settings/prefs/pref_util.js';

import {getCss} from './controlled_radio_button.css.js';
import {getHtml} from './controlled_radio_button.html.js';
import {PrefKeyObserverMixinLit} from './pref_key_observer_mixin_lit.js';

const ControlledRadioButtonElementBase =
    PrefKeyObserverMixinLit(CrRippleMixin(CrRadioButtonMixinLit(CrLitElement)));

export class ControlledRadioButtonElement extends
    ControlledRadioButtonElementBase {
  static get is() {
    return 'controlled-radio-button';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      pref: {type: Object},
    };
  }

  protected accessor pref: chrome.settingsPrivate.PrefObject|undefined =
      undefined;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('pref')) {
      this.disabled = this.pref?.enforcement ===
          chrome.settingsPrivate.Enforcement.ENFORCED;
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

  protected showIndicator_(): boolean {
    if (!this.disabled || !this.pref) {
      return false;
    }

    return this.name === prefToString(this.pref);
  }

  protected onIndicatorClick_(e: Event) {
    // Disallow <controlled-radio-button on-click="..."> when disabled.
    e.preventDefault();
    e.stopPropagation();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'controlled-radio-button': ControlledRadioButtonElement;
  }
}

customElements.define(
    ControlledRadioButtonElement.is, ControlledRadioButtonElement);
