// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * settings-slider wraps a cr-slider. It maps the slider's values from a
 * linear UI range to a range of real values. When |value| does not map exactly
 * to a tick mark, it interpolates to the nearest tick.
 */
import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/cr_elements/cr_slider/cr_slider.js';
import '/shared/settings/controls/cr_policy_pref_indicator.js';

import type {CrSliderElement, SliderTick} from '//resources/cr_elements/cr_slider/cr_slider.js';
import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrPolicyPrefMixinLit} from '/shared/settings/controls/cr_policy_pref_mixin_lit.js';
import {PrefService} from '/shared/settings/prefs2/pref_service.js';

import {loadTimeData} from '../i18n_setup.js';

import {PrefKeyObserverMixinLit} from './pref_key_observer_mixin_lit.js';
import {getCss} from './settings_slider.css.js';
import {getHtml} from './settings_slider.html.js';

export interface SettingsSliderElement {
  $: {
    slider: CrSliderElement,
  };
}

const SettingsSliderElementBase =
    CrPolicyPrefMixinLit(PrefKeyObserverMixinLit(CrLitElement));

export class SettingsSliderElement extends SettingsSliderElementBase {
  static get is() {
    return 'settings-slider';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /** Values corresponding to each tick. */
      ticks: {type: Array},

      /**
       * A scale factor used to support fractional pref values. This is not
       * compatible with |ticks|, i.e. if |scale| is not 1 then |ticks| must be
       * empty.
       */
      scale: {type: Number},

      min: {type: Number},
      max: {type: Number},
      labelAria: {type: String},
      labelMin: {type: String},
      labelMax: {type: String},
      disabled: {type: Boolean},

      // The value of ariaDisabled should only be "true" or "false".
      ariaDisabled: {type: String},

      showMarkers: {type: Boolean},
      disableSlider_: {type: Boolean},
      updateValueInstantly: {type: Boolean},
      sliderValue_: {type: Number},
      markerCount_: {type: Number},
      pref: {type: Object},
    };
  }

  override accessor pref: chrome.settingsPrivate.PrefObject<number>|undefined =
      undefined;

  accessor ticks: SliderTick[]|number[] = [];
  accessor scale: number = 1;
  accessor min: number|undefined = undefined;
  accessor max: number|undefined = undefined;
  accessor labelAria: string = '';
  accessor labelMin: string = '';
  accessor labelMax: string = '';
  accessor disabled: boolean = false;
  override accessor ariaDisabled: string = '';
  accessor showMarkers: boolean = false;
  accessor updateValueInstantly: boolean = true;

  protected accessor disableSlider_: boolean = false;
  protected accessor sliderValue_: number = 0;
  protected accessor markerCount_: number = 0;

  override willUpdate(changedProperties: PropertyValues) {
    super.willUpdate(changedProperties as PropertyValues<this>);

    if (changedProperties.has('disabled') || changedProperties.has('pref')) {
      this.disableSlider_ = this.computeDisableSlider_();
    }

    if (changedProperties.has('updateValueInstantly')) {
      this.onSliderChanged_();
    }
  }

  override updated(changedProperties: PropertyValues) {
    super.updated(changedProperties as PropertyValues<this>);

    if (changedProperties.has('pref') || changedProperties.has('ticks') ||
        changedProperties.has('showMarkers')) {
      this.valueChanged_();
    }
  }

  override focus() {
    this.$.slider.focus();
  }

  private getTickValue_(tick: number|SliderTick): number {
    return typeof tick === 'object' ? tick.value : tick;
  }

  private getTickValueAtIndex_(index: number): number {
    return this.getTickValue_(this.ticks[index]);
  }

  protected onCrSliderValueChanged_() {
    this.onSliderChanged_();
  }

  protected onDraggingChanged_() {
    this.onSliderChanged_();
  }

  protected onUpdatingFromKey_() {
    this.onSliderChanged_();
  }

  /**
   * Sets the |pref.value| property to the value corresponding to the knob
   * position after a user action.
   */
  private onSliderChanged_() {
    if (!this.pref) {
      return;
    }

    if (this.$.slider.dragging && !this.updateValueInstantly) {
      return;
    }

    const sliderValue = this.$.slider.value;

    let newValue;
    if (this.ticks && this.ticks.length > 0) {
      newValue = this.getTickValueAtIndex_(sliderValue);
    } else {
      newValue = sliderValue / this.scale;
    }

    PrefService.getInstance().setPrefValue(this.prefKey, newValue);
  }

  private computeDisableSlider_(): boolean {
    return this.disabled || this.isPrefEnforced();
  }

  /**
   * Updates the knob position when |pref.value| changes. If the knob is still
   * being dragged, this instead forces |pref.value| back to the current
   * position.
   */
  private valueChanged_() {
    if (this.pref === undefined || this.$.slider.dragging ||
        this.$.slider.updatingFromKey) {
      return;
    }

    const numTicks = this.ticks.length;
    if (this.ticks.length === 1) {
      return;
    }

    const prefValue = this.pref.value;

    // The preference and slider values are continuous when |ticks| is empty.
    if (numTicks === 0) {
      this.sliderValue_ = prefValue * this.scale;
      return;
    }

    assert(this.scale === 1);
    // Limit the number of ticks to 10 to keep the slider from looking too busy.
    const MAX_TICKS = 10;
    this.markerCount_ =
        (this.showMarkers || numTicks <= MAX_TICKS) ? numTicks : 0;

    // Convert from the public |value| to the slider index (where the knob
    // should be positioned on the slider).
    const index =
        this.ticks
            .map(
                (tick: number|SliderTick) =>
                    Math.abs(this.getTickValue_(tick) - prefValue))
            .reduce(
                (acc, diff, index) => diff < acc.diff ? {index, diff} : acc,
                {index: -1, diff: Number.MAX_VALUE})
            .index;
    assert(index !== -1);
    this.sliderValue_ = index;

    const tickValue = this.getTickValueAtIndex_(index);
    if (this.pref.value !== tickValue) {
      PrefService.getInstance().setPrefValue(this.pref.key, tickValue);
    }
  }

  protected getRoleDescription_(): string {
    return loadTimeData.getStringF(
        'settingsSliderRoleDescription', this.labelMin, this.labelMax);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-slider': SettingsSliderElement;
  }
}

customElements.define(SettingsSliderElement.is, SettingsSliderElement);
