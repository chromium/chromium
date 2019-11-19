// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {afterNextRender} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview The NavigationBehavior is in charge of manipulating and
 *     watching window.history.state changes. The page is using the history
 *     state object to remember state instead of changing the URL directly,
 *     because the flow requires that users can use browser-back/forward to
 *     navigate between steps, without being able to go directly or copy an URL
 *     that points at a specific step. Using history.state object allows adding
 *     or popping history state without actually changing the path.
 */

/**
 * Valid route pathnames.
 * @enum {string}
 */
export const Routes = {
  LANDING: 'landing',
  NEW_USER: 'new-user',
  RETURNING_USER: 'returning-user',
};

/**
 * Regular expression that captures the leading slash, the content and the
 * trailing slash in three different groups.
 * @const {!RegExp}
 */
const CANONICAL_PATH_REGEX = /(^\/)([\/-\w]+)(\/$)/;
const path = location.pathname.replace(CANONICAL_PATH_REGEX, '$1$2');

// Sets up history state based on the url path, unless it's already set (e.g.
// when user uses browser-back button to get back on chrome://welcome/...).
if (!history.state || !history.state.route || !history.state.step) {
  switch (path) {
    case `/${Routes.NEW_USER}`:
      history.replaceState({route: Routes.NEW_USER, step: 1}, '', path);
      break;
    case `/${Routes.RETURNING_USER}`:
      history.replaceState({route: Routes.RETURNING_USER, step: 1}, '', path);
      break;
    default:
      history.replaceState(
          {route: Routes.LANDING, step: Routes.LANDING}, '', '/');
  }
}

/** @type {!Set<!PolymerElement>} */
const routeObservers = new Set();

/** @type {?PolymerElement} */
let currentRouteElement;

// Notifies all the elements that extended NavigationBehavior.
function notifyObservers() {
  if (currentRouteElement) {
    (/** @type {{onRouteExit: Function}} */ (currentRouteElement))
        .onRouteExit();
    currentRouteElement = null;
  }

  const route = /** @type {!Routes} */ (history.state.route);
  const step = history.state.step;
  routeObservers.forEach(observer => {
    (/** @type {{onRouteChange: Function}} */ (observer))
        .onRouteChange(route, step);

    // Modules are only attached to DOM if they're for the current route, so
    // as long as the id of an element matches up to the current step, it
    // means that element is for the current route.
    if (observer.id == `step-${step}`) {
      currentRouteElement = observer;
    }
  });

  // If currentRouteElement is not null, it means there was a new route.
  if (currentRouteElement) {
    (/** @type {{onRouteEnter: Function}} */ (currentRouteElement))
        .onRouteEnter();
    (/** @type {{updateFocusForA11y: Function}} */ (currentRouteElement))
        .updateFocusForA11y();
  }
}

// Notifies all elements when browser history is popped.
window.addEventListener('popstate', notifyObservers);

// Notify the active element before unload.
window.addEventListener('beforeunload', () => {
  if (currentRouteElement) {
    (/** @type {{onRouteUnload: Function}} */ (currentRouteElement))
        .onRouteUnload();
  }
});

export function navigateToNextStep() {
  history.pushState(
      {
        route: history.state.route,
        step: history.state.step + 1,
      },
      '', `/${history.state.route}`);
  notifyObservers();
}

/**
 * @param {!Routes} route
 * @param {number} step
 */
export function navigateTo(route, step) {
  assert([
    Routes.LANDING,
    Routes.NEW_USER,
    Routes.RETURNING_USER,
  ].includes(route));

  history.pushState(
      {
        route: route,
        step: step,
      },
      '', '/' + (route === Routes.LANDING ? '' : route));
  notifyObservers();
}

/**
 * Elements can override onRoute(Change|Enter|Exit) to handle route changes.
 * Order of hooks being called:
 *   1) onRouteExit() on the old route
 *   2) onRouteChange() on all subscribed routes
 *   3) onRouteEnter() on the new route
 *
 * @polymerBehavior
 */
export const NavigationBehavior = {
  /** @override */
  attached: function() {
    assert(!routeObservers.has(this));
    routeObservers.add(this);

    const route = /** @type {!Routes} */ (history.state.route);
    const step = history.state.step;

    // history state was set when page loaded, so when the element first
    // attaches, call the route-change handler to initialize first.
    this.onRouteChange(route, step);

    // Modules are only attached to DOM if they're for the current route, so
    // as long as the id of an element matches up to the current step, it
    // means that element is for the current route.
    if (this.id == `step-${step}`) {
      currentRouteElement = this;
      this.onRouteEnter();
      this.updateFocusForA11y();
    }
  },

  /** Called to update focus when progressing through the modules. */
  updateFocusForA11y: function() {
    const header = this.$$('h1');
    if (header) {
      afterNextRender(this, () => header.focus());
    }
  },

  /** @override */
  detached: function() {
    assert(routeObservers.delete(this));
  },

  /**
   * @param {!Routes} route
   * @param {number} step
   */
  onRouteChange: function(route, step) {},

  onRouteEnter: function() {},

  onRouteExit: function() {},

  onRouteUnload: function() {},
};
