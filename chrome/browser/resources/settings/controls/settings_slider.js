// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * settings-slider wraps a cr-slider. It maps the slider's values from a
 * linear UI range to a range of real values.  When |value| does not map exactly
 * to a tick mark, it interpolates to the nearest tick.
 */
Polymer({
  is: 'settings-slider',

  behaviors: [CrPolicyPrefBehavior],

  properties: {
    /** @type {!chrome.settingsPrivate.PrefObject} */
    pref: Object,

    /**
     * Values corresponding to each tick.
     * @type {!Array<cr_slider.SliderTick>|!Array<number>}
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

    labelMin: String,

    labelMax: String,

    disabled: Boolean,

    showMarkers: Boolean,

    /** @private */
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
  },

  observers: [
    'valueChanged_(pref.*, ticks.*, loaded_)',
  ],

  attached: function() {
    this.loaded_ = true;
  },

  /**
   * @param {number|cr_slider.SliderTick} tick
   * @return {number|undefined}
   */
  getTickValue_: function(tick) {
    return typeof tick == 'object' ? tick.value : tick;
  },

  /**
   * @param {number} index
   * @return {number|undefined}
   * @private
   */
  getTickValueAtIndex_: function(index) {
    return this.getTickValue_(this.ticks[index]);
  },

  /**
   * Sets the |pref.value| property to the value corresponding to the knob
   * position after a user action.
   * @private
   */
  onSliderChanged_: function() {
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
  },

  /** @private */
  computeDisableSlider_: function() {
    return this.disabled || this.isPrefEnforced();
  },

  /**
   * Updates the knob position when |pref.value| changes. If the knob is still
   * being dragged, this instead forces |pref.value| back to the current
   * position.
   * @private
   */
  valueChanged_: function() {
    if (this.pref == undefined || !this.loaded_ || this.$.slider.dragging ||
        this.$.slider.updatingFromKey) {
      return;
    }

    // First update the slider settings if |ticks| was set.
    const numTicks = this.ticks.length;
    if (numTicks == 1) {
      this.$.slider.disabled = true;
      return;
    }

    const prefValue = /** @type {number} */ (this.pref.value);

    // The preference and slider values are continuous when |ticks| is empty.
    if (numTicks == 0) {
      this.$.slider.value = prefValue * this.scale;
      return;
    }

    assert(this.scale == 1);
    // Limit the number of ticks to 10 to keep the slider from looking too busy.
    const MAX_TICKS = 10;
    this.$.slider.markerCount =
        (this.showMarkers || numTicks <= MAX_TICKS) ? numTicks : 0;

    // Convert from the public |value| to the slider index (where the knob
    // should be positioned on the slider).
    const index =
        this.ticks.map(tick => Math.abs(this.getTickValue_(tick) - prefValue))
            .reduce(
                (acc, diff, index) => diff < acc.diff ? {index, diff} : acc,
                {index: -1, diff: Number.MAX_VALUE})
            .index;
    assert(index != -1);
    if (this.$.slider.value != index) {
      this.$.slider.value = index;
    }
    const tickValue = this.getTickValueAtIndex_(index);
    if (this.pref.value != tickValue) {
      this.set('pref.value', tickValue);
    }
  },
});
