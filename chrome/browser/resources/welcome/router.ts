// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import type {NavigationMixinInterface} from './navigation_mixin.js';

/**
 * Valid route pathnames.
 */
export enum Routes {
  LANDING = 'landing',
  NEW_USER = 'new-user',
  RETURNING_USER = 'returning-user'
}

export const routeObservers: Set<NavigationMixinInterface> = new Set();

let currentRouteElement: NavigationMixinInterface|null;

export function setCurrentRouteElement(element: NavigationMixinInterface) {
  currentRouteElement = element;
}

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

function main() {
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

  // Notifies all elements when browser history is popped.
  window.addEventListener('popstate', notifyObservers);

  // Notify the active element before unload.
  window.addEventListener('beforeunload', () => {
    if (currentRouteElement) {
      (currentRouteElement as {onRouteUnload: Function}).onRouteUnload();
    }
  });
}
main();
