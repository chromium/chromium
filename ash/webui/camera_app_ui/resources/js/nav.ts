// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from './assert.js';
import {DEPLOYED_VERSION} from './deployed_version.js';
import {toggleExpertMode} from './expert.js';
import * as state from './state.js';
import * as toast from './toast.js';
import {ViewName} from './type.js';
import * as util from './util.js';
import {EnterOptions, LeaveCondition, View} from './views/view.js';
import {windowController} from './window_controller.js';

/**
 * All views stacked in ascending z-order (DOM order) for navigation, and only
 * the topmost shown view is active (clickable/focusable).
 */
let allViews: View[] = [];

/**
 * Index of the current topmost shown view in the stacked views.
 */
let topmostIndex = -1;

/**
 * Gets view and all recursive subviews.
 */
function* getRecursiveViews(view: View): Generator<View> {
  yield view;
  for (const subview of view.getSubViews()) {
    yield* getRecursiveViews(subview);
  }
}

/**
 * Sets up navigation for all views, e.g. camera-view, dialog-view, etc.
 *
 * @param views All views in ascending z-order.
 */
export function setup(views: View[]): void {
  allViews = views.flatMap((v) => [...getRecursiveViews(v)]);
  document.addEventListener('pointerdown', () => {
    state.set(state.State.KEYBOARD_NAVIGATION, false);
  });
  document.addEventListener('keydown', () => {
    state.set(state.State.KEYBOARD_NAVIGATION, true);
  });
}

/**
 * Checks if the view is already shown.
 *
 * @param index Index of the view.
 */
function isShown(index: number): boolean {
  return state.get(allViews[index].name);
}

/**
 * Shows the view indexed in the stacked views and activates the view only if
 * it becomes the topmost shown view.
 *
 * @param index Index of the view.
 */
function show(index: number): View {
  const view = allViews[index];
  if (isShown(index)) {
    return view;
  }
  state.set(view.name, true);
  view.layout();
  if (index > topmostIndex) {
    if (topmostIndex >= 0) {
      allViews[topmostIndex].onCoveredAsTop();
    }
    topmostIndex = index;
    allViews[index].onShownAsTop();
  }
  return view;
}

/**
 * Finds the next topmost shown view in the stacked views.
 *
 * @return Index of the view found; otherwise, -1.
 */
function findNextTopmostIndex(): number {
  for (let i = topmostIndex - 1; i >= 0; i--) {
    if (isShown(i)) {
      return i;
    }
  }
  return -1;
}

/**
 * Hides the view indexed in the stacked views and deactivate the view if it was
 * the topmost shown view.
 *
 * @param index Index of the view.
 */
function hide(index: number) {
  assert(isShown(index));
  if (index === topmostIndex) {
    allViews[index].onHideAsTop();
    const next = findNextTopmostIndex();
    topmostIndex = next;
    if (next >= 0) {
      allViews[next].onUncoveredAsTop(allViews[index].name);
    }
  }
  state.set(allViews[index].name, false);
}

/**
 * Finds the view by its name in the stacked views.
 *
 * @param name View name.
 * @return Index of the view found; otherwise, -1.
 */
function findIndex(name: ViewName): number {
  return allViews.findIndex((view) => view.name === name);
}

/**
 * Opens a navigation session of the view; shows the view before entering it and
 * hides the view after leaving it for the ended session.
 *
 * The Warning view can be opened multiple times with different warning types
 * before being closed. `hide` might be called multiple times at the time the
 * Warning view is closed (no remaining warning types).
 *
 * @param name View name.
 * @param options Optional rest parameters for entering the view.
 * @return Promise for the operation or result.
 */
export function open(
    name: ViewName, options?: EnterOptions): {closed: Promise<LeaveCondition>} {
  const index = findIndex(name);
  const view = show(index);
  return {
    closed: view.enter(options).finally(() => {
      if (isShown(index)) {
        hide(index);
      }
    }),
  };
}

/**
 * Closes the current navigation session of the view by leaving it.
 *
 * @param name View name.
 * @param condition Optional condition for leaving the view.
 */
export function close(name: ViewName, condition?: unknown): void {
  const index = findIndex(name);
  allViews[index].leave({kind: 'CLOSED', val: condition});
}

/**
 * Handles key pressed event.
 */
export function onKeyPressed(event: KeyboardEvent): void {
  const key = util.getKeyboardShortcut(event);
  switch (key) {
    case 'BrowserBack':
      // Only works for non-intent instance.
      if (!state.get(state.State.INTENT)) {
        // This is used in keypress event handler, and we don't wait for the
        // window to minimize here.
        void windowController.minimize();
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
      toast.showDebugMessage(`SWA${
          DEPLOYED_VERSION === undefined ?
              '' :
              `, Local overrde enabled (${DEPLOYED_VERSION})`}`);
      break;
    case 'Ctrl-Shift-E':
      toggleExpertMode();
      break;
    default:
      // Make the topmost shown view handle the pressed key.
      if (topmostIndex >= 0 && allViews[topmostIndex].onKeyPressed(key)) {
        event.preventDefault();
      }
  }
}

/**
 * Relayout all shown views.
 *
 * All shown views need being relayout after window is resized or state
 * changed.
 */
export function layoutShownViews(): void {
  for (let i = allViews.length - 1; i >= 0; i--) {
    if (isShown(i)) {
      allViews[i].layout();
    }
  }
}

/**
 * Returns whether the view is the top view above all shown view.
 */
export function isTopMostView(name: ViewName): boolean {
  return topmostIndex === findIndex(name);
}
