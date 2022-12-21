// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview |GlobalScrollTargetBehavior| allows an element to be aware of
 * the global scroll target.
 *
 * |scrollTarget| will be populated async by |setGlobalScrollTarget|.
 *
 * |subpageScrollTarget| will be equal to the |scrollTarget|, but will only be
 * populated when the current route is the |subpageRoute|.
 *
 * |setGlobalScrollTarget| should only be called once.
 */

import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {Route, Router} from './router.js';
import {RouteObserverBehavior} from './route_observer_behavior.js';

let scrollTargetResolver = new PromiseResolver();

/** @polymerBehavior */
export const GlobalScrollTargetBehaviorImpl = {
  properties: {
    /**
     * Read only property for the scroll target.
     * @type {HTMLElement}
     */
    scrollTarget: {
      type: Object,
      readOnly: true,
    },

    /**
     * Read only property for the scroll target that a subpage should use.
     * It will be set/cleared based on the current route.
     * @type {HTMLElement}
     */
    subpageScrollTarget: {
      type: Object,
      computed: 'getActiveTarget_(scrollTarget, active_)',
    },

    /**
     * The |subpageScrollTarget| should only be set for this route.
     * @type {Route}
     * @private
     */
    subpageRoute: Object,

    /** Whether the |subpageRoute| is active or not. */
    active_: Boolean,
  },

  /** @override */
  attached() {
    this.active_ = Router.getInstance().getCurrentRoute() === this.subpageRoute;
    scrollTargetResolver.promise.then(this._setScrollTarget.bind(this));
  },

  /** @param {!Route} route */
  currentRouteChanged(route) {
    // Immediately set the scroll target to active when this page is
    // activated, but wait a task to remove the scroll target when the page is
    // deactivated. This gives scroll handlers like iron-list a chance to
    // handle scroll events that are fired as a result of the route changing.
    // TODO(https://crbug.com/859794): Having this timeout can result some
    // jumpy behaviour in the scroll handlers. |this.active_| can be set
    // immediately when this bug is fixed.
    if (route === this.subpageRoute) {
      this.active_ = true;
    } else {
      setTimeout(() => {
        this.active_ = false;
      });
    }
  },

  /**
   * Returns the target only when the route is active.
   * @param {HTMLElement} target
   * @param {boolean} active
   * @return {?HTMLElement|undefined}
   * @private
   */
  getActiveTarget_(target, active) {
    if (target === undefined || active === undefined) {
      return undefined;
    }

    return active ? target : null;
  },
};

/** @interface */
export class GlobalScrollTargetBehaviorInterface {
  constructor() {
    /**
     * Read only property for the scroll target.
     * @type {HTMLElement}
     */
    this.scrollTarget;

    /**
     * Read only property for the scroll target that a subpage should use.
     * It will be set/cleared based on the current route.
     * @type {HTMLElement}
     */
    this.subpageScrollTarget;

    /**
     * The |subpageScrollTarget| should only be set for this route.
     * @private {Route}
     */
    this.subpageRoute;
  }

  /** @param {!Route} route */
  currentRouteChanged(route) {}
}

/**
 * This should only be called once.
 * @param {HTMLElement} scrollTarget
 */
export function setGlobalScrollTarget(scrollTarget) {
  scrollTargetResolver.resolve(scrollTarget);
}

export function resetGlobalScrollTargetForTesting() {
  scrollTargetResolver = new PromiseResolver();
}

// This is done to make the closure compiler happy: it needs fully qualified
// names when specifying an array of behaviors.
/** @polymerBehavior */
export const GlobalScrollTargetBehavior =
    [RouteObserverBehavior, GlobalScrollTargetBehaviorImpl];
