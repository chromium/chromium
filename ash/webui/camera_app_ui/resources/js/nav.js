// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from './assert.js';
import * as dom from './dom.js';
import {toggleExpertMode} from './expert.js';
import * as state from './state.js';
import * as toast from './toast.js';
// eslint-disable-next-line no-unused-vars
import {ViewName} from './type.js';
import * as util from './util.js';
// eslint-disable-next-line no-unused-vars
import {View} from './views/view.js';
import {windowController} from './window_controller.js';

/**
 * All views stacked in ascending z-order (DOM order) for navigation, and only
 * the topmost visible view is active (clickable/focusable).
 * @type {!Array<!View>}
 */
let allViews = [];

/**
 * Index of the current topmost visible view in the stacked views.
 * @type {number}
 */
let topmostIndex = -1;

// Disable checking jsdoc generator annotation which is incompatible with
// closure compiler: JSdoc use @generator and @yields while closure compiler use
// @return {Generator}.
/* eslint-disable valid-jsdoc */

/**
 * Gets view and all recursive subviews.
 * @param {!View} view
 * @return {!Generator<!View>}
 */
function* getRecursiveViews(view) {
  yield view;
  for (const subview of view.getSubViews()) {
    yield* getRecursiveViews(subview);
  }
}

/* eslint-enable valid-jsdoc */

/**
 * Sets up navigation for all views, e.g. camera-view, dialog-view, etc.
 * @param {!Array<!View>} views All views in ascending z-order.
 */
export function setup(views) {
  allViews = views.flatMap((v) => [...getRecursiveViews(v)]);
  // Manage all tabindex usages in for navigation.
  document.body.addEventListener('keydown', (event) => {
    const e = assertInstanceof(event, KeyboardEvent);
    if (e.key === 'Tab') {
      state.set(state.State.TAB_NAVIGATION, true);
    }
  });
  document.body.addEventListener(
      'pointerdown', () => state.set(state.State.TAB_NAVIGATION, false));
}

/**
 * Activates the view to be focusable.
 * @param {number} index Index of the view.
 */
function activate(index) {
  // Restore the view's child elements' tabindex and then focus the view.
  const view = allViews[index];
  view.root.setAttribute('aria-hidden', 'false');
  dom.getAllFrom(view.root, '[tabindex]', HTMLElement).forEach((element) => {
    if (element.dataset['tabindex'] === undefined) {
      // First activation, no need to restore tabindex from data-tabindex.
      return;
    }
    element.setAttribute('tabindex', element.dataset['tabindex']);
    element.removeAttribute('data-tabindex');
  });
  view.focus();
}

/**
 * Deactivates the view to be unfocusable.
 * @param {number} index Index of the view.
 */
function deactivate(index) {
  const view = allViews[index];
  view.root.setAttribute('aria-hidden', 'true');
  dom.getAllFrom(view.root, '[tabindex]', HTMLElement).forEach((element) => {
    element.dataset['tabindex'] = element.getAttribute('tabindex');
    element.setAttribute('tabindex', '-1');
  });
  const activeElement = document.activeElement;
  if (activeElement instanceof HTMLElement) {
    activeElement.blur();
  }
}

/**
 * Checks if the view is already shown.
 * @param {number} index Index of the view.
 * @return {boolean} Whether the view is shown or not.
 */
function isShown(index) {
  return state.get(allViews[index].name);
}

/**
 * Shows the view indexed in the stacked views and activates the view only if
 * it becomes the topmost visible view.
 * @param {number} index Index of the view.
 * @return {!View} View shown.
 */
function show(index) {
  const view = allViews[index];
  if (!isShown(index)) {
    state.set(view.name, true);
    view.layout();
    if (index > topmostIndex) {
      if (topmostIndex >= 0) {
        deactivate(topmostIndex);
      }
      activate(index);
      topmostIndex = index;
    }
  }
  return view;
}

/**
 * Finds the next topmost visible view in the stacked views.
 * @return {number} Index of the view found; otherwise, -1.
 */
function findNextTopmostIndex() {
  for (let i = topmostIndex - 1; i >= 0; i--) {
    if (isShown(i)) {
      return i;
    }
  }
  return -1;
}

/**
 * Hides the view indexed in the stacked views and deactivate the view if it was
 * the topmost visible view.
 * @param {number} index Index of the view.
 */
function hide(index) {
  if (index === topmostIndex) {
    deactivate(index);
    const next = findNextTopmostIndex();
    if (next >= 0) {
      activate(next);
    }
    topmostIndex = next;
  }
  state.set(allViews[index].name, false);
}

/**
 * Finds the view by its name in the stacked views.
 * @param {!ViewName} name View name.
 * @return {number} Index of the view found; otherwise, -1.
 */
function findIndex(name) {
  return allViews.findIndex((view) => view.name === name);
}

/**
 * Opens a navigation session of the view; shows the view before entering it and
 * hides the view after leaving it for the ended session.
 * @param {!ViewName} name View name.
 * @param {...*} args Optional rest parameters for entering the view.
 * @return {!Promise<*>} Promise for the operation or result.
 */
export function open(name, ...args) {
  const index = findIndex(name);
  return show(index).enter(...args).finally(() => {
    hide(index);
  });
}

/**
 * Closes the current navigation session of the view by leaving it.
 * @param {!ViewName} name View name.
 * @param {*=} condition Optional condition for leaving the view.
 * @return {boolean} Whether successfully leaving the view or not.
 */
export function close(name, condition) {
  const index = findIndex(name);
  return allViews[index].leave(condition);
}

/**
 * Handles key pressed event.
 * @param {!KeyboardEvent} event Key press event.
 */
export function onKeyPressed(event) {
  const key = util.getShortcutIdentifier(event);
  switch (key) {
    case 'BrowserBack':
      // Only works for non-intent instance.
      if (!state.get(state.State.INTENT)) {
        windowController.minimize();
      }
      break;
    case 'Alt--':
      // Prevent intent window from minimizing.
      if (state.get(state.State.INTENT)) {
        event.preventDefault();
      }
      break;
    case 'Ctrl-=':
    case 'Ctrl--':
      // Blocks the in-app zoom in/out to avoid unexpected layout.
      event.preventDefault();
      break;
    case 'Ctrl-V':
      toast.showDebugMessage('SWA');
      break;
    case 'Ctrl-Shift-E':
      toggleExpertMode();
      break;
    default:
      // Make the topmost visible view handle the pressed key.
      if (topmostIndex >= 0 && allViews[topmostIndex].onKeyPressed(key)) {
        event.preventDefault();
      }
  }
}

/**
 * Handles when the window state or size changed.
 */
export function onWindowStatusChanged() {
  // All visible views need being relayout after window is resized or state
  // changed.
  for (let i = allViews.length - 1; i >= 0; i--) {
    if (isShown(i)) {
      allViews[i].layout();
    }
  }
}

/**
 * Returns whether the view is the top view above all shown view.
 * @param {!ViewName} name Name of the view
 * @return {boolean}
 */
export function isTopMostView(name) {
  return topmostIndex === findIndex(name);
}
