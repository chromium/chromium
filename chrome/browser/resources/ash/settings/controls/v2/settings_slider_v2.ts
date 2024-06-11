// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * settings-slider-v2 is a component that displays a range of values. When
 * `ticks` is specified and `value` does not map exactly to a tick mark, the
 * slider interpolates to the nearest tick.
 *
 * - Usage: without pref
 *   - `value` must be specified and `pref` must not be used.
 *
 *   // With ticks
 *   <settings-slider-v2
 *       value="[[sliderValue_]]"
 *       ticks="[[sliderTicks_]]"
 *       on-change="onSliderChange_"
 *       min-label="$i18n{low}"
 *       max-label="$i18n{high}">
 *   <settings-slider-v2>
 *
 *   // With scale
 *   <settings-slider-v2
 *       value="[[sliderValue_]]"
 *       min="0"
 *       max="100"
 *       scale="100"
 *       on-change="onSliderChange_"
 *       min-label="$i18n{low}"
 *       max-label="$i18n{high}">
 *   <settings-slider-v2>
 *
 * - Usage: with pref
 *   - `pref` must be specified and `value` must not be used.
 *
 *   // With ticks
 *   <settings-slider-v2
 *       pref="[[prefs.foo.bar]]"
 *       ticks="[[sliderTicks_]]"
 *       on-change="onSliderChange_"
 *       min-label="$i18n{low}"
 *       max-label="$i18n{high}"
 *       hide-markers>
 *   <settings-slider-v2>
 *
 *   // With scale
 *   <settings-slider-v2
 *       pref="[[prefs.foo.bar]]"
 *       min="0"
 *       max="100"
 *       scale="100"
 *       on-change="onSliderChange_"
 *       min-label="$i18n{low}"
 *       max-label="$i18n{high}">
 *   <settings-slider-v2>
 */

import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_hidden_style.css.js';
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
       * The current value of the slider. It shouldn't be used or updated if
       * `pref` is specified.
       */
      value: Number,

      /**
       * Values corresponding to each tick. It should be empty if `scale` is
       * specified.
       */
      ticks: {
        type: Array,
        value: () => [],
      },

      /**
       * A scale factor used to support fractional values. For example, if
       * min=0, max=10, scale=10, the value ranges for any decimal number
       * between [0, 1]. If min=0, max=10, scale=1, the value ranges for any
       * decimal number between [0, 10].
       * This is not compatible with `ticks`, i.e. if `scale` is not 1 then
       * `ticks` must not be set.
       */
      scale: {
        type: Number,
        value: 1,
      },

      /**
       * The slider minimum value. If `ticks` is not used, this must be
       * specified.
       */
      min: Number,

      /**
       * The slider maximum value. If `ticks` is not used, this must be
       * specified.
       */
      max: Number,

      /**
       * Label for the min value of the slider that shows below the slider. Also
       * used as the A11y label for the min value.
       */
      minLabel: String,

      /**
       * Label for the max value of the slider that shows below the slider. Also
       * used as the A11y label for the max value.
       */
      maxLabel: String,

      /**
       * Whether or not to hide the min and max labels below the slider.
       * Defaults to false.
       */
      hideLabel: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether or not to hide tick marks on the slider. Default to false.
       * Only compatible with `ticks`, and not compatible with `scale`.
       */
      hideMarkers: {
        type: Boolean,
        value: false,
      },

      /**
       * By default, the slider value will only be updated when the dragging
       * event is finished. Set this property to true to trigger a change event
       * any time the slider is dragged.
       */
      updateValueInstantly: {
        type: Boolean,
        value: false,
        observer: 'onSliderChanged_',
      },

      // A11y properties added since they are data-bound in HTML.
      ariaLabel: {
        type: String,
        reflectToAttribute: false,
        observer: 'onAriaLabelSet_',
      },

      ariaDescription: {
        type: String,
        reflectToAttribute: false,
        observer: 'onAriaDescriptionSet_',
      },

      loaded_: Boolean,
    };
  }

  static get observers() {
    return [
      'syncPrefChangeToValue_(pref.*)',
      'valueChanged_(value, ticks.*, loaded_)',
    ];
  }

  ticks: SliderTick[]|number[];
  scale: number;
  min: number;
  max: number;
  hideLabel: boolean;
  minLabel: string;
  maxLabel: string;
  hideMarkers: boolean;
  updateValueInstantly: boolean;
  override validPrefTypes = [chrome.settingsPrivate.PrefType.NUMBER];
  value: number;
  private loaded_: boolean;

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
   * action. If `pref` is not defined, update the `value` instead.
   */
  private onSliderChanged_(): void {
    if (!this.loaded_) {
      return;
    }

    if (this.$.slider.dragging && !this.updateValueInstantly) {
      return;
    }

    const sliderValue = this.$.slider.value;
    if (this.ticks.length > 0) {
      this.value = this.getTickValueAtIndex_(sliderValue);
    } else {
      this.value = sliderValue / this.scale;
    }

    if (this.pref) {
      this.updatePrefValueFromUserAction(this.value);
    }

    this.dispatchEvent(new CustomEvent('change', {
      bubbles: true,
      composed: false,  // Event should not pass the shadow DOM boundary.
      detail: this.value,
    }));
  }

  /**
   * This observer watches changes to `pref` and syncs it to the `value`
   * property.
   */
  private syncPrefChangeToValue_(): void {
    if (this.pref) {
      this.value = this.pref.value;
    }
  }

  /**
   * This observer watches changes to `value` via downwards data-flow and
   * updates the slider accordingly. If the knob is in the middle of being
   * dragged, the slider value is forced back to the current position.
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

    const currentValue = this.value;

    // The preference and slider values are continuous when `ticks` is empty.
    if (numTicks === 0) {
      this.$.slider.value = currentValue * this.scale;
      return;
    }

    assert(this.scale === 1, 'Scale has to be 1 if ticks is set.');
    // Limit the number of ticks to 10 to keep the slider from looking too busy.
    const MAX_TICKS = 10;
    this.$.slider.markerCount =
        (this.hideMarkers || numTicks > MAX_TICKS) ? 0 : numTicks;

    // Convert from the public `value` to the slider index (where the knob
    // should be positioned on the slider).
    const index =
        this.ticks
            .map(
                (tick: number|SliderTick) =>
                    Math.abs(this.getTickValue_(tick) - currentValue))
            .reduce(
                (acc, diff, index) => diff < acc.diff ? {index, diff} : acc,
                {index: -1, diff: Number.MAX_VALUE})
            .index;
    assert(index !== -1);
    if (this.$.slider.value !== index) {
      this.$.slider.value = index;
    }
    const tickValue = this.getTickValueAtIndex_(index);
    if (this.value !== tickValue) {
      this.value = tickValue;
    }
  }

  private getRoleDescription_(): string {
    if (this.minLabel && this.maxLabel) {
      return loadTimeData.getStringF(
          'settingsSliderRoleDescription', this.minLabel, this.maxLabel);
    }

    return '';
  }

  private getAriaDisabled_(): string {
    return this.disabled ? 'true' : 'false';
  }

  /**
   * Manually remove the aria-label attribute from the host node since it is
   * applied to the internal slider. `reflectToAttribute=false` does not resolve
   * this issue. This prevents the aria-label from being duplicated by
   * screen readers.
   */
  private onAriaLabelSet_(): void {
    const ariaLabel = this.getAttribute('aria-label');
    this.removeAttribute('aria-label');
    if (ariaLabel) {
      this.ariaLabel = ariaLabel;
    }
  }

  /**
   * Manually remove the aria-description attribute from the host node since it
   * is applied to the internal slider. `reflectToAttribute=false` does not
   * resolve this issue. This prevents the aria-description from being
   * duplicated by screen readers.
   */
  private onAriaDescriptionSet_(): void {
    const ariaDescription = this.getAttribute('aria-description');
    this.removeAttribute('aria-description');
    if (ariaDescription) {
      this.ariaDescription = ariaDescription;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsSliderV2Element.is]: SettingsSliderV2Element;
  }
}

customElements.define(SettingsSliderV2Element.is, SettingsSliderV2Element);
