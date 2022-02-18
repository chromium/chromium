// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';

import {Route, Router} from '../router.js';

import {RouteObserverBehavior} from './route_observer_behavior.js';

/** @polymerBehavior */
export const RouteOriginBehaviorImpl = {
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
   * @protected {!Route|undefined}
   */
  route_: undefined,

  /**
   * Adds a route path to |this.focusConfig_| if the route exists. Otherwise
   *     it does nothing.
   * @param {!Route|undefined} route
   * @param {string} value A query selector leading to a button that routes
   *     the user to |route| if it is defined.
   */
  addFocusConfig(route, value) {
    if (route) {
      this.focusConfig_.set(route.path, value);
    }
  },

  /** @override */
  attached() {
    // All elements with this behavior must specify their route.
    assert(this.route_ instanceof Route);
  },

  /**
   * RouteObserverBehavior
   * @param {!Route} newRoute
   * @param {!Route|undefined} oldRoute
   * @protected
   */
  currentRouteChanged(newRoute, oldRoute) {
    // Don't attempt to focus any anchor element, unless last navigation was a
    // 'pop' (backwards) navigation.
    if (!Router.getInstance().lastRouteChangeWasPopstate()) {
      return;
    }

    if (this.route_ !== newRoute || !oldRoute) {
      return;
    }

    const focusSelector = this.focusConfig_.get(oldRoute.path);
    if (focusSelector) {
      this.$$(focusSelector).focus();
    }
  },
};

/** @polymerBehavior */
export const RouteOriginBehavior =
    [RouteObserverBehavior, RouteOriginBehaviorImpl];

/** @interface */
export class RouteOriginBehaviorInterface {
  /**
   * @param {!Route|undefined} route
   * @param {string} value
   */
  addFocusConfig(route, value) {}
}
