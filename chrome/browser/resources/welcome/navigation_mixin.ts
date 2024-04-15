// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The NavigationMixin is in charge of manipulating and
 *     watching window.history.state changes. The page is using the history
 *     state object to remember state instead of changing the URL directly,
 *     because the flow requires that users can use browser-back/forward to
 *     navigate between steps, without being able to go directly or copy an URL
 *     that points at a specific step. Using history.state object allows adding
 *     or popping history state without actually changing the path.
 */

import '../strings.m.js';

import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {routeObservers, setCurrentRouteElement} from './router.js';
import type {Routes} from './router.js';

type Constructor<T> = new (...args: any[]) => T;

/**
 * Elements can override onRoute(Change|Enter|Exit) to handle route changes.
 * Order of hooks being called:
 *   1) onRouteExit() on the old route
 *   2) onRouteChange() on all subscribed routes
 *   3) onRouteEnter() on the new route
 */
export const NavigationMixin =
    <T extends Constructor<CrLitElement>>(superClass: T): T&
    Constructor<NavigationMixinInterface> => {
      class NavigationMixin extends superClass {
        static get properties() {
          return {
            subtitle: String,
          };
        }

        subtitle?: string;

        override connectedCallback() {
          super.connectedCallback();

          assert(!routeObservers.has(this));
          routeObservers.add(this);
          const route = (history.state.route as Routes);
          const step = history.state.step;

          // history state was set when page loaded, so when the element first
          // attaches, call the route-change handler to initialize first.
          this.onRouteChange(route, step);

          // Modules are only attached to DOM if they're for the current route,
          // so as long as the id of an element matches up to the current step,
          // it means that element is for the current route.
          if (this.id === `step-${step}`) {
            setCurrentRouteElement(this);
            this.notifyRouteEnter();
          }
        }

        /**
         * Notifies elements that route was entered and updates the state of the
         * app based on the new route.
         */
        notifyRouteEnter() {
          this.onRouteEnter();
          this.updateFocusForA11y();
          this.updateTitle();
        }

        /** Called to update focus when progressing through the modules. */
        async updateFocusForA11y() {
          const header = this.shadowRoot!.querySelector('h1');
          if (header) {
            await this.updateComplete;
            header.focus();
          }
        }

        updateTitle() {
          let title = loadTimeData.getString('headerText');
          if (this.subtitle) {
            title += ' - ' + this.subtitle;
          }
          document.title = title;
        }

        override disconnectedCallback() {
          super.disconnectedCallback();
          assert(routeObservers.delete(this));
        }

        onRouteChange(_route: Routes, _step: number) {}
        onRouteEnter() {}
        onRouteExit() {}
        onRouteUnload() {}
      }

      return NavigationMixin;
    };

export interface NavigationMixinInterface {
  subtitle?: string;
  notifyRouteEnter(): void;
  updateFocusForA11y(): void;
  updateTitle(): void;
  onRouteChange(route: Routes, step: number): void;
  onRouteEnter(): void;
  onRouteExit(): void;
  onRouteUnload(): void;
}
