// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from './assert.js';
import * as dom from './dom.js';

/**
 * Wrapper element that shows tooltip.
 */
let wrapper: HTMLElement|null = null;

/**
 * Hovered element whose tooltip to be shown.
 */
let hovered: HTMLElement|null = null;

/**
 * Name of event triggered for positioning tooltip.
 */
export const TOOLTIP_POSITION_EVENT_NAME = 'tooltipposition';

/**
 * Positions tooltip relative to UI.
 *
 * @param rect UI's reference region.
 */
export function position(rect: DOMRectReadOnly): void {
  assert(wrapper !== null);

  const [edgeMargin, elementMargin] = [5, 8];
  let tooltipTop = rect.top - wrapper.offsetHeight - elementMargin;
  if (tooltipTop < edgeMargin) {
    tooltipTop = rect.bottom + elementMargin;
  }
  wrapper.style.top = tooltipTop + 'px';

  // Center over the hovered element but avoid touching edges.
  const hoveredCenter = rect.left + rect.width / 2;
  const left = Math.min(
      Math.max(hoveredCenter - wrapper.clientWidth / 2, edgeMargin),
      document.body.offsetWidth - wrapper.offsetWidth - edgeMargin);
  wrapper.style.left = Math.round(left) + 'px';
}

/**
 * Hides the shown tooltip if any.
 */
export function hide(): void {
  assert(wrapper !== null);

  if (hovered !== null) {
    hovered = null;
    wrapper.textContent = '';
    wrapper.classList.remove('visible');
  }
}

/**
 * Shows a tooltip over the hovered element.
 *
 * @param element Hovered element whose tooltip to be shown.
 */
function show(element: HTMLElement) {
  assert(wrapper !== null);

  hide();
  let message = element.getAttribute('aria-label');
  if (element instanceof HTMLInputElement) {
    if (element.hasAttribute('tooltip-true') && element.checked) {
      message = element.getAttribute('tooltip-true');
    }
    if (element.hasAttribute('tooltip-false') && !element.checked) {
      message = element.getAttribute('tooltip-false');
    }
  }
  wrapper.textContent = message;
  hovered = element;
  const positionEvent =
      new CustomEvent(TOOLTIP_POSITION_EVENT_NAME, {cancelable: true});
  const doDefault = hovered.dispatchEvent(positionEvent);
  if (doDefault) {
    position(hovered.getBoundingClientRect());
  }
  wrapper.classList.add('visible');
}

/**
 * Sets up tooltips for elements.
 *
 * @param elements Elements whose tooltips to be shown.
 */
export function setup(elements: HTMLElement[]): void {
  wrapper = dom.get('#tooltip', HTMLElement);
  for (const el of elements) {
    function handler() {
      // Handler hides tooltip only when it's for the element.
      if (el === hovered) {
        hide();
      }
    }
    el.addEventListener('mouseleave', handler);
    el.addEventListener('click', handler);
    el.addEventListener('blur', handler);
    el.addEventListener('mouseenter', () => show(el));
    el.addEventListener('focus', () => show(el));
  }
}
