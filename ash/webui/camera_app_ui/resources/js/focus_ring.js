// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  assert,
  assertInstanceof,
} from './chrome_util.js';
import {cssStyle} from './css.js';
import * as dom from './dom.js';
import {getStyleValueInPx} from './util.js';

/**
 * Focus ring element.
 * @type {?HTMLElement}
 */
let ring = null;

/**
 * @type {?CSSStyleDeclaration}
 */
let ringCSSStyle = null;

/**
 * All valid values of '--focus-ring-style'.
 * @const {!Set<string>}
 */
const ringStyleValues = new Set(['circle', 'mode-item-input', 'none', 'pill']);

/**
 * The reference bounding rectangle of focused UI.
 * @type {!DOMRectReadOnly}
 */
let uiRect = new DOMRectReadOnly();

/**
 * Name of event triggered for calculating bounding rectangle of focused UI.
 * @const {string}
 */
export const FOCUS_RING_UI_RECT_EVENT_NAME = 'focusringuirect';

/**
 * Sets reference bounding rectangle of focused UI.
 * @param {!DOMRectReadOnly} rect
 */
export function setUIRect(rect) {
  uiRect = rect;
}

/**
 * @param {!FocusEvent} e
 */
function onFocus({target}) {
  const el = assertInstanceof(target, HTMLElement);
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

/**
 * @public
 */
export function initialize() {
  ring = dom.get('#focus-ring', HTMLElement);
  ringCSSStyle = cssStyle('#focus-ring');

  const setup = (el) => {
    el.addEventListener('focus', onFocus);
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
