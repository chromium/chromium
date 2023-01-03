// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from 'chrome://resources/js/assert_ts.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {beforeNextRender, dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from './assert_extras.js';
import {Constructor} from './common/types.js';
import {RouteObserverMixin, RouteObserverMixinInterface} from './route_observer_mixin.js';
import {Route, Router} from './router.js';

export interface RouteOriginMixinInterface extends RouteObserverMixinInterface {
  addFocusConfig(route: Route|undefined, value: string): void;
}

export const RouteOriginMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<RouteOriginMixinInterface> => {
      const superClassBase = RouteObserverMixin(superClass);

      class RouteOriginMixin extends superClassBase implements
          RouteOriginMixinInterface {
        static get properties() {
          return {
            /**
             * A Map specifying which element should be focused when exiting a
             * subpage. The key of the map holds a Route path, and the value
             * holds either a query selector that identifies the associated
             * element to focus or a function to be run when a
             * neon-animation-finish event is handled.
             */
            focusConfig_: {
              type: Object,
              value: () => new Map(),
            },
          };
        }

        private focusConfig_: Map<string, string|Function>;
        /**
         * The route corresponding to this page.
         */
        private route_: Route|undefined = undefined;

        override connectedCallback(): void {
          super.connectedCallback();
          // All elements with this behavior must specify their route.
          assertInstanceof(
              this.route_, Route,
              'Route origin element must specify its route.');
        }

        override currentRouteChanged(newRoute: Route, prevRoute?: Route) {
          // Only attempt to focus an anchor element if the most recent
          // navigation was a 'pop' (backwards) navigation.
          if (!Router.getInstance().lastRouteChangeWasPopstate()) {
            return;
          }

          // Route change does not apply to this page.
          if (this.route_ !== newRoute) {
            return;
          }

          this.triggerFocus_(prevRoute);
        }

        /**
         * Adds a route path to |this.focusConfig_| if the route exists.
         * Otherwise it does nothing.
         * @param value A query selector leading to a button that routes
         *     the user to |route| if it is defined.
         */
        addFocusConfig(route: Route|undefined, value: string) {
          if (route) {
            this.focusConfig_.set(route.path, value);
          }
        }

        /**
         * Focuses the element for a given route by finding the associated
         * query selector or calling the configured function.
         */
        private triggerFocus_(route?: Route) {
          if (!route) {
            return;
          }

          const pathConfig = this.focusConfig_.get(route.path);
          if (pathConfig) {
            if (typeof pathConfig === 'function') {
              pathConfig();
            } else if (typeof pathConfig === 'string') {
              const element = castExists(
                  this.shadowRoot!.querySelector<HTMLElement>(pathConfig));
              beforeNextRender(this, () => {
                focusWithoutInk(element);
              });
            }
          }
        }
      }

      return RouteOriginMixin;
    });
