// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * settings-slider wraps a cr-slider. It maps the slider's values from a
 * linear UI range to a range of real values.  When |value| does not map exactly
 * to a tick mark, it interpolates to the nearest tick.
 */
import '//resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '//resources/ash/common/cr_elements/cr_slider/cr_slider.js';
import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';

import {CrSliderElement, SliderTick} from '//resources/ash/common/cr_elements/cr_slider/cr_slider.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CrPolicyPrefMixin} from '/shared/settings/controls/cr_policy_pref_mixin.js';

import {getTemplate} from './settings_slider.html.js';

export interface SettingsSliderElement {
  $: {
    slider: CrSliderElement,
  };
}

const SettingsSliderElementBase = CrPolicyPrefMixin(PolymerElement);

export class SettingsSliderElement extends SettingsSliderElementBase {
  static get is() {
    return 'settings-slider';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      pref: Object,

      /**
       * Values corresponding to each tick.
       */
      ticks: {
        type: Array,
        value: () => [],
      },

      /**
       * A scale factor used to support fractional pref values. This is not
       * compatible with |ticks|, i.e. if |scale| is not 1 then |ticks| must be
       * empty.
       */
      scale: {
        type: Number,
        value: 1,
      },

      min: Number,

      max: Number,

      labelAria: String,

      labelMin: String,

      labelMax: String,

      disabled: Boolean,

      // The value of ariaDisabled should only be "true" or "false".
      ariaDisabled: String,

      showMarkers: Boolean,

      disableSlider_: {
        computed: 'computeDisableSlider_(pref.*, disabled, ticks.*)',
        type: Boolean,
      },

      updateValueInstantly: {
        type: Boolean,
        value: true,
        observer: 'onSliderChanged_',
      },

      loaded_: Boolean,
    };
  }

  static get observers() {
    return [
      'valueChanged_(pref.*, ticks.*, loaded_)',
    ];
  }

  pref: chrome.settingsPrivate.PrefObject<number>;
  ticks: SliderTick[]|number[];
  scale: number;
  min: number;
  max: number;
  labelAria: string;
  labelMin: string;
  labelMax: string;
  disabled: boolean;
  showMarkers: boolean;
  private disableSlider_: boolean;
  updateValueInstantly: boolean;
  private loaded_: boolean;

  override ariaDisabled: string;

  override connectedCallback(): void {
    super.connectedCallback();

    this.loaded_ = true;
  }

  override focus(): void {
    this.$.slider.focus();
  }

  private getTickValue_(tick: number|SliderTick): number {
    return typeof tick === 'object' ? tick.value : tick;
  }

  private getTickValueAtIndex_(index: number): number {
    return this.getTickValue_(this.ticks[index]);
  }

  /**
   * Sets the |pref.value| property to the value corresponding to the knob
   * position after a user action.
   */
  private onSliderChanged_(): void {
    if (!this.loaded_) {
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

    this.set('pref.value', newValue);
  }

  private computeDisableSlider_(): boolean {
    return this.disabled || this.isPrefEnforced();
  }

  /**
   * Updates the knob position when |pref.value| changes. If the knob is still
   * being dragged, this instead forces |pref.value| back to the current
   * position.
   */
  private valueChanged_(): void {
    if (this.pref === undefined || !this.loaded_ || this.$.slider.dragging ||
        this.$.slider.updatingFromKey) {
      return;
    }

    // First update the slider settings if |ticks| was set.
    const numTicks = this.ticks.length;
    if (numTicks === 1) {
      this.$.slider.disabled = true;
      return;
    }

    const prefValue = this.pref.value;

    // The preference and slider values are continuous when |ticks| is empty.
    if (numTicks === 0) {
      this.$.slider.value = prefValue * this.scale;
      return;
    }

    assert(this.scale === 1);
    // Limit the number of ticks to 10 to keep the slider from looking too busy.
    const MAX_TICKS = 10;
    this.$.slider.markerCount =
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
    if (this.$.slider.value !== index) {
      this.$.slider.value = index;
    }
    const tickValue = this.getTickValueAtIndex_(index);
    if (this.pref.value !== tickValue) {
      this.set('pref.value', tickValue);
    }
  }

  private getRoleDescription_(): string {
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
