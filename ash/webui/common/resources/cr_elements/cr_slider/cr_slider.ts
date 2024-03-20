// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-slider' is a slider component used to select a number from
 * a continuous or discrete range of numbers.
 *
 * Forked from ui/webui/resources/cr_elements/cr_slider/cr_slider.ts
 */
import '../cr_hidden_style.css.js';
import '../cr_shared_vars.css.js';

import {assert} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {PaperRippleMixin} from '//resources/polymer/v3_0/paper-behaviors/paper-ripple-mixin.js';
import {Debouncer, microTask, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cr_slider.html.js';

/**
 * The |value| is the corresponding value that the current slider tick is
 * associated with. The string |label| is shown in the UI as the label for the
 * current slider value. The |ariaValue| number is used for aria-valuemin,
 * aria-valuemax, and aria-valuenow, and is optional. If missing, |value| will
 * be used instead.
 */
export interface SliderTick {
  value: number;
  label: string;
  ariaValue?: number;
}

function clamp(min: number, max: number, value: number): number {
  return Math.min(max, Math.max(min, value));
}

function getAriaValue(tick: SliderTick|number): number {
  if (Number.isFinite(tick as number)) {
    return tick as number;
  }

  const sliderTick = tick as SliderTick;
  return sliderTick.ariaValue !== undefined ? sliderTick.ariaValue! :
                                              sliderTick.value;
}

const CrSliderElementBase = PaperRippleMixin(PolymerElement);

/**
 * The following are the events emitted from cr-slider.
 *
 * cr-slider-value-changed: fired when updating slider via the UI.
 * dragging-changed: fired on pointer down and on pointer up.
 */

export interface CrSliderElement {
  $: {
    bar: HTMLElement,
    container: HTMLElement,
    knobAndLabel: HTMLElement,
    knob: HTMLElement,
  };
}

export class CrSliderElement extends CrSliderElementBase {
  static get is() {
    return 'cr-slider';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      disabled: {
        type: Boolean,
        value: false,
      },

      /**
       * Internal representation of disabled depending on |disabled| and
       * |ticks|.
       */
      disabled_: {
        type: Boolean,
        computed: 'computeDisabled_(disabled, ticks.*)',
        reflectToAttribute: true,
        observer: 'onDisabledChanged_',
      },

      dragging: {
        type: Boolean,
        value: false,
        notify: true,
      },

      updatingFromKey: {
        type: Boolean,
        value: false,
        notify: true,
      },

      /**
       * The amount the slider value increments by when pressing any of the keys
       * from `deltaKeyMap_`. Defaults to 1.
       */
      keyPressSliderIncrement: {
        type: Number,
        value: 1,
      },

      markerCount: {
        type: Number,
        value: 0,
      },

      max: {
        type: Number,
        value: 100,
      },

      min: {
        type: Number,
        value: 0,
      },

      /**
       * When set to false, the keybindings are not handled by this component,
       * for example when the owner of the component wants to set up its own
       * keybindings.
       */
      noKeybindings: {
        type: Boolean,
        value: false,
      },

      snaps: {
        type: Boolean,
        value: false,
      },

      /**
       * The data associated with each tick on the slider. Each element in the
       * array contains a value and the label corresponding to that value.
       */
      ticks: {
        type: Array,
        value: () => [],
      },

      value: Number,

      label_: {
        type: String,
        value: '',
      },

      showLabel_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      isRtl_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /**
       * |transiting_| is set to true when bar is touched or clicked. This
       * triggers a single position transition effect to take place for the
       * knob, bar and label. When the transition is complete, |transiting_| is
       * set to false resulting in no transition effect during dragging, manual
       * value updates and keyboard events.
       */
      transiting_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
    };
  }

  static get observers() {
    return [
      'onTicksChanged_(ticks.*)',
      'updateUi_(ticks.*, value, min, max)',
      'onValueMinMaxChange_(value, min, max)',
      'buildDeltaKeyMap_(isRtl_, keyPressSliderIncrement)',
    ];
  }

  disabled: boolean;
  dragging: boolean;
  updatingFromKey: boolean;
  keyPressSliderIncrement: number;
  markerCount: number;
  max: number;
  min: number;
  noKeybindings: boolean;
  snaps: boolean;
  ticks: SliderTick[]|number[];
  value: number;

  private disabled_: boolean;
  private label_: string;
  private showLabel_: boolean;
  private isRtl_: boolean;
  private transiting_: boolean;

  private deltaKeyMap_: Map<string, number>|null = null;
  private draggingEventTracker_: EventTracker|null = null;
  private debouncer_: Debouncer;

  /* eslint-disable-next-line @typescript-eslint/naming-convention */
  override _rippleContainer: Element;

  override ready() {
    super.ready();
    this.setAttribute('role', 'slider');

    this.addEventListener('blur', this.hideRipple_);
    this.addEventListener('focus', this.showRipple_);
    this.addEventListener('keydown', this.onKeyDown_);
    this.addEventListener('keyup', this.onKeyUp_);
    this.addEventListener('pointerdown', this.onPointerDown_.bind(this));
  }

  override connectedCallback() {
    super.connectedCallback();
    this.isRtl_ = window.getComputedStyle(this)['direction'] === 'rtl';
    this.draggingEventTracker_ = new EventTracker();
  }

  private fire_(eventName: string, detail?: any) {
    this.dispatchEvent(
        new CustomEvent(eventName, {bubbles: true, composed: true, detail}));
  }

  private computeDisabled_(): boolean {
    return this.disabled || this.ticks.length === 1;
  }

  /**
   * When markers are displayed on the slider, they are evenly spaced across
   * the entire slider bar container and are rendered on top of the bar and
   * bar container. The location of the marks correspond to the discrete
   * values that the slider can have.
   * @return The array items have no type since this is used to
   *     create |markerCount| number of markers.
   */
  private getMarkers_<T>(): T[] {
    return new Array(Math.max(0, this.markerCount - 1));
  }

  private getMarkerClass_(index: number): string {
    const currentStep = (this.markerCount - 1) * this.getRatio();
    return index < currentStep ? 'active-marker' : 'inactive-marker';
  }

  /**
   * The ratio is a value from 0 to 1.0 corresponding to a location along the
   * slider bar where 0 is the minimum value and 1.0 is the maximum value.
   * This is a helper function used to calculate the bar width, knob location
   * and label location.
   */
  getRatio(): number {
    return (this.value - this.min) / (this.max - this.min);
  }

  /**
   * Removes all event listeners related to dragging, and cancels ripple.
   */
  private stopDragging_(pointerId: number) {
    this.draggingEventTracker_!.removeAll();
    this.releasePointerCapture(pointerId);
    this.dragging = false;
    this.hideRipple_();
  }

  private hideRipple_() {
    if (this.noink) {
      return;
    }

    this.getRipple().clear();
    this.showLabel_ = false;
  }

  private showRipple_() {
    if (this.noink) {
      return;
    }

    if (!this.getRipple().holdDown) {
      this.getRipple().showAndHoldDown();
    }
    this.showLabel_ = true;
  }

  private onDisabledChanged_() {
    this.setAttribute('tabindex', this.disabled_ ? '-1' : '0');
    this.blur();
  }

  private onKeyDown_(event: KeyboardEvent) {
    if (this.disabled_ || this.noKeybindings) {
      return;
    }

    if (event.metaKey || event.shiftKey || event.altKey || event.ctrlKey) {
      return;
    }

    let newValue: number|undefined;
    if (event.key === 'Home') {
      newValue = this.min;
    } else if (event.key === 'End') {
      newValue = this.max;
    } else if (this.deltaKeyMap_!.has(event.key)) {
      newValue = this.value + this.deltaKeyMap_!.get(event.key)!;
    }

    if (newValue === undefined) {
      return;
    }

    this.updatingFromKey = true;
    if (this.updateValue_(newValue)) {
      this.fire_('cr-slider-value-changed');
    }
    event.preventDefault();
    event.stopPropagation();
    this.showRipple_();
  }

  private onKeyUp_(event: KeyboardEvent) {
    if (event.key === 'Home' || event.key === 'End' ||
        this.deltaKeyMap_!.has(event.key)) {
      setTimeout(() => {
        this.updatingFromKey = false;
      });
    }
  }

  /**
   * When the left-mouse button is pressed, the knob location is updated and
   * dragging starts.
   */
  private onPointerDown_(event: PointerEvent) {
    if (this.disabled_ ||
        event.buttons !== 1 && event.pointerType === 'mouse') {
      return;
    }

    this.dragging = true;
    this.transiting_ = true;
    this.updateValueFromClientX_(event.clientX);
    this.showRipple_();

    this.setPointerCapture(event.pointerId);
    const stopDragging = this.stopDragging_.bind(this, event.pointerId);

    assert(!!this.draggingEventTracker_);
    this.draggingEventTracker_.add(this, 'pointermove', (e: PointerEvent) => {
      // Prevent unwanted text selection to occur while moving the pointer,
      // this is important.
      e.preventDefault();

      // If the left-button on the mouse is pressed by itself, then update.
      // Otherwise stop capturing the mouse events because the drag operation
      // is complete.
      if (e.buttons !== 1 && e.pointerType === 'mouse') {
        stopDragging();
        return;
      }
      this.updateValueFromClientX_(e.clientX);
    });
    this.draggingEventTracker_.add(this, 'pointercancel', stopDragging);
    this.draggingEventTracker_.add(this, 'pointerdown', stopDragging);
    this.draggingEventTracker_.add(this, 'pointerup', stopDragging);
    this.draggingEventTracker_.add(this, 'keydown', (e: KeyboardEvent) => {
      if (e.key === 'Escape' || e.key === 'Tab' || e.key === 'Home' ||
          e.key === 'End' || this.deltaKeyMap_!.has(e.key)) {
        stopDragging();
      }
    });
  }

  private onTicksChanged_() {
    if (this.ticks.length > 1) {
      this.snaps = true;
      this.max = this.ticks.length - 1;
      this.min = 0;
    }
    if (this.value !== undefined) {
      this.updateValue_(this.value);
    }
  }

  private onTransitionEnd_() {
    this.transiting_ = false;
  }

  private onValueMinMaxChange_() {
    this.debouncer_ = Debouncer.debounce(this.debouncer_, microTask, () => {
      if (this.value === undefined || this.min === undefined ||
          this.max === undefined) {
        return;
      }
      this.updateValue_(this.value);
    });
  }

  private updateUi_() {
    const percent = `${this.getRatio() * 100}%`;
    this.$.bar.style.width = percent;
    this.$.knobAndLabel.style.marginInlineStart = percent;

    const ticks = this.ticks;
    const value = this.value;
    if (ticks && ticks.length > 0 && Number.isInteger(value) && value >= 0 &&
        value < ticks.length) {
      const tick = ticks[this.value]!;
      this.label_ = Number.isFinite(tick) ? '' : (tick as SliderTick).label;
      const ariaValueNow = getAriaValue(tick);
      this.setAttribute('aria-valuetext', String(this.label_ || ariaValueNow));
      this.setAttribute('aria-valuenow', ariaValueNow.toString());
      this.setAttribute('aria-valuemin', getAriaValue(ticks[0]!).toString());
      this.setAttribute(
          'aria-valuemax', getAriaValue(ticks.slice(-1)[0]!).toString());
    } else {
      this.setAttribute(
          'aria-valuetext', value !== undefined ? value.toString() : '');
      this.setAttribute(
          'aria-valuenow', value !== undefined ? value.toString() : '');
      this.setAttribute('aria-valuemin', this.min.toString());
      this.setAttribute('aria-valuemax', this.max.toString());
    }
  }

  private updateValue_(value: number): boolean {
    this.$.container.hidden = false;
    if (this.snaps) {
      // Skip update if |value| has not passed the next value .8 units away.
      // The value will update as the drag approaches the next value.
      if (Math.abs(this.value - value) < .8) {
        return false;
      }
      value = Math.round(value);
    }
    value = clamp(this.min, this.max, value);
    if (this.value === value) {
      return false;
    }
    this.value = value;
    return true;
  }

  private updateValueFromClientX_(clientX: number) {
    const rect = this.$.container.getBoundingClientRect();
    let ratio = (clientX - rect.left) / rect.width;
    if (this.isRtl_) {
      ratio = 1 - ratio;
    }
    if (this.updateValue_(ratio * (this.max - this.min) + this.min)) {
      this.fire_('cr-slider-value-changed');
    }
  }

  private buildDeltaKeyMap_() {
    const increment = this.keyPressSliderIncrement;
    const decrement = -this.keyPressSliderIncrement;
    this.deltaKeyMap_ = new Map([
      ['ArrowDown', decrement],
      ['ArrowUp', increment],
      ['PageDown', decrement],
      ['PageUp', increment],
      ['ArrowLeft', this.isRtl_ ? increment : decrement],
      ['ArrowRight', this.isRtl_ ? decrement : increment],
    ]);
  }

  // Overridden from PaperRippleMixin
  /* eslint-disable-next-line @typescript-eslint/naming-convention */
  override _createRipple() {
    this._rippleContainer = this.$.knob;
    const ripple = super._createRipple();
    ripple.id = 'ink';
    ripple.setAttribute('recenters', '');
    ripple.classList.add('circle');
    return ripple;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-slider': CrSliderElement;
  }
}

customElements.define(CrSliderElement.is, CrSliderElement);
