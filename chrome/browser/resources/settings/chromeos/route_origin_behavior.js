// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {focusWithoutInk} from 'chrome://resources/ash/common/focus_without_ink_js.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {beforeNextRender} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route, Router} from './router.js';

import {RouteObserverBehavior} from './route_observer_behavior.js';

/** @polymerBehavior */
export const RouteOriginBehaviorImpl = {
  properties: {
    /**
     * A Map specifying which element should be focused when exiting a
     * subpage. The key of the map holds a Route path, and the value holds
     * either a query selector that identifies the associated element to focus
     * or a function to be run when a neon-animation-finish event is handled.
     * @protected {!Map<string, string|Function>}
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
   * @protected
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
   * @param {!Route=} prevRoute
   * @protected
   */
  currentRouteChanged(newRoute, prevRoute) {
    // Only attempt to focus an anchor element if the most recent navigation
    // was a 'pop' (backwards) navigation.
    if (!Router.getInstance().lastRouteChangeWasPopstate()) {
      return;
    }

    // Route change does not apply to this page.
    if (this.route_ !== newRoute) {
      return;
    }

    this.triggerFocus_(prevRoute);
  },

  /**
   * Focuses the element for a given route by finding the associated
   * query selector or calling the configured function.
   * @param {Route=} route
   * @private
   */
  triggerFocus_(route) {
    if (!route) {
      return;
    }

    const pathConfig = this.focusConfig_.get(route.path);
    if (pathConfig) {
      if (typeof pathConfig === 'function') {
        pathConfig();
      } else if (typeof pathConfig === 'string') {
        const element = assert(this.$$(String(pathConfig)));
        beforeNextRender(this, () => {
          focusWithoutInk(element);
        });
      }
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

  /**
   * @param {!Route} newRoute
   * @param {!Route=} prevRoute
   * @protected
   */
  currentRouteChanged(newRoute, prevRoute) {}
}
