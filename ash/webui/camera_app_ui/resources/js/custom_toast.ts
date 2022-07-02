// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as animation from './animation.js';
import {assert, assertExists, assertNotReached} from './assert.js';
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
   * @param anchor Element to show ripple effect on.
   */
  constructor(private readonly anchor: HTMLElement) {
    const style = this.anchor.computedStyleMap();

    this.width = util.getStyleValueInPx(style, '--ripple-start-width');
    this.height = util.getStyleValueInPx(style, '--ripple-start-height');
    this.cancelHandle = setInterval(() => {
      this.addRipple();
    }, RIPPLE_INTERVAL_MS);

    this.addRipple();
  }

  private async addRipple(): Promise<void> {
    const rect = this.anchor.getBoundingClientRect();
    if (rect.width === 0) {
      return;
    }
    const template = util.instantiateTemplate('#ripple-template');
    const ripple = dom.getFrom(template, '.ripple', HTMLDivElement);
    const style = ripple.attributeStyleMap;
    style.set('left', CSS.px(rect.left - (this.width - rect.width) / 2));
    style.set('top', CSS.px(rect.top - (this.height - rect.height) / 2));
    style.set('width', CSS.px(this.width));
    style.set('height', CSS.px(this.height));
    document.body.appendChild(template);
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

type PositionProperties = Array<{
  elProperty: PositionProperty,
  toastProperty: PositionProperty,
  offset: number,
}>;

type PositionInfos = Array<{
  target: HTMLElement,
  properties: PositionProperties,
}>;

export enum IndicatorType {
  DOWNLOAD_DOCUMENT_SCANNER = 'download_document_scanner',
}

function getIndicatorI18nStringId(indicatorType: IndicatorType): I18nString {
  switch (indicatorType) {
    case IndicatorType.DOWNLOAD_DOCUMENT_SCANNER:
      return I18nString.DOWNLOADING_DOCUMENT_SCANNING_FEATURE;
    default:
      assertNotReached();
  }
}

function getIndicatorIcon(indicatorType: IndicatorType): string|null {
  switch (indicatorType) {
    case IndicatorType.DOWNLOAD_DOCUMENT_SCANNER:
      return '/images/download_dlc_toast_icon.svg';
    default:
      return null;
  }
}

function getOffsetProperties(
    element: HTMLElement, prefix: string): PositionProperties {
  const properties = [];
  const style = element.computedStyleMap();

  function getPositionProperty(key: string) {
    const property = assertExists(style.get(key)).toString();
    return util.assertEnumVariant(PositionProperty, property);
  }

  for (const dir of ['x', 'y']) {
    const toastProperty = getPositionProperty(`--${prefix}-ref-${dir}`);
    const elProperty = getPositionProperty(`--${prefix}-element-ref-${dir}`);
    const offset = util.getStyleValueInPx(style, `--${prefix}-offset-${dir}`);
    properties.push({elProperty, toastProperty, offset});
  }
  return properties;
}

function updatePositions(anchor: HTMLElement, infos: PositionInfos): void {
  for (const {target, properties} of infos) {
    updatePosition(anchor, target, properties);
  }
}

function updatePosition(
    anchor: HTMLElement, targetElement: HTMLElement,
    properties: PositionProperties): void {
  const rect = anchor.getBoundingClientRect();
  const style = targetElement.attributeStyleMap;
  if (rect.width === 0) {
    style.set('display', 'none');
    return;
  }
  style.clear();
  for (const {elProperty, toastProperty, offset} of properties) {
    let value = rect[elProperty] + offset;
    if (toastProperty === PositionProperty.RIGHT) {
      value = window.innerWidth - value;
    } else if (toastProperty === PositionProperty.BOTTOM) {
      value = window.innerHeight - value;
    }
    style.set(toastProperty, CSS.px(value));
  }
}

/**
 * Controller for showing a toast.
 */
class Toast {
  protected cancelHandle: number;

  constructor(
      protected readonly anchor: HTMLElement,
      protected readonly template: DocumentFragment,
      protected readonly toast: HTMLDivElement,
      protected readonly positionInfos: PositionInfos) {
    this.cancelHandle = setInterval(() => {
      updatePositions(anchor, positionInfos);
    }, TOAST_POSITION_UPDATE_MS);
    updatePositions(anchor, positionInfos);
  }

  show(): void {
    document.body.appendChild(this.template);
  }

  focus(): void {
    this.anchor.setAttribute('aria-owns', 'new-feature-toast');
    this.toast.focus();
  }

  hide(): void {
    this.anchor.removeAttribute('aria-owns');
    clearInterval(this.cancelHandle);
    for (const {target} of this.positionInfos) {
      document.body.removeChild(target);
    }
  }
}

class NewFeatureToast extends Toast {
  constructor(anchor: HTMLElement) {
    const template = util.instantiateTemplate('#new-feature-toast-template');
    const toast = dom.getFrom(template, '#new-feature-toast', HTMLDivElement);

    const i18nId = util.assertEnumVariant(
        I18nString, anchor.getAttribute('i18n-new-feature'));
    const textElement =
        dom.getFrom(template, '.custom-toast-text', HTMLSpanElement);
    const text = loadTimeData.getI18nMessage(i18nId);
    textElement.textContent = text;
    const ariaLabel =
        loadTimeData.getI18nMessage(I18nString.NEW_CONTROL_NAVIGATION, text);
    toast.setAttribute('aria-label', ariaLabel);

    super(anchor, template, toast, [{
            target: toast,
            properties: getOffsetProperties(anchor, 'toast'),
          }]);
  }
}

class IndicatorToast extends Toast {
  constructor(anchor: HTMLElement, indicatorType: IndicatorType) {
    const template = util.instantiateTemplate('#indicator-toast-template');
    const toast = dom.getFrom(template, '#indicator-toast', HTMLDivElement);

    const i18nId = getIndicatorI18nStringId(indicatorType);
    const textElement =
        dom.getFrom(template, '.custom-toast-text', HTMLSpanElement);
    const text = loadTimeData.getI18nMessage(i18nId);
    textElement.textContent = text;
    toast.setAttribute('aria-label', text);

    const icon = getIndicatorIcon(indicatorType);
    const iconElement =
        dom.getFrom(template, '#indicator-icon', HTMLImageElement);
    if (icon === null) {
      iconElement.hidden = true;
    } else {
      iconElement.src = icon;
      iconElement.hidden = false;
    }

    const indicatorDot =
        dom.getFrom(template, '#indicator-dot', HTMLDivElement);
    super(anchor, template, toast, [
      {
        target: toast,
        properties: getOffsetProperties(anchor, 'toast'),
      },
      {
        target: indicatorDot,
        properties: getOffsetProperties(anchor, 'indicator-dot'),
      },
    ]);
  }
}

let showing: {
  ripple: RippleEffect|null,
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
  if (ripple !== null) {
    ripple.stop();
  }
  toast.hide();
  clearTimeout(timeout);
}

/**
 * Timeout for showing new feature toast.
 */
const SHOWING_TIMEOUT_MS = 10000;

/**
 * Shows the new feature toast message around the `anchor` element. The message
 * to show is defined in HTML attribute and the relative position is defined in
 * CSS.
 */
export function showNewFeatureToast(anchor: HTMLElement): void {
  return show(new NewFeatureToast(anchor), new RippleEffect(anchor));
}

/**
 * Shows the indicator toast message and an indicator dot around the `anchor`
 * element. The message to show is given by `indicatorType` and the relative
 * position of the toast and dot are defined in CSS.
 */
export function showIndicatorToast(
    anchor: HTMLElement, indicatorType: IndicatorType): void {
  assert(indicatorType !== undefined);
  return show(new IndicatorToast(anchor, indicatorType));
}

/**
 * Shows the toast on the given element.
 */
function show(toast: Toast, ripple: RippleEffect|null = null): void {
  hide();

  const timeout = setTimeout(hide, SHOWING_TIMEOUT_MS);
  showing = {ripple, toast, timeout};
}

/**
 * @return If toast is showing.
 */
export function isShowing(): boolean {
  return showing !== null;
}

/**
 * Focuses to toast.
 */
export function focus(): void {
  if (showing === null) {
    return;
  }
  showing.toast.focus();
}
