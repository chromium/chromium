// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from 'chrome://resources/js/assert.js';
import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Constructor} from './types.js';
import {Route, Router} from '../router.js';

export interface RouteObserverMixinInterface {
  currentRouteChanged(newRoute: Route, oldRoute?: Route): void;
}

export const RouteObserverMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<RouteObserverMixinInterface> => {
      class RouteObserverMixin extends superClass implements
          RouteObserverMixinInterface {
        override connectedCallback(): void {
          super.connectedCallback();

          const routerInstance = Router.getInstance();
          routerInstance.addObserver(this);

          // Emulating Polymer data bindings, the observer is called when the
          // element starts observing the route.
          this.currentRouteChanged(routerInstance.currentRoute, undefined);
        }

        override disconnectedCallback(): void {
          super.disconnectedCallback();

          Router.getInstance().removeObserver(this);
        }

        currentRouteChanged(_newRoute: Route, _oldRoute?: Route): void {
          assertNotReached('Element must implement currentRouteChanged().');
        }
      }

      return RouteObserverMixin;
    });
