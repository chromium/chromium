// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The NavigationBehavior is in charge of manipulating and
 *     watching window.history.state changes. The page is using the history
 *     state object to remember state instead of changing the URL directly,
 *     because the flow requires that users can use browser-back/forward to
 *     navigate between steps, without being able to go directly or copy an URL
 *     that points at a specific step. Using history.state object allows adding
 *     or popping history state without actually changing the path.
 */

cr.define('welcome', function() {
  'use strict';

  /**
   * Valid route pathnames.
   * @enum {string}
   */
  const Routes = {
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

  // Notifies all the elements that extended NavigationBehavior.
  function notifyObservers() {
    const route = /** @type {!welcome.Routes} */ (history.state.route);
    const step = history.state.step;
    routeObservers.forEach((observer) => {
      (/** @type {{onRouteChange: Function}} */ (observer))
          .onRouteChange(route, step);
    });
  }

  // Notifies all elements when browser history is popped.
  window.addEventListener('popstate', notifyObservers);

  function navigateToNextStep() {
    history.pushState(
        {
          route: history.state.route,
          step: history.state.step + 1,
        },
        '', `/${history.state.route}`);
    notifyObservers();
  }

  /**
   * @param {!welcome.Routes} route
   * @param {number} step
   */
  function navigateTo(route, step) {
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

  /** @polymerBehavior */
  const NavigationBehavior = {
    /** @override */
    attached: function() {
      assert(!routeObservers.has(this));
      routeObservers.add(this);

      // history state was set when page loaded, so when the element first
      // attaches, call the route-change handler to initialize first.
      this.onRouteChange(
          /** @type {!welcome.Routes} */ (history.state.route),
          history.state.step);
    },

    /** @override */
    detached: function() {
      assert(routeObservers.delete(this));
    },

    /**
     * Elements can override onRouteChange to handle route changes.
     * @param {!welcome.Routes} route
     * @param {number} step
     */
    onRouteChange: function(route, step) {},
  };

  return {
    NavigationBehavior: NavigationBehavior,
    navigateTo: navigateTo,
    navigateToNextStep: navigateToNextStep,
    Routes: Routes,
  };
});
