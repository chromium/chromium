// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {RouteObserverBehavior, Route, Router} from '../router.js';
// #import {assert} from 'chrome://resources/js/assert.m.js';

cr.define('settings', function() {
  /** @polymerBehavior */
  /* #export */ const RouteOriginBehaviorImpl = {
    properties: {
      /**
       * A map whose values are query selectors of subpage buttons on the page
       *     keyed by the route path they lead to.
       * @protected {!Map<string, string>}
       */
      focusConfig_: {
        type: Object,
        value: () => new Map(),
      },
    },

    /**
     * The route corresponding to this page.
     * @protected {!settings.Route|undefined}
     */
    route_: undefined,

    /**
     * Adds a route path to |this.focusConfig_| if the route exists. Otherwise
     *     it does nothing.
     * @param {!settings.Route|undefined} route
     * @param {string} value A query selector leading to a button that routes
     *     the user to |route| if it is defined.
     */
    addFocusConfig_(route, value) {
      if (route) {
        this.focusConfig_.set(route.path, value);
      }
    },

    /** @override */
    attached() {
      // All elements with this behavior must specify their route.
      assert(this.route_ instanceof settings.Route);
    },

    /**
     * settings.RouteObserverBehavior
     * @param {!settings.Route} newRoute
     * @param {!settings.Route} oldRoute
     * @protected
     */
    currentRouteChanged(newRoute, oldRoute) {
      // Don't attempt to focus any anchor element, unless last navigation was a
      // 'pop' (backwards) navigation.
      if (!settings.Router.getInstance().lastRouteChangeWasPopstate()) {
        return;
      }
      const focusSelector = this.focusConfig_.get(oldRoute.path);

      if (this.route_ !== newRoute || !focusSelector) {
        return;
      }

      this.$$(focusSelector).focus();
    },
  };

  /** @polymerBehavior */
  /* #export */ const RouteOriginBehavior =
      [settings.RouteObserverBehavior, RouteOriginBehaviorImpl];

  // #cr_define_end
  return {RouteOriginBehaviorImpl, RouteOriginBehavior};
});

