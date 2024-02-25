// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Extends the RouteObserverMixin by adding focus configuration via a mapping
 * of Route path to element selector. When exiting a subpage via back
 * navigation, the element which triggers the subpage's route will be focused.
 *
 * Subscribing elements must specify their `route` instance variable and call
 * the `currentRouteChanged()` super method.
 */

import {assertInstanceof} from 'chrome://resources/js/assert.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {afterNextRender, dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route, Router, routes} from '../router.js';

import {RouteObserverMixin, RouteObserverMixinInterface} from './route_observer_mixin.js';
import {Constructor} from './types.js';

type FinderFn = () => HTMLElement|null;
export type ElementConfig = string|HTMLElement|FinderFn;
export type FocusConfig = Map<string, ElementConfig>;

export interface RouteOriginMixinInterface extends RouteObserverMixinInterface {
  route: Route|undefined;
  addFocusConfig(route: Route|undefined, value: ElementConfig): void;
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
             * subpage. The key of the map holds a Route object's path, and the
             * value holds the configuration to find and focus the element. See
             * addFocusConfig() for more details.
             */
            focusConfig_: {
              type: Object,
              value: () => new Map(),
            },
          };
        }

        /**
         * The route corresponding to this page.
         */
        route: Route|undefined = undefined;
        private focusConfig_: FocusConfig;

        override connectedCallback(): void {
          super.connectedCallback();

          // All elements using this mixin must specify their route.
          assertInstanceof(
              this.route, Route,
              `Route origin element "${this.tagName}" must specify its route.`);
        }

        override currentRouteChanged(newRoute: Route, prevRoute?: Route): void {
          // Only attempt to focus an anchor element if the most recent
          // navigation was a 'pop' (backwards) navigation.
          if (!Router.getInstance().lastRouteChangeWasPopstate()) {
            return;
          }

          // Route change does not apply to the route for this page.
          // When infinite scroll exists (OsSettingsRevampWayfinding disabled)
          // subpage triggers should be refocused if the previous route was the
          // root page.
          if (newRoute !== this.route && newRoute !== routes.BASIC) {
            return;
          }

          if (prevRoute) {
            // Defer focusing trigger element until after next render
            afterNextRender(this, () => {
              this.focusTriggerElement(prevRoute);
            });
          }
        }

        /**
         * Adds a route path to |this.focusConfig_| if the route exists.
         * Otherwise it does nothing.
         * @param value One of the following:
         *  1) A string representing a query selector for the element.
         *  2) A reference to the element.
         *  3) A function that returns the element, or returns null if the
         *     element will be focused manually.
         */
        addFocusConfig(route: Route|undefined, value: ElementConfig): void {
          if (route) {
            this.focusConfig_.set(route.path, value);
          }
        }

        /**
         * Focuses the element for a given route by finding the associated
         * query selector or calling the configured function.
         */
        private focusTriggerElement(route: Route): void {
          const config = this.focusConfig_.get(route.path);
          if (!config) {
            return;
          }

          let element: HTMLElement|null = null;
          if (typeof config === 'function') {
            element = config();
          } else if (typeof config === 'string') {
            element = this.shadowRoot!.querySelector<HTMLElement>(config);
          } else if (config instanceof HTMLElement) {
            element = config;
          }

          if (element) {
            focusWithoutInk(element);
          }
        }
      }

      return RouteOriginMixin;
    });
