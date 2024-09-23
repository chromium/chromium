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
import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {RouteObserverMixin, RouteObserverMixinInterface} from '../common/route_observer_mixin.js';
import {Route, Router} from '../router.js';

import {Constructor} from './types.js';

let scrollTargetResolver = new PromiseResolver<HTMLElement>();

export interface GlobalScrollTargetMixinInterface extends
    RouteObserverMixinInterface {
  scrollTarget: HTMLElement;
  subpageRoute: Route;
  subpageScrollTarget: HTMLElement;
  currentRouteChanged(route: Route): void;
}

export const GlobalScrollTargetMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<GlobalScrollTargetMixinInterface> => {
      const superclassBase = RouteObserverMixin(superClass);

      // Needed to define auto-generated method. See more:
      // https://polymer-library.polymer-project.org/3.0/docs/devguide/properties#read-only
      interface GlobalScrollTargetMixinInternal {
        _setScrollTarget(scrollTarget: HTMLElement): void;
      }

      class GlobalScrollTargetMixinInternal extends superclassBase implements
          GlobalScrollTargetMixinInterface {
        static get properties() {
          return {
            /**
             * Read only property for the scroll target.
             */
            scrollTarget: {
              type: Object,
              readOnly: true,
            },

            /**
             * Read only property for the scroll target that a subpage
             * should use. It will be set/cleared based on the current
             * route.
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
        subpageRoute: Route;
        subpageScrollTarget: HTMLElement;
        private active_: boolean;

        override connectedCallback(): void {
          super.connectedCallback();

          this.active_ =
              Router.getInstance().currentRoute === this.subpageRoute;
          scrollTargetResolver.promise.then(this._setScrollTarget.bind(this));
        }

        override currentRouteChanged(route: Route): void {
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
        private getActiveTarget_(target: HTMLElement, active: boolean):
            HTMLElement|null {
          if (target === undefined || active === undefined) {
            return null;
          }

          return active ? target : null;
        }
      }
      return GlobalScrollTargetMixinInternal;
    });

/**
 * This should only be called once.
 */
export function setGlobalScrollTarget(scrollTarget: HTMLElement): void {
  scrollTargetResolver.resolve(scrollTarget);
}

export function resetGlobalScrollTargetForTesting(): void {
  scrollTargetResolver = new PromiseResolver<HTMLElement>();
}
