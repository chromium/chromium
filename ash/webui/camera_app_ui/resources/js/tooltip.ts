// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from './assert.js';
import * as dom from './dom.js';
import {TextTooltip} from './lit/components/text-tooltip.js';

/**
 * Wrapper element that shows tooltip.
 */
let tooltipElement: TextTooltip|null = null;

/**
 * Hides the shown tooltip.
 */
export function hide(): void {
  assert(tooltipElement !== null);

  tooltipElement.anchorTarget = null;
  tooltipElement.target = null;
}

/**
 * Sets up tooltips for elements.
 *
 * @param elements Elements whose tooltips to be shown.
 */
export function setupElements(elements: HTMLElement[]): void {
  for (const el of elements) {
    function hideHandler() {
      assert(tooltipElement !== null);
      if (tooltipElement.target === el) {
        hide();
      }
    }
    function showHandler() {
      assert(tooltipElement !== null);
      let anchor = el;
      const selector = el.dataset['tooltipAnchor'];
      if (selector !== undefined) {
        anchor = dom.getFrom(el, selector, HTMLElement);
      }
      tooltipElement.target = el;
      tooltipElement.anchorTarget = anchor;
    }
    el.addEventListener('mouseleave', hideHandler);
    el.addEventListener('click', hideHandler);
    el.addEventListener('blur', hideHandler);
    el.addEventListener('mouseenter', showHandler);
    el.addEventListener('focus', showHandler);
  }
}

/**
 *  Initializes the tooltips. This should be called before other methods.
 */
export function init(): void {
  tooltipElement = dom.get('text-tooltip', TextTooltip);
}
