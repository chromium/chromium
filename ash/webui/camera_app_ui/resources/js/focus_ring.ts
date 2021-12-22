// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  assert,
  assertInstanceof,
} from './assert.js';
import {cssStyle} from './css.js';
import * as dom from './dom.js';
import {getStyleValueInPx} from './util.js';

/**
 * Focus ring element.
 */
let ring: HTMLElement|null = null;

let ringCSSStyle: CSSStyleDeclaration|null = null;

/**
 * All valid values of '--focus-ring-style'.
 */
const ringStyleValues = new Set(['circle', 'mode-item-input', 'none', 'pill']);

/**
 * The reference bounding rectangle of focused UI.
 */
let uiRect = new DOMRectReadOnly();

/**
 * Name of event triggered for calculating bounding rectangle of focused UI.
 */
export const FOCUS_RING_UI_RECT_EVENT_NAME = 'focusringuirect';

/**
 * Sets reference bounding rectangle of focused UI.
 */
export function setUIRect(rect: DOMRectReadOnly): void {
  uiRect = rect;
}

/**
 * Shows focus ring on |el|.
 */
function showFocus(el: HTMLElement) {
  const style = el.computedStyleMap();
  const size = getStyleValueInPx(style, '--focus-ring-size');
  const ringStyleValue = `${style.get('--focus-ring-style')}`;
  for (const v of ringStyleValues) {
    ring.classList.toggle(v, ringStyleValue.includes(v));
  }
  const uiRectEvent =
      new CustomEvent(FOCUS_RING_UI_RECT_EVENT_NAME, {cancelable: true});
  const doDefault = el.dispatchEvent(uiRectEvent);
  if (doDefault) {
    setUIRect(el.getBoundingClientRect());
  }
  ringCSSStyle.setProperty('width', `${uiRect.width + size * 2}px`);
  ringCSSStyle.setProperty('height', `${uiRect.height + size * 2}px`);
  ringCSSStyle.setProperty('top', `${(uiRect.top + uiRect.bottom) / 2}px`);
  ringCSSStyle.setProperty('left', `${(uiRect.left + uiRect.right) / 2}px`);
}

export function initialize(): void {
  ring = dom.get('#focus-ring', HTMLElement);
  ringCSSStyle = cssStyle('#focus-ring');

  const setup = (el: HTMLElement) => {
    el.addEventListener('focus', () => showFocus(el));
    if (el === document.activeElement) {
      showFocus(el);
    }
  };

  dom.getAll('[tabindex]', HTMLElement).forEach(setup);
  const observer = new MutationObserver((mutationList) => {
    mutationList.forEach((mutation) => {
      assert(mutation.type === 'childList');
      // Only the newly added nodes with [tabindex] are considered here. So
      // simply adding class attribute on existing element will not work.
      for (const node of mutation.addedNodes) {
        if (!(node instanceof HTMLElement)) {
          continue;
        }
        const el = assertInstanceof(node, HTMLElement);
        if (el.hasAttribute('tabindex')) {
          setup(el);
        }
      }
    });
  });
  observer.observe(document.body, {
    subtree: true,
    childList: true,
  });
}
