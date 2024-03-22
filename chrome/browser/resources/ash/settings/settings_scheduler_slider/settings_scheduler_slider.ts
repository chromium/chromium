// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview settings-scheduler-slider is used to set the custom automatic
 * schedule of the Night Light feature, so that users can set their desired
 * start and end times.
 */

import '../settings_shared.css.js';

import {PrefsMixin, PrefsMixinInterface} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin, I18nMixinInterface} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {IronResizableBehavior} from 'chrome://resources/polymer/v3_0/iron-resizable-behavior/iron-resizable-behavior.js';
import {PaperRippleMixin, PaperRippleMixinInterface} from 'chrome://resources/polymer/v3_0/paper-behaviors/paper-ripple-mixin.js';
import {PaperRippleElement} from 'chrome://resources/polymer/v3_0/paper-ripple/paper-ripple.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Constructor} from '../common/types.js';

import {getTemplate} from './settings_scheduler_slider.html.js';

export interface SettingsSchedulerSliderElement {
  $: {
    dummyRippleContainer: HTMLDivElement,
    endKnob: HTMLDivElement,
    endLabel: HTMLDivElement,
    endProgress: HTMLDivElement,
    markersContainer: HTMLDivElement,
    sliderBar: HTMLDivElement,
    sliderContainer: HTMLDivElement,
    startKnob: HTMLDivElement,
    startLabel: HTMLDivElement,
    startProgress: HTMLDivElement,
  };
}

type TrackEvent = CustomEvent<{
  state: string,
  x: number,
  y: number,
  dx: number,
  dy: number,
  ddx: number,
  ddy: number,
}>;

const HOURS_PER_DAY = 24;
const MIN_KNOBS_DISTANCE_MINUTES = 60;
const OFFSET_MINUTES_6PM = 18 * 60;
const TOTAL_MINUTES_PER_DAY = 24 * 60;
const DEFAULT_CUSTOM_START_TIME = 18 * 60;
const DEFAULT_CUSTOM_END_TIME = 6 * 60;

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
 */
function modulo(x: number, y: number): number {
  return ((x % y) + y) % y;
}

const SettingsSchedulerSliderElementBase =
    mixinBehaviors(
        [IronResizableBehavior],
        PaperRippleMixin(PrefsMixin(I18nMixin(PolymerElement)))) as
    Constructor<PolymerElement&I18nMixinInterface&PrefsMixinInterface&
                IronResizableBehavior&PaperRippleMixinInterface>;

export class SettingsSchedulerSliderElement extends
    SettingsSchedulerSliderElementBase {
  static get is() {
    return 'settings-scheduler-slider';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The start time pref object being tracked.
       */
      prefStartTime: {
        type: Object,
        notify: true,
        value() {
          return {
            key: 'ash.fake_feature.custom_start_time',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: DEFAULT_CUSTOM_START_TIME,
          };
        },
      },

      /**
       * The end time pref object being tracked.
       */
      prefEndTime: {
        type: Object,
        notify: true,
        value() {
          return {
            key: 'ash.fake_feature.custom_end_time',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: DEFAULT_CUSTOM_END_TIME,
          };
        },
      },

      /**
       * Whether the element is ready and fully rendered.
       */
      isReady_: Boolean,

      /**
       * Whether the window is in RTL locales.
       */
      isRTL_: Boolean,

      /**
       * Whether to use the 24-hour format for the time shown in the label
       * bubbles.
       */
      shouldUse24Hours_: Boolean,
    };
  }

  static get observers() {
    return [
      'updateKnobs_(prefs.*, isRTL_, isReady_)',
      'hourFormatChanged_(prefs.settings.clock.use_24hour_clock.*)',
      'updateMarkers_(prefs.*, isRTL_, isReady_)',
    ];
  }

  prefStartTime: chrome.settingsPrivate.PrefObject<number>;
  prefEndTime: chrome.settingsPrivate.PrefObject<number>;
  private dragObject_: HTMLElement|null;
  private isReady_: boolean;
  private isRTL_: boolean;
  /* eslint-disable-next-line @typescript-eslint/naming-convention */
  private _ripple: PaperRippleElement|null;
  private shouldUse24Hours_: boolean;
  private valueAtDragStart_?: number;

  constructor() {
    super();

    /**
     * The object currently being dragged. Either the start or end knobs.
     */
    this.dragObject_ = null;
  }

  override ready(): void {
    super.ready();

    this.addEventListener('iron-resize', this.onResize_);
    this.addEventListener('focus', this.onFocus_);
    this.addEventListener('blur', this.onBlur_);
    this.addEventListener('keydown', this.onKeyDown_);
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.isRTL_ = window.getComputedStyle(this).direction === 'rtl';
    this.$.sliderContainer.addEventListener('contextmenu', (e) => {
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
  }

  private prefsAvailable_(): boolean {
    return [this.prefStartTime, this.prefEndTime].every(
        pref => pref !== undefined);
  }

  private updateMarkers_(): void {
    if (!this.isReady_ || !this.prefsAvailable_()) {
      return;
    }

    const startHour = this.prefStartTime.value / 60.0;
    const endHour = this.prefEndTime.value / 60.0;

    const markersContainer = this.$.markersContainer;
    markersContainer.innerHTML = window.trustedTypes!.emptyHTML;
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
  }

  /**
   * Return true if the start knob is focused.
   */
  private isStartKnobFocused_(): boolean {
    return this.shadowRoot!.activeElement === this.$.startKnob;
  }

  /**
   * Return true if the end knob is focused.
   */
  private isEndKnobFocused_(): boolean {
    return this.shadowRoot!.activeElement === this.$.endKnob;
  }

  /**
   * Return whether either of the two knobs is focused.
   */
  private isEitherKnobFocused_(): boolean {
    return this.isStartKnobFocused_() || this.isEndKnobFocused_();
  }

  /**
   * Invoked when the element is resized and the knobs positions need to be
   * updated.
   */
  private onResize_(): void {
    this.updateKnobs_();
  }

  /**
   * Called when the value of the pref associated with whether to use the
   * 24-hour clock format is changed. This will also refresh the slider.
   */
  private hourFormatChanged_(): void {
    this.shouldUse24Hours_ =
        this.getPref<boolean>('settings.clock.use_24hour_clock').value;
  }

  /**
   * Gets the style of legend div determining its absolute left position.
   * @param percent The value of the div's left as a percent (0 - 100).
   * @param isRTL whether window is in RTL locale.
   * @return The CSS style of the legend div.
   */
  private getLegendStyle_(percent: number, isRTL: boolean): string {
    const percentage = isRTL ? 100 - percent : percent;
    return `left: ${percentage}%`;
  }

  /**
   * Gets the aria label for the start time knob.
   * @return The start time string to be announced.
   */
  private getAriaLabelStartTime_(): string {
    return this.i18n(
        'startTime',
        this.getTimeString_(this.prefStartTime.value, this.shouldUse24Hours_));
  }

  /**
   * Gets the aria label for the end time knob.
   * @return The end time string to be announced.
   */
  private getAriaLabelEndTime_(): string {
    return this.i18n(
        'endTime',
        this.getTimeString_(this.prefEndTime.value, this.shouldUse24Hours_));
  }


  /**
   * If one of the two knobs is focused, this function blurs it.
   */
  private blurAnyFocusedKnob_(): void {
    if (this.isEitherKnobFocused_()) {
      (this.shadowRoot!.activeElement as HTMLElement).blur();
    }
  }

  /**
   * Start dragging the target knob.
   */
  private startDrag_(event: Event): void {
    event.preventDefault();

    // Only handle start or end knobs. Use the "knob-inner" divs just to display
    // the knobs.
    if (event.target === this.$.startKnob ||
        event.target === this.$.startKnob.firstElementChild) {
      this.dragObject_ = this.$.startKnob;
      this.valueAtDragStart_ = this.prefStartTime.value;
    } else if (
        event.target === this.$.endKnob ||
        event.target === this.$.endKnob.firstElementChild) {
      this.dragObject_ = this.$.endKnob;
      this.valueAtDragStart_ = this.prefEndTime.value;
    } else {
      return;
    }

    this.handleKnobEvent_(event, this.dragObject_);
  }

  /**
   * Continues dragging the selected knob if any.
   */
  private continueDrag_(event: TrackEvent): void {
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
  }

  /**
   * Converts horizontal pixels into number of minutes.
   */
  private getDeltaMinutes_(deltaX: number): number {
    return (this.isRTL_ ? -1 : 1) *
        Math.floor(
            TOTAL_MINUTES_PER_DAY * deltaX / this.$.sliderBar.offsetWidth);
  }

  /**
   * Updates the knob's corresponding pref value in response to dragging, which
   * will in turn update the location of the knob and its corresponding label
   * bubble and its text contents.
   */
  private doKnobTracking_(event: TrackEvent): void {
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
        this.valueAtDragStart_! + this.getDeltaMinutes_(event.detail.dx), true);
  }

  /**
   * Ends the dragging.
   */
  private endDrag_(event: TrackEvent): void {
    event.preventDefault();
    this.dragObject_ = null;
    this.removeRipple_();
  }

  /**
   * Gets the given knob's offset ratio with respect to its parent element
   * (which is the slider bar).
   * @param knob Either one of the two knobs.
   */
  private getKnobRatio_(knob: HTMLElement): number {
    return parseFloat(knob.style.left) / this.$.sliderBar.offsetWidth;
  }

  /**
   * Converts the time of day, given as |hour| and |minutes|, to its language-
   * sensitive time string representation.
   * @param hour The hour of the day (0 - 23).
   * @param minutes The minutes of the hour (0 - 59).
   * @param shouldUse24Hours Whether to use the 24-hour time format.
   */
  private getLocaleTimeString_(
      hour: number, minutes: number, shouldUse24Hours: boolean): string {
    const d = new Date();
    d.setHours(hour);
    d.setMinutes(minutes);
    d.setSeconds(0);
    d.setMilliseconds(0);

    return d.toLocaleTimeString(
        navigator.language,
        {hour: 'numeric', minute: 'numeric', hour12: !shouldUse24Hours});
  }

  /**
   * Converts the |offsetMinutes| value (which the number of minutes since
   * 00:00) to its language-sensitive time string representation.
   * @param offsetMinutes The time of day represented as the number of
   *    minutes from 00:00.
   * @param shouldUse24Hours Whether to use the 24-hour time format.
   */
  private getTimeString_(offsetMinutes: number, shouldUse24Hours: boolean):
      string {
    const hour = Math.floor(offsetMinutes / 60);
    const minute = Math.floor(offsetMinutes % 60);
    return this.getLocaleTimeString_(hour, minute, shouldUse24Hours);
  }

  /**
   * Using the current start and end times prefs, this function updates the
   * knobs and their label bubbles and refreshes the slider.
   */
  private updateKnobs_(): void {
    if (!this.isReady_ || !this.prefsAvailable_() ||
        this.$.sliderBar.offsetWidth === 0) {
      return;
    }

    const startOffsetMinutes: number = this.prefStartTime.value;
    this.updateKnobLeft_(this.$.startKnob, startOffsetMinutes);

    const endOffsetMinutes: number = this.prefEndTime.value;
    this.updateKnobLeft_(this.$.endKnob, endOffsetMinutes);

    this.refresh_();
  }

  /**
   * Updates the absolute left coordinate of the given |knob| based on the time
   * it represents given as an |offsetMinutes| value.
   */
  private updateKnobLeft_(knob: HTMLElement, offsetMinutes: number): void {
    const offsetAfter6pm =
        (offsetMinutes + TOTAL_MINUTES_PER_DAY - OFFSET_MINUTES_6PM) %
        TOTAL_MINUTES_PER_DAY;
    let ratio = offsetAfter6pm / TOTAL_MINUTES_PER_DAY;

    if (ratio === 0) {
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
  }

  /**
   * Refreshes elements of the slider other than the knobs (the label bubbles,
   * and the progress bar).
   */
  private refresh_(): void {
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
    const endProgressLeft: number = startKnob.offsetLeft >= endKnob.offsetLeft ?
        0 :
        parseFloat(startKnob.style.left);
    endProgress.style.left = `${endProgressLeft}px`;
    endProgress.style.width =
        `${parseFloat(endKnob.style.left) - endProgressLeft}px`;

    // The start progress bar starts at the start knob, and ends at either the
    // end knob or the end of the slider (whichever is to its right).
    const startProgressRight: number =
        endKnob.offsetLeft < startKnob.offsetLeft ?
        this.$.sliderBar.offsetWidth :
        parseFloat(endKnob.style.left);
    startProgress.style.left = startKnob.style.left;
    startProgress.style.width =
        `${startProgressRight - parseFloat(startKnob.style.left)}px`;

    this.fixLabelsOverlapIfAny_();
  }

  /**
   * If the label bubbles overlap, this function fixes them by moving the end
   * label up a little.
   */
  private fixLabelsOverlapIfAny_(): void {
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
  }

  /**
   * Return the value of the pref that corresponds to the other knob than
   * `this.shadowRoot!.activeElement`
   */
  private getOtherKnobPrefValue_(): number {
    if (this.isStartKnobFocused_()) {
      return this.prefEndTime.value;
    }
    return this.prefStartTime.value;
  }

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
   */
  private updatePref_(updatedValue: number, fromUserGesture: boolean): void {
    const otherValue = this.getOtherKnobPrefValue_();

    const totalMinutes = TOTAL_MINUTES_PER_DAY;
    const minDistance = MIN_KNOBS_DISTANCE_MINUTES;
    if (modulo(otherValue - updatedValue, totalMinutes) < minDistance) {
      updatedValue = otherValue + (fromUserGesture ? -1 : 1) * minDistance;
    } else if (modulo(updatedValue - otherValue, totalMinutes) < minDistance) {
      updatedValue = otherValue + (fromUserGesture ? 1 : -1) * minDistance;
    }

    // The knobs are allowed to wrap around.
    if (this.isStartKnobFocused_()) {
      this.set(
          'prefStartTime.value', modulo(updatedValue, TOTAL_MINUTES_PER_DAY));
    } else if (this.isEndKnobFocused_()) {
      this.set(
          'prefEndTime.value', modulo(updatedValue, TOTAL_MINUTES_PER_DAY));
    }
  }

  private getPrefValue_(): number|null {
    if (this.isStartKnobFocused_()) {
      return this.prefStartTime.value;
    } else if (this.isEndKnobFocused_()) {
      return this.prefEndTime.value;
    } else {
      return null;
    }
  }

  /**
   * Overrides _createRipple() from PaperRippleMixin to create the ripple
   * only on a knob if it's focused, or on a dummy hidden element so that it
   * doesn't show.
   */
  /* eslint-disable-next-line @typescript-eslint/naming-convention */
  override _createRipple(): PaperRippleElement {
    if (this.isEitherKnobFocused_()) {
      this._rippleContainer = this.shadowRoot!.activeElement as HTMLElement;
    } else {
      // We can't just skip the ripple creation and return early with null here.
      // The code inherited from PaperRippleMixin expects that this function
      // returns a ripple element. So to avoid crashes, we'll setup the ripple
      // to be created under a hidden element.
      this._rippleContainer = this.$.dummyRippleContainer;
    }
    const ripple = super._createRipple();
    ripple.id = 'ink';
    ripple.setAttribute('recenters', '');
    ripple.classList.add('circle');
    return ripple;
  }

  private onFocus_(event: Event): void {
    this.handleKnobEvent_(event);
  }

  /**
   * Handles focus, drag and key events on the start and end knobs.
   * If |overrideElement| is provided, it will be the knob that gains focus and
   * and the ripple. Otherwise, the knob is determined from the |event|.
   */
  private handleKnobEvent_(event: Event, overrideElement?: HTMLElement|null):
      void {
    const knob = overrideElement ||
        (event.composedPath().find(
            el => (el as HTMLElement).classList?.contains('knob'))) as
                HTMLElement |
            undefined;
    if (!knob) {
      event.preventDefault();
      return;
    }

    if (this._rippleContainer !== knob) {
      this.removeRipple_();
      knob.focus();
    }

    this.ensureRipple();

    if (this.hasRipple()) {
      this._ripple!.style.display = '';
      this._ripple!.holdDown = true;
    }
  }

  /**
   * Handles blur events on the start and end knobs.
   */
  private onBlur_(): void {
    this.removeRipple_();
  }

  /**
   * Removes ripple if one exists.
   */
  private removeRipple_(): void {
    if (this.hasRipple()) {
      this._ripple!.remove();
      this._ripple = null;
    }
  }

  private onKeyDown_(event: KeyboardEvent): void {
    if (event.key === 'Tab') {
      if (event.shiftKey && this.isEndKnobFocused_()) {
        event.preventDefault();
        this.handleKnobEvent_(event, this.$.startKnob);
        return;
      }

      if (!event.shiftKey && this.isStartKnobFocused_()) {
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
      const value = this.getPrefValue_();
      if (value === null) {
        return;
      }

      const delta = deltaKeyMap[event.key as keyof typeof deltaKeyMap];
      this.updatePref_(value + delta, false);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-scheduler-slider': SettingsSchedulerSliderElement;
  }
}

customElements.define(
    SettingsSchedulerSliderElement.is, SettingsSchedulerSliderElement);
