// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from 'chrome://resources/ash/common/assert.js';

import {Route, Router} from './router.js';

/** @polymerBehavior */
export const RouteObserverBehavior = {
  /** @override */
  attached() {
    Router.getInstance().addObserver(this);

    // Emulating Polymer data bindings, the observer is called when the
    // element starts observing the route.
    this.currentRouteChanged(Router.getInstance().currentRoute, undefined);
  },

  /** @override */
  detached() {
    Router.getInstance().removeObserver(this);
  },

  /**
   * @param {!Route} newRoute
   * @param {!Route=} opt_oldRoute
   */
  currentRouteChanged(newRoute, opt_oldRoute) {
    assertNotReached();
  },
};

/** @interface */
export class RouteObserverBehaviorInterface {
  /**
   * @param {!Route} newRoute
   * @param {!Route=} opt_oldRoute
   */
  currentRouteChanged(newRoute, opt_oldRoute) {}
}
