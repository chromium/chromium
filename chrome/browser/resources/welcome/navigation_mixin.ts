// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {afterNextRender, dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview The NavigationMixin is in charge of manipulating and
 *     watching window.history.state changes. The page is using the history
 *     state object to remember state instead of changing the URL directly,
 *     because the flow requires that users can use browser-back/forward to
 *     navigate between steps, without being able to go directly or copy an URL
 *     that points at a specific step. Using history.state object allows adding
 *     or popping history state without actually changing the path.
 */

/**
 * Valid route pathnames.
 */
export enum Routes {
  LANDING = 'landing',
  NEW_USER = 'new-user',
  RETURNING_USER = 'returning-user'
}

/**
 * Regular expression that captures the leading slash, the content and the
 * trailing slash in three different groups.
 */
const CANONICAL_PATH_REGEX: RegExp = /(^\/)([\/-\w]+)(\/$)/;
const path = location.pathname.replace(CANONICAL_PATH_REGEX, '$1$2');

// Sets up history state based on the url path, unless it's already set (e.g.
// when user uses browser-back button to get back on chrome://welcome/...).
if (!history.state || !history.state.route || !history.state.step) {
  switch (path) {
    case `/${Routes.NEW_USER}`:
      history.replaceState({route: Routes.NEW_USER, step: 1}, '', path);
      break;
    case `/${Routes.RETURNING_USER}`:
      history.replaceState({route: Routes.RETURNING_USER, step: 1}, '', path);
      break;
    default:
      history.replaceState(
          {route: Routes.LANDING, step: Routes.LANDING}, '', '/');
  }
}

const routeObservers: Set<NavigationMixinInterface> = new Set();
let currentRouteElement: NavigationMixinInterface|null;

// Notifies all the elements that extended NavigationMixin.
function notifyObservers() {
  if (currentRouteElement) {
    currentRouteElement.onRouteExit();
    currentRouteElement = null;
  }
  const route = (history.state.route as Routes);
  const step = history.state.step;
  routeObservers.forEach(observer => {
    observer.onRouteChange(route, step);

    // Modules are only attached to DOM if they're for the current route, so
    // as long as the id of an element matches up to the current step, it
    // means that element is for the current route.
    if ((observer as unknown as HTMLElement).id === `step-${step}`) {
      currentRouteElement = observer;
    }
  });

  // If currentRouteElement is not null, it means there was a new route.
  if (currentRouteElement) {
    (currentRouteElement as NavigationMixinInterface).notifyRouteEnter();
  }
}

// Notifies all elements when browser history is popped.
window.addEventListener('popstate', notifyObservers);

// Notify the active element before unload.
window.addEventListener('beforeunload', () => {
  if (currentRouteElement) {
    (currentRouteElement as {onRouteUnload: Function}).onRouteUnload();
  }
});

export function navigateToNextStep() {
  history.pushState(
      {route: history.state.route, step: history.state.step + 1}, '',
      `/${history.state.route}`);
  notifyObservers();
}

export function navigateTo(route: Routes, step: number) {
  assert([
    Routes.LANDING,
    Routes.NEW_USER,
    Routes.RETURNING_USER,
  ].includes(route));

  history.pushState(
      {
        route: route,
        step: step,
      },
      '', '/' + (route === Routes.LANDING ? '' : route));
  notifyObservers();
}

type Constructor<T> = new (...args: any[]) => T;

/**
 * Elements can override onRoute(Change|Enter|Exit) to handle route changes.
 * Order of hooks being called:
 *   1) onRouteExit() on the old route
 *   2) onRouteChange() on all subscribed routes
 *   3) onRouteEnter() on the new route
 */
export const NavigationMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
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
            currentRouteElement = this;
            this.notifyRouteEnter();
          }
        }

        /**
         * Notifies elements that route was entered and updates the state of the
         * app based on the new route.
         */
        notifyRouteEnter(): void {
          this.onRouteEnter();
          this.updateFocusForA11y();
          this.updateTitle();
        }

        /** Called to update focus when progressing through the modules. */
        updateFocusForA11y(): void {
          const header = this.shadowRoot!.querySelector('h1');
          if (header) {
            afterNextRender(this, () => header.focus());
          }
        }

        updateTitle(): void {
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

        onRouteChange(_route: Routes, _step: number): void {}
        onRouteEnter(): void {}
        onRouteExit(): void {}
        onRouteUnload(): void {}
      }

      return NavigationMixin;
    });

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
