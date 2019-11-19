// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

/**
 * @fileoverview
 * night-light-slider is used to set the custom automatic schedule of the
 * Night Light feature, so that users can set their desired start and end
 * times.
 */

const HOURS_PER_DAY = 24;
const MIN_KNOBS_DISTANCE_MINUTES = 60;
const OFFSET_MINUTES_6PM = 18 * 60;
const TOTAL_MINUTES_PER_DAY = 24 * 60;

/**
 * % is the javascript remainder operator that satisfies the following for the
 * resultant z given the operands x and y as in (z = x % y):
 *   1. x = k * y + z
 *   2. k is an integer.
 *   3. |z| < |y|
 *   4. z has the same sign as x.
 *
 * It is more convenient to have z be the same sign as y. In most cases y
 * is a positive integer, and it is more intuitive to have z also be a positive
 * integer (0 <= z < y).
 *
 * For example (-1 % 24) equals -1 whereas modulo(-1, 24) equals 23.
 * @param {number} x
 * @param {number} y
 * @return {number}
 */
function modulo(x, y) {
  return ((x % y) + y) % y;
}

Polymer({
  is: 'night-light-slider',

  behaviors: [
    PrefsBehavior,
    Polymer.IronResizableBehavior,
    Polymer.PaperRippleBehavior,
  ],

  properties: {
    /**
     * Whether the element is ready and fully rendered.
     * @private
     */
    isReady_: Boolean,

    /**
     * Whether the window is in RTL locales.
     * @private
     */
    isRTL_: Boolean,

    /**
     * Whether to use the 24-hour format for the time shown in the label
     * bubbles.
     * @private
     */
    shouldUse24Hours_: Boolean,
  },

  listeners: {
    'iron-resize': 'onResize_',
    focus: 'onFocus_',
    blur: 'onBlur_',
    keydown: 'onKeyDown_',
  },

  observers: [
    'updateKnobs_(prefs.ash.night_light.custom_start_time.*, ' +
        'prefs.ash.night_light.custom_end_time.*, isRTL_, isReady_)',
    'hourFormatChanged_(prefs.settings.clock.use_24hour_clock.*)',
    'updateMarkers_(prefs.ash.night_light.custom_start_time.*, ' +
        'prefs.ash.night_light.custom_end_time.*, isRTL_, isReady_)',
  ],

  /**
   * The object currently being dragged. Either the start or end knobs.
   * @type {Element}
   * @private
   */
  dragObject_: null,

  /** @override */
  attached: function() {
    this.isRTL_ = window.getComputedStyle(this).direction == 'rtl';

    this.$.sliderContainer.addEventListener('contextmenu', function(e) {
      // Prevent the context menu from interfering with dragging the knobs using
      // touch.
      e.preventDefault();
      return false;
    });

    setTimeout(() => {
      // This is needed to make sure that the positions of the knobs and their
      // label bubbles are correctly updated when the display settings page is
      // opened for the first time after login. The page need to be fully
      // rendered.
      this.isReady_ = true;
    });
  },

  /**
   * @return {boolean}
   * @private
   */
  prefsAvailable: function() {
    return ['custom_start_time', 'custom_end_time']
        .map(key => `prefs.ash.night_light.${key}.value`)
        .every(path => this.get(path) != undefined);
  },

  /** @private */
  updateMarkers_: function() {
    if (!this.isReady_ || !this.prefsAvailable()) {
      return;
    }

    const startHour = /** @type {number} */ (
            this.getPref('ash.night_light.custom_start_time').value) / 60.0;
    const endHour = /** @type {number} */ (
        this.getPref('ash.night_light.custom_end_time').value) / 60.0;

    const markersContainer = this.$.markersContainer;
    markersContainer.innerHTML = '';
    for (let i = 0; i <= HOURS_PER_DAY; ++i) {
      const marker = document.createElement('div');

      const hourIndex = this.isRTL_ ? 24 - i : i;
      // Rotate around clock by 18 hours for the 6pm start.
      const hour = (hourIndex + 18) % 24;
      if (startHour < endHour) {
        marker.className = hour > startHour && hour < endHour ?
            'active-marker' :
            'inactive-marker';
      } else {
        marker.className = hour > endHour && hour < startHour ?
            'inactive-marker' :
            'active-marker';
      }
      markersContainer.appendChild(marker);
      marker.style.left = (i * 100 / HOURS_PER_DAY) + '%';
    }
  },

  /**
   * Invoked when the element is resized and the knobs positions need to be
   * updated.
   * @private
   */
  onResize_: function() {
    this.updateKnobs_();
  },

  /**
   * Called when the value of the pref associated with whether to use the
   * 24-hour clock format is changed. This will also refresh the slider.
   * @private
   */
  hourFormatChanged_: function() {
    this.shouldUse24Hours_ = /** @type {boolean} */ (
        this.getPref('settings.clock.use_24hour_clock').value);
  },

  /**
   * Gets the style of legend div determining its absolute left position.
   * @param {number} percent The value of the div's left as a percent (0 - 100).
   * @param {boolean} isRTL whether window is in RTL locale.
   * @return {string} The CSS style of the legend div.
   * @private
   */
  getLegendStyle_: function(percent, isRTL) {
    percent = isRTL ? 100 - percent : percent;
    return 'left: ' + percent + '%';
  },

  /**
   * If one of the two knobs is focused, this function blurs it.
   * @private
   */
  blurAnyFocusedKnob_: function() {
    const activeElement = this.shadowRoot.activeElement;
    if (activeElement == this.$.startKnob || activeElement == this.$.endKnob) {
      activeElement.blur();
    }
  },

  /**
   * Start dragging the target knob.
   * @param {!Event} event
   * @private
   */
  startDrag_: function(event) {
    event.preventDefault();

    // Only handle start or end knobs. Use the "knob-inner" divs just to display
    // the knobs.
    if (event.target == this.$.startKnob ||
        event.target == this.$.startKnob.firstElementChild) {
      this.dragObject_ = this.$.startKnob;
    } else if (event.target == this.$.endKnob ||
               event.target == this.$.endKnob.firstElementChild) {
      this.dragObject_ = this.$.endKnob;
    } else {
      return;
    }

    this.handleKnobEvent_(event, this.dragObject_);

    this.valueAtDragStart_ = this.getPrefValue_(this.dragObject_);
  },

  /**
   * Continues dragging the selected knob if any.
   * @param {!Event} event
   * @private
   */
  continueDrag_: function(event) {
    if (!this.dragObject_) {
      return;
    }

    event.stopPropagation();
    switch (event.detail.state) {
      case 'start':
        this.startDrag_(event);
        break;
      case 'track':
        this.doKnobTracking_(event);
        break;
      case 'end':
        this.endDrag_(event);
        break;
    }
  },

  /**
   * Converts horizontal pixels into number of minutes.
   * @param {number} deltaX
   * @return {number}
   * @private
   */
  getDeltaMinutes_: function(deltaX) {
    return (this.isRTL_ ? -1 : 1) *
        Math.floor(
            TOTAL_MINUTES_PER_DAY * deltaX / this.$.sliderBar.offsetWidth);
  },

  /**
   * Updates the knob's corresponding pref value in response to dragging, which
   * will in turn update the location of the knob and its corresponding label
   * bubble and its text contents.
   * @param {!Event} event
   * @private
   */
  doKnobTracking_: function(event) {
    const lastDeltaMinutes = this.getDeltaMinutes_(event.detail.ddx);
    if (Math.abs(lastDeltaMinutes) < 1) {
      return;
    }

    // Using |ddx| to compute the delta minutes and adding that to the current
    // value will result in a rounding error for every update. The cursor will
    // drift away from the knob. Storing the original value and calculating the
    // delta minutes from |dx| will provide a stable update that will not lose
    // pixel movement due to rounding.
    this.updatePref_(
        this.valueAtDragStart_ + this.getDeltaMinutes_(event.detail.dx), true);
  },

  /**
   * Ends the dragging.
   * @param {!Event} event
   * @private
   */
  endDrag_: function(event) {
    event.preventDefault();
    this.dragObject_ = null;
    this.removeRipple_();
  },

  /**
   * Gets the given knob's offset ratio with respect to its parent element
   * (which is the slider bar).
   * @param {HTMLDivElement} knob Either one of the two knobs.
   * @return {number}
   * @private
   */
  getKnobRatio_: function(knob) {
    return parseFloat(knob.style.left) / this.$.sliderBar.offsetWidth;
  },

  /**
   * Converts the time of day, given as |hour| and |minutes|, to its language-
   * sensitive time string representation.
   * @param {number} hour The hour of the day (0 - 23).
   * @param {number} minutes The minutes of the hour (0 - 59).
   * @param {boolean} shouldUse24Hours Whether to use the 24-hour time format.
   * @return {string}
   * @private
   */
  getLocaleTimeString_: function(hour, minutes, shouldUse24Hours) {
    const d = new Date();
    d.setHours(hour);
    d.setMinutes(minutes);
    d.setSeconds(0);
    d.setMilliseconds(0);

    return d.toLocaleTimeString(
        [], {hour: '2-digit', minute: '2-digit', hour12: !shouldUse24Hours});
  },

  /**
   * Converts the |offsetMinutes| value (which the number of minutes since
   * 00:00) to its language-sensitive time string representation.
   * @param {number} offsetMinutes The time of day represented as the number of
   * minutes from 00:00.
   * @param {boolean} shouldUse24Hours Whether to use the 24-hour time format.
   * @return {string}
   * @private
   */
  getTimeString_: function(offsetMinutes, shouldUse24Hours) {
    const hour = Math.floor(offsetMinutes / 60);
    const minute = Math.floor(offsetMinutes % 60);

    return this.getLocaleTimeString_(hour, minute, shouldUse24Hours);
  },

  /**
   * Using the current start and end times prefs, this function updates the
   * knobs and their label bubbles and refreshes the slider.
   * @private
   */
  updateKnobs_: function() {
    if (!this.isReady_ || !this.prefsAvailable() ||
        this.$.sliderBar.offsetWidth == 0) {
      return;
    }
    const startOffsetMinutes = /** @type {number} */ (
        this.getPref('ash.night_light.custom_start_time').value);
    this.updateKnobLeft_(this.$.startKnob, startOffsetMinutes);
    const endOffsetMinutes = /** @type {number} */ (
        this.getPref('ash.night_light.custom_end_time').value);
    this.updateKnobLeft_(this.$.endKnob, endOffsetMinutes);
    this.refresh_();
  },

  /**
   * Updates the absolute left coordinate of the given |knob| based on the time
   * it represents given as an |offsetMinutes| value.
   * @param {HTMLDivElement} knob
   * @param {number} offsetMinutes
   * @private
   */
  updateKnobLeft_: function(knob, offsetMinutes) {
    const offsetAfter6pm =
        (offsetMinutes + TOTAL_MINUTES_PER_DAY - OFFSET_MINUTES_6PM) %
        TOTAL_MINUTES_PER_DAY;
    let ratio = offsetAfter6pm / TOTAL_MINUTES_PER_DAY;

    if (ratio == 0) {
      // If the ratio is 0, then there are two possibilities:
      // - The knob time is 6:00 PM on the left side of the slider.
      // - The knob time is 6:00 PM on the right side of the slider.
      // We need to check the current knob offset ratio to determine which case
      // it is.
      const currentKnobRatio = this.getKnobRatio_(knob);
      ratio = currentKnobRatio > 0.5 ? 1.0 : 0.0;
    } else {
      ratio = this.isRTL_ ? (1.0 - ratio) : ratio;
    }
    knob.style.left = (ratio * this.$.sliderBar.offsetWidth) + 'px';
  },

  /**
   * Refreshes elements of the slider other than the knobs (the label bubbles,
   * and the progress bar).
   * @private
   */
  refresh_: function() {
    // The label bubbles have the same left coordinates as their corresponding
    // knobs.
    this.$.startLabel.style.left = this.$.startKnob.style.left;
    this.$.endLabel.style.left = this.$.endKnob.style.left;

    // In RTL locales, the relative positions of the knobs are flipped for the
    // purpose of calculating the styles of the progress bars below.
    const rtl = this.isRTL_;
    const endKnob = rtl ? this.$.startKnob : this.$.endKnob;
    const startKnob = rtl ? this.$.endKnob : this.$.startKnob;
    const startProgress = rtl ? this.$.endProgress : this.$.startProgress;
    const endProgress = rtl ? this.$.startProgress : this.$.endProgress;

    // The end progress bar starts from either the start knob or the start of
    // the slider (whichever is to its left) and ends at the end knob.
    const endProgressLeft = startKnob.offsetLeft >= endKnob.offsetLeft ?
        '0px' :
        startKnob.style.left;
    endProgress.style.left = endProgressLeft;
    endProgress.style.width =
        (parseFloat(endKnob.style.left) - parseFloat(endProgressLeft)) + 'px';

    // The start progress bar starts at the start knob, and ends at either the
    // end knob or the end of the slider (whichever is to its right).
    const startProgressRight = endKnob.offsetLeft < startKnob.offsetLeft ?
        this.$.sliderBar.offsetWidth :
        endKnob.style.left;
    startProgress.style.left = startKnob.style.left;
    startProgress.style.width =
        (parseFloat(startProgressRight) - parseFloat(startKnob.style.left)) +
        'px';

    this.fixLabelsOverlapIfAny_();
  },

  /**
   * If the label bubbles overlap, this function fixes them by moving the end
   * label up a little.
   * @private
   */
  fixLabelsOverlapIfAny_: function() {
    const startLabel = this.$.startLabel;
    const endLabel = this.$.endLabel;
    const distance = Math.abs(
        parseFloat(startLabel.style.left) - parseFloat(endLabel.style.left));
    // Both knobs have the same width, but the one being dragged is scaled up by
    // 125%.
    if (distance <= (1.25 * startLabel.offsetWidth)) {
      // Shift the end label up so that it doesn't overlap with the start label.
      endLabel.classList.add('end-label-overlap');
    } else {
      endLabel.classList.remove('end-label-overlap');
    }
  },

  /**
   * Given the |prefPath| that corresponds to one knob time, it gets the value
   * of the pref that corresponds to the other knob.
   * @param {string} prefPath
   * @return {number}
   * @private
   */
  getOtherKnobPrefValue_: function(prefPath) {
    if (prefPath == 'ash.night_light.custom_start_time') {
      return /** @type {number} */ (
          this.getPref('ash.night_light.custom_end_time').value);
    }

    return /** @type {number} */ (
        this.getPref('ash.night_light.custom_start_time').value);
  },

  /**
   * Updates the value of the pref and wraps around if necessary.
   *
   * When the |updatedValue| would put the start and end times closer than the
   * minimum distance, the |updatedValue| is changed to maintain the minimum
   * distance.
   *
   * When |fromUserGesture| is true the update source is from a pointer such as
   * a mouse, touch or pen. When the knobs are close, the dragging knob will
   * stay on the same side with respect to the other knob. For example, when the
   * minimum distance is 1 hour, the start knob is at 8:30 am, and the end knob
   * is at 7:00, let's examine what happens if the start knob is dragged past
   * the end knob. At first the start knob values will change past 8:20 and
   * 8:10, all the way up to 8:00. Further movements in the same direction will
   * not change the start knob value until the pointer crosses past the end knob
   * (modulo the bar width). At that point, the start knob value will be updated
   * to 6:00 and remain at 6:00 until the pointer passes the 6:00 location.
   *
   * When |fromUserGesture| is false, the input is coming from a key event. As
   * soon as the |updatedValue| is closer than the minimum distance, the knob
   * is moved to the other side of the other knob. For example, with a minimum
   * distance of 1 hour, the start knob is at 8:00 am, and the end knob is at
   * 7:00, if the start knob value is decreased, then the start knob will be
   * updated to 6:00.
   * @param {number} updatedValue
   * @param {boolean} fromUserGesture
   * @private
   */
  updatePref_: function(updatedValue, fromUserGesture) {
    const prefPath = assert(this.getFocusedKnobPrefPathIfAny_());
    const otherValue = this.getOtherKnobPrefValue_(prefPath);

    const totalMinutes = TOTAL_MINUTES_PER_DAY;
    const minDistance = MIN_KNOBS_DISTANCE_MINUTES;
    if (modulo(otherValue - updatedValue, totalMinutes) < minDistance) {
      updatedValue = otherValue + (fromUserGesture ? -1 : 1) * minDistance;
    } else if (modulo(updatedValue - otherValue, totalMinutes) < minDistance) {
      updatedValue = otherValue + (fromUserGesture ? 1 : -1) * minDistance;
    }

    // The knobs are allowed to wrap around.
    this.setPrefValue(prefPath, modulo(updatedValue, TOTAL_MINUTES_PER_DAY));
  },

  /**
   * @param {Element} knob
   * @returns {?string}
   * @private
   */
  getPrefPath_: function(knob) {
    if (knob == this.$.startKnob) {
      return 'ash.night_light.custom_start_time';
    }

    if (knob == this.$.endKnob) {
      return 'ash.night_light.custom_end_time';
    }

    return null;
  },

  /**
   * @param {Element} knob
   * @returns {?number}
   * @private
   */
  getPrefValue_: function(knob) {
    const path = this.getPrefPath_(knob);
    return path ? /** @type {number} */ (this.getPref(path).value) : null;
  },

  /**
   * Gets the pref path of the currently focused knob. Returns null if no knob
   * is currently focused.
   * @return {?string}
   * @private
   */
  getFocusedKnobPrefPathIfAny_: function() {
    return this.getPrefPath_(this.shadowRoot.activeElement);
  },

  /**
   * @return {boolean} Whether either of the two knobs is focused.
   * @private
   */
  isEitherKnobFocused_: function() {
    const activeElement = this.shadowRoot.activeElement;
    return activeElement == this.$.startKnob || activeElement == this.$.endKnob;
  },

  /**
   * Overrides _createRipple() from PaperRippleBehavior to create the ripple
   * only on a knob if it's focused, or on a dummy hidden element so that it
   * doesn't show.
   * @protected
   */
  _createRipple: function() {
    if (this.isEitherKnobFocused_()) {
      this._rippleContainer = this.shadowRoot.activeElement;
    } else {
      // We can't just skip the ripple creation and return early with null here.
      // The code inherited from PaperRippleBehavior expects that this function
      // returns a ripple element. So to avoid crashes, we'll setup the ripple
      // to be created under a hidden element.
      this._rippleContainer = this.$.dummyRippleContainer;
    }
    const ripple = Polymer.PaperRippleBehavior._createRipple();
    ripple.id = 'ink';
    ripple.setAttribute('recenters', '');
    ripple.classList.add('circle', 'toggle-ink');
    return ripple;
  },

  /**
   * @param {!Event} event
   * @private
   */
  onFocus_: function(event) {
    this.handleKnobEvent_(event);
  },

  /**
   * Handles focus, drag and key events on the start and end knobs.
   * If |overrideElement| is provided, it will be the knob that gains focus and
   * and the ripple. Otherwise, the knob is determined from the |event|.
   * @param {!Event} event
   * @param {Element=} overrideElement
   * @private
   */
  handleKnobEvent_: function(event, overrideElement) {
    const knob = overrideElement ||
        event.path.find(el => el.classList && el.classList.contains('knob'));
    if (!knob) {
      event.preventDefault();
      return;
    }

    if (this._rippleContainer != knob) {
      this.removeRipple_();
      knob.focus();
    }

    this.ensureRipple();

    if (this.hasRipple()) {
      this._ripple.style.display = '';
      this._ripple.holdDown = true;
    }
  },

  /**
   * Handles blur events on the start and end knobs.
   * @private
   */
  onBlur_: function() {
    this.removeRipple_();
  },

  /**
   * Removes ripple if one exists.
   * @private
   */
  removeRipple_: function() {
    if (this.hasRipple()) {
      this._ripple.remove();
      this._ripple = null;
    }
  },

  /**
   * @param {!Event} event
   * @private
   */
  onKeyDown_: function(event) {
    const activeElement = this.shadowRoot.activeElement;
    if (event.key == 'Tab') {
      if (event.shiftKey && this.$.endKnob == activeElement) {
        event.preventDefault();
        this.handleKnobEvent_(event, this.$.startKnob);
        return;
      }

      if (!event.shiftKey && this.$.startKnob == activeElement) {
        event.preventDefault();
        this.handleKnobEvent_(event, this.$.endKnob);
      }
      return;
    }

    if (event.metaKey || event.shiftKey || event.altKey || event.ctrlKey) {
      return;
    }

    const deltaKeyMap = {
      ArrowDown: -1,
      ArrowLeft: this.isRTL_ ? 1 : -1,
      ArrowRight: this.isRTL_ ? -1 : 1,
      ArrowUp: 1,
      PageDown: -15,
      PageUp: 15,
    };

    if (event.key in deltaKeyMap) {
      this.handleKnobEvent_(event);

      event.preventDefault();
      const value = this.getPrefValue_(activeElement);
      if (value == null) {
        return;
      }

      const delta = deltaKeyMap[event.key];
      this.updatePref_(value + delta, false);
    }
  },
});
})();
