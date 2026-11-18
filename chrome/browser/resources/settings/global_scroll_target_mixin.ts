// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview |GlobalScrollTargetMixinLit| allows an element to be aware of
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
import {dedupingMixin} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {routes} from './route.js';
import type {Route, RouteObserverMixinInterface} from './router.js';
import {Router} from './router.js';

let scrollTargetResolver = new PromiseResolver<HTMLElement>();

type Constructor<T> = new (...args: any[]) => T;

export interface GlobalScrollTargetMixinLitInterface extends
    RouteObserverMixinInterface {
  scrollTarget: HTMLElement;
  subpageScrollTarget: HTMLElement|null;
  subpageRoute: Route;
}

export const GlobalScrollTargetMixinLit = dedupingMixin(
    <T extends Constructor<CrLitElement&RouteObserverMixinInterface>>(
        superClass: T): T&Constructor<GlobalScrollTargetMixinLitInterface> => {
      class GlobalScrollTargetMixinLit extends superClass implements
          GlobalScrollTargetMixinLitInterface {
        static get properties() {
          return {
            subpageScrollTarget: {type: Object},
          };
        }

        accessor subpageScrollTarget: HTMLElement|null =
            document.documentElement;

        scrollTarget: HTMLElement = document.documentElement;
        subpageRoute: Route = routes.BASIC;
        private active_: boolean = false;

        override connectedCallback() {
          super.connectedCallback();

          this.active_ =
              Router.getInstance().getCurrentRoute() === this.subpageRoute;
          scrollTargetResolver.promise.then((scrollTarget: HTMLElement) => {
            this.scrollTarget = scrollTarget;
            this.subpageScrollTarget = this.active_ ? this.scrollTarget : null;
          });
        }

        override currentRouteChanged(route: Route, oldRoute?: Route) {
          super.currentRouteChanged(route, oldRoute);
          // Immediately set the scroll target to active when this page is
          // activated, but wait a task to remove the scroll target when the
          // page is deactivated. This gives scroll handlers like
          // cr-lazy-list a chance to handle scroll events that are fired as
          // a result of the route changing.
          // TODO(crbug.com/40583428): Having this timeout can result some
          // jumpy behaviour in the scroll handlers. |this.active_| can be set
          // immediately when this bug is fixed.
          if (route === this.subpageRoute) {
            this.active_ = true;
            this.subpageScrollTarget = this.scrollTarget;
          } else if (this.active_) {
            this.active_ = false;
            setTimeout(() => {
              if (!this.active_) {
                this.subpageScrollTarget = null;
              }
            });
          }
        }
      }

      return GlobalScrollTargetMixinLit;
    });

/**
 * This should only be called once.
 */
export function setGlobalScrollTarget(scrollTarget: HTMLElement) {
  scrollTargetResolver.resolve(scrollTarget);
}

export function resetGlobalScrollTargetForTesting() {
  scrollTargetResolver = new PromiseResolver();
}
