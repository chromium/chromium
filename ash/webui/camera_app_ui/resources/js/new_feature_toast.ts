// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as animation from './animation.js';
import {assertExists} from './assert.js';
import * as dom from './dom.js';
import {I18nString} from './i18n_string.js';
import * as loadTimeData from './models/load_time_data.js';
import * as util from './util.js';

/**
 * Interval of emerge time between two consecutive ripples in milliseconds.
 */
const RIPPLE_INTERVAL_MS = 5000;

/**
 * Controller for showing ripple effect.
 */
class RippleEffect {
  /**
   * Initial width of ripple in px.
   */
  private readonly width: number;

  /**
   * Initial height of ripple in px.
   */
  private readonly height: number;

  private readonly cancelHandle: number;

  /**
   * @param el Element to show ripple effect on.
   */
  constructor(private readonly el: HTMLElement) {
    const style = this.el.computedStyleMap();

    this.width = util.getStyleValueInPx(style, '--ripple-start-width');
    this.height = util.getStyleValueInPx(style, '--ripple-start-height');
    this.cancelHandle = setInterval(() => {
      this.addRipple();
    }, RIPPLE_INTERVAL_MS);

    this.addRipple();
  }

  private async addRipple(): Promise<void> {
    const rect = this.el.getBoundingClientRect();
    if (rect.width === 0) {
      return;
    }
    const tpl = util.instantiateTemplate('#ripple-template');
    const ripple = dom.getFrom(tpl, '.ripple', HTMLDivElement);
    const style = ripple.attributeStyleMap;
    style.set('left', CSS.px(rect.left - (this.width - rect.width) / 2));
    style.set('top', CSS.px(rect.top - (this.height - rect.height) / 2));
    style.set('width', CSS.px(this.width));
    style.set('height', CSS.px(this.height));
    document.body.appendChild(tpl);
    await animation.play(ripple);
    document.body.removeChild(ripple);
  }

  /**
   * Stops ripple effect.
   */
  stop(): void {
    clearInterval(this.cancelHandle);
  }
}

/**
 * Interval for toast updaing position.
 */
const TOAST_POSITION_UPDATE_MS = 500;

enum PositionProperty {
  BOTTOM = 'bottom',
  LEFT = 'left',
  RIGHT = 'right',
  TOP = 'top',
}

/**
 * Controller for showing new feature toast.
 */
class Toast {
  private readonly offsetProperties: Array<{
    elProperty: PositionProperty,
    toastProperty: PositionProperty,
    offset: number,
  }>;

  private readonly toast: HTMLDivElement;

  private readonly cancelHandle: number;

  constructor(private readonly el: HTMLElement) {
    /**
     * Offset between the position property of toast and the target |el| to
     * determine their relative position.
     */
    this.offsetProperties = (() => {
      const properties = [];
      const style = this.el.computedStyleMap();

      function getPositionProperty(key: string) {
        const property = assertExists(style.get(key)).toString();
        return util.assertEnumVariant(PositionProperty, property);
      }

      for (const dir of ['x', 'y']) {
        const toastProperty = getPositionProperty(`--toast-ref-${dir}`);
        const elProperty = getPositionProperty(`--toast-element-ref-${dir}`);
        const offset = util.getStyleValueInPx(style, `--toast-offset-${dir}`);
        properties.push({elProperty, toastProperty, offset});
      }
      return properties;
    })();

    const tpl = util.instantiateTemplate('#new-feature-toast-template');
    this.toast = dom.getFrom(tpl, '#new-feature-toast', HTMLDivElement);

    this.cancelHandle = setInterval(() => {
      this.updatePosition();
    }, TOAST_POSITION_UPDATE_MS);

    // Set up i18n texts.
    const i18nId =
        util.assertEnumVariant(I18nString, el.getAttribute('i18n-new-feature'));
    const textEl = dom.getFrom(tpl, '.new-feature-toast-text', HTMLSpanElement);
    const text = loadTimeData.getI18nMessage(i18nId);
    textEl.textContent = text;
    const ariaLabel =
        loadTimeData.getI18nMessage(I18nString.NEW_CONTROL_NAVIGATION, text);
    this.toast.setAttribute('aria-label', ariaLabel);

    document.body.appendChild(tpl);
    this.updatePosition();
  }

  private updatePosition() {
    const rect = this.el.getBoundingClientRect();
    const style = this.toast.attributeStyleMap;
    if (rect.width === 0) {
      style.set('display', 'none');
      return;
    }
    style.clear();
    for (const {elProperty, toastProperty, offset} of this.offsetProperties) {
      let value = rect[elProperty] + offset;
      if (toastProperty === PositionProperty.RIGHT) {
        value = window.innerWidth - value;
      } else if (toastProperty === PositionProperty.BOTTOM) {
        value = window.innerHeight - value;
      }
      style.set(toastProperty, CSS.px(value));
    }
  }

  focus(): void {
    this.el.setAttribute('aria-owns', 'new-feature-toast');
    this.toast.focus();
  }

  hide(): void {
    this.el.removeAttribute('aria-owns');
    clearInterval(this.cancelHandle);
    document.body.removeChild(this.toast);
  }
}

let showing: {
  ripple: RippleEffect,
  toast: Toast,
  timeout: number,
}|null = null;

/**
 * Hides the new feature toast.
 */
export function hide(): void {
  if (showing === null) {
    return;
  }
  const {ripple, toast, timeout} = showing;
  showing = null;
  ripple.stop();
  toast.hide();
  clearTimeout(timeout);
}

/**
 * Timeout for showing new feature toast.
 */
const SHOWING_TIMEOUT_MS = 10000;

/**
 * Shows the toast on the given element.
 */
export function show(el: HTMLElement): void {
  if (showing !== null) {
    hide();
  }

  const ripple = new RippleEffect(el);
  const toast = new Toast(el);
  const timeout = setTimeout(hide, SHOWING_TIMEOUT_MS);
  showing = {ripple, toast, timeout};
}

/**
 * @return If new feature toast is showing.
 */
export function isShowing(): boolean {
  return showing !== null;
}

/**
 * Focuses to new feature toast.
 */
export function focus(): void {
  if (showing === null) {
    return;
  }
  showing.toast.focus();
}
