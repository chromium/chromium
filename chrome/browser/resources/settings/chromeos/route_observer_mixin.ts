// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route, Router} from './router.js';

type Constructor<T> = new (...args: any[]) => T;

export interface RouteObserverMixinInterface {
  currentRouteChanged(newRoute: Route, oldRoute?: Route): void;
}

export const RouteObserverMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<RouteObserverMixinInterface> => {
      class RouteObserverMixin extends superClass implements
          RouteObserverMixinInterface {
        private routerInstance_: Router;

        constructor(...args: any[]) {
          super(...args);

          this.routerInstance_ = Router.getInstance();
        }

        override connectedCallback(): void {
          super.connectedCallback();

          this.routerInstance_.addObserver(this);

          // Emulating Polymer data bindings, the observer is called when the
          // element starts observing the route.
          this.currentRouteChanged(
              this.routerInstance_.currentRoute, undefined);
        }

        override disconnectedCallback(): void {
          super.disconnectedCallback();

          this.routerInstance_.removeObserver(this);
        }

        currentRouteChanged(_newRoute: Route, _oldRoute?: Route): void {
          assertNotReached('Element must implement currentRouteChanged().');
        }
      }

      return RouteObserverMixin;
    });
