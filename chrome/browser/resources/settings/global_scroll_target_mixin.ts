// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview |GlobalScrollTargetMixin| allows an element to be aware of
 * the global scroll target.
 *
 * |scrollTarget| will be populated async by |setGlobalScrollTarget|.
 *
 * |subpageScrollTarget| will be equal to the |scrollTarget|, but will only be
 * populated when the current route is the |subpageRoute|.
 *
 * |setGlobalScrollTarget| should only be called once.
 */

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import type {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {dedupingMixin} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {Route, RouteObserverMixinInterface} from './router.js';
import {RouteObserverMixin, Router} from './router.js';

let scrollTargetResolver = new PromiseResolver<HTMLElement>();

type Constructor<T> = new (...args: any[]) => T;

export const GlobalScrollTargetMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<GlobalScrollTargetMixinInterface> => {
      const superClassBase = RouteObserverMixin(superClass) as unknown as T;

      class GlobalScrollTargetMixin extends superClassBase {
        static get properties() {
          return {
            scrollTarget: Object,

            /**
             * Read only property for the scroll target that a subpage should
             * use. It will be set/cleared based on the current route.
             */
            subpageScrollTarget: {
              type: Object,
              computed: 'getActiveTarget_(scrollTarget, active_)',
            },

            /**
             * The |subpageScrollTarget| should only be set for this route.
             */
            subpageRoute: Object,

            /** Whether the |subpageRoute| is active or not. */
            active_: Boolean,
          };
        }

        scrollTarget: HTMLElement;
        subpageScrollTarget: HTMLElement|null;
        subpageRoute: Route;
        private active_: boolean;

        override connectedCallback() {
          super.connectedCallback();

          this.active_ =
              Router.getInstance().getCurrentRoute() === this.subpageRoute;
          scrollTargetResolver.promise.then((scrollTarget: HTMLElement) => {
            this.scrollTarget = scrollTarget;
          });
        }

        // TODO(dpapad): Figure out why adding the |override| keyword here
        // throws an error.
        currentRouteChanged(route: Route) {
          // Immediately set the scroll target to active when this page is
          // activated, but wait a task to remove the scroll target when the
          // page is deactivated. This gives scroll handlers like iron-list a
          // chance to handle scroll events that are fired as a result of the
          // route changing.
          // TODO(crbug.com/40583428): Having this timeout can result some
          // jumpy behaviour in the scroll handlers. |this.active_| can be set
          // immediately when this bug is fixed.
          if (route === this.subpageRoute) {
            this.active_ = true;
          } else {
            setTimeout(() => {
              this.active_ = false;
            });
          }
        }

        /**
         * Returns the target only when the route is active.
         */
        private getActiveTarget_(
            target: HTMLElement|undefined,
            active: boolean|undefined): HTMLElement|null|undefined {
          if (target === undefined || active === undefined) {
            return undefined;
          }

          return active ? target : null;
        }
      }

      return GlobalScrollTargetMixin;
    });

export interface GlobalScrollTargetMixinInterface extends
    RouteObserverMixinInterface {
  scrollTarget: HTMLElement;
}

/**
 * This should only be called once.
 */
export function setGlobalScrollTarget(scrollTarget: HTMLElement) {
  scrollTargetResolver.resolve(scrollTarget);
}

export function resetGlobalScrollTargetForTesting() {
  scrollTargetResolver = new PromiseResolver();
}
