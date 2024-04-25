// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * settings-slider-v2 wraps a cr-slider. It maps the slider's values from a
 * linear UI range to a range of real values. When `value` does not map exactly
 * to a tick mark, it interpolates to the nearest tick.
 */
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_slider/cr_slider.js';
import 'chrome://resources/ash/common/cr_elements/cros_color_overrides.css.js';

import {CrSliderElement, SliderTick} from 'chrome://resources/ash/common/cr_elements/cr_slider/cr_slider.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PrefControlMixinInternal} from './pref_control_mixin_internal.js';
import {getTemplate} from './settings_slider_v2.html.js';

export interface SettingsSliderV2Element {
  $: {
    slider: CrSliderElement,
  };
}

const SettingsSliderV2ElementBase = PrefControlMixinInternal(PolymerElement);

export class SettingsSliderV2Element extends SettingsSliderV2ElementBase {
  static get is() {
    return 'settings-slider-v2' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Values corresponding to each tick. It should be empty if `scale` is
       * specified.
       */
      ticks: {
        type: Array,
        value: () => [],
      },

      /**
       * A scale factor used to support fractional pref values. This is not
       * compatible with `ticks`, i.e. if `scale` is not 1 then `ticks` must be
       * empty.
       */
      scale: {
        type: Number,
        value: 1,
      },

      /** The slider minimum value. */
      min: Number,

      /** The slider maximum value. */
      max: Number,

      /**
       * whether or not to hide the min and max label below the slider. Defaults
       * to false.
       */
      hideLabel: {
        type: Boolean,
        value: false,
      },

      /** Label under the min end of the slider. */
      minLabel: String,

      /** Label under the max end of the slider. */
      maxLabel: String,

      /** Accessibility label. */
      ariaLabel: String,

      /**
       * By default, the slider value will only be updated when the dragging
       * event is finished.
       */
      updateValueInstantly: {
        type: Boolean,
        value: false,
        observer: 'onSliderChanged_',
      },

      /** Whether or not to show tick marks on the slider. Default to false. */
      showMarkers: {
        type: Boolean,
        value: false,
      },

      loaded_: Boolean,

      /**
       * The current value of the slider. It shouldn't be used or updated if
       * `pref` is specified.
       */
      value_: Number,
    };
  }

  static get observers() {
    return [
      'valueChanged_(pref.*, ticks.*, loaded_, value_)',
    ];
  }

  ticks: SliderTick[]|number[];
  scale: number;
  min: number;
  max: number;
  hideLabel: boolean;
  minLabel: string;
  maxLabel: string;
  showMarkers: boolean;
  updateValueInstantly: boolean;
  private loaded_: boolean;
  private value_: number;

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
   * Dispatches a `user-action-setting-pref-change` event to update `pref.value`
   * property to the value corresponding to the knob position after a user
   * action. If `pref` is not defined, update the `value_` instead.
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

    if (this.pref) {
      this.set('pref.value', newValue);
      this.dispatchPrefChange(newValue);
    } else {
      this.value_ = newValue;
    }
  }

  /**
   * Updates the knob position when slider value changes. If the knob is still
   * being dragged, this instead forces slider value back to the current
   * position.
   */
  private valueChanged_(): void {
    if (!this.loaded_ || this.$.slider.dragging ||
        this.$.slider.updatingFromKey) {
      return;
    }

    // First update the slider settings if `ticks` was set.
    const numTicks = this.ticks.length;
    if (numTicks === 1) {
      this.disabled = true;
      return;
    }

    const value = (this.pref ? this.pref.value : this.value_);

    // The preference and slider values are continuous when `ticks` is empty.
    if (numTicks === 0) {
      this.$.slider.value = value * this.scale;
      return;
    }

    assert(this.scale === 1);
    // Limit the number of ticks to 10 to keep the slider from looking too busy.
    const MAX_TICKS = 10;
    this.$.slider.markerCount =
        (this.showMarkers || numTicks <= MAX_TICKS) ? numTicks : 0;

    // Convert from the public `value` to the slider index (where the knob
    // should be positioned on the slider).
    const index =
        this.ticks
            .map(
                (tick: number|SliderTick) =>
                    Math.abs(this.getTickValue_(tick) - value))
            .reduce(
                (acc, diff, index) => diff < acc.diff ? {index, diff} : acc,
                {index: -1, diff: Number.MAX_VALUE})
            .index;
    assert(index !== -1);
    if (this.$.slider.value !== index) {
      this.$.slider.value = index;
    }
    const tickValue = this.getTickValueAtIndex_(index);

    if (this.pref && this.pref.value !== tickValue) {
      this.set('pref.value', tickValue);
      this.dispatchPrefChange(tickValue);
    }
    if (!this.pref && this.value_ !== tickValue) {
      this.value_ = tickValue;
    }
  }

  private getRoleDescription_(): string {
    return loadTimeData.getStringF(
        'settingsSliderRoleDescription', this.minLabel, this.maxLabel);
  }

  private getAriaDisabled_(): string {
    return this.disabled ? 'true' : 'false';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsSliderV2Element.is]: SettingsSliderV2Element;
  }
}

customElements.define(SettingsSliderV2Element.is, SettingsSliderV2Element);
