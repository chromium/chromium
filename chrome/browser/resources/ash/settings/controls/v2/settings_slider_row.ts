// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 *
 * 'settings-slider-row' is a component which wraps a basic 'settings-row' with
 * a slotted control component `settings-slider-v2`. This component can be used
 * with or without prefs.
 *
 * - Usage: without pref
 *   - `value` must be specified and `pref` must not be used.
 *
 *   <settings-slider-row
 *       label="Lorem ipsum"
 *       sublabel="Lorem ipsum dolor sit amet"
 *       icon="os-settings:display"
 *       value="[[sliderValue_]]"
 *       ticks="[[sliderTicks_]]"
 *       on-change="onSliderRowChange_"
 *       min-label="$i18n{low}"
 *       max-label="$i18n{high}">
 *   <settings-slider-v2>
 *
 * - Usage: with pref
 *   - `pref` must be specified and `value` must not be used.
 *
 *   <settings-slider-row
 *       label="Lorem ipsum"
 *       sublabel="Lorem ipsum dolor sit amet"
 *       icon="os-settings:display"
 *       pref="[[prefs.foo.bar]]"
 *       min="0"
 *       max="100"
 *       scale="100"
 *       on-change="onSliderRowChange_"
 *       min-label="$i18n{low}"
 *       max-label="$i18n{high}"
 *       update-value-instantly>
 *   <settings-slider-row>
 */

import './settings_slider_v2.js';
import './settings_row.js';

import {SliderTick} from 'chrome://resources/ash/common/cr_elements/cr_slider/cr_slider.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseRowMixin} from './base_row_mixin.js';
import {PrefControlMixinInternal} from './pref_control_mixin_internal.js';
import {SettingsRowElement} from './settings_row.js';
import {getTemplate} from './settings_slider_row.html.js';
import {SettingsSliderV2Element} from './settings_slider_v2.js';

export interface SettingsSliderRowElement {
  $: {
    slider: SettingsSliderV2Element,
    internalRow: SettingsRowElement,
  };
}

const SettingsSliderRowElementBase =
    PrefControlMixinInternal(BaseRowMixin(PolymerElement));

export class SettingsSliderRowElement extends SettingsSliderRowElementBase {
  static get is() {
    return 'settings-slider-row' as const;
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

      updateValueInstantly: {
        type: Boolean,
        value: false,
      },
    };
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
  value: number;

  override focus(): void {
    this.$.slider.focus();
  }

  private propagateChangeEvent_({detail}: CustomEvent<number>): void {
    this.dispatchEvent(new CustomEvent('change', {
      bubbles: true,
      composed: false,  // Event should not pass the shadow DOM boundary.
      detail,
    }));
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
    [SettingsSliderRowElement.is]: SettingsSliderRowElement;
  }
}

customElements.define(SettingsSliderRowElement.is, SettingsSliderRowElement);
