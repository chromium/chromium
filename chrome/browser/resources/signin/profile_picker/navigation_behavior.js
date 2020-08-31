// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';

import {isForceSigninEnabled, isSignInProfileCreationSupported} from './policy_helper.js';

/**
 * ProfilePickerPages enum.
 * These values are persisted to logs and should not be renumbered or
 * re-used.
 * See tools/metrics/histograms/enums.xml.
 * @enum {number}
 */
const Pages = {
  MAIN_VIEW: 0,
  PROFILE_TYPE_CHOICE: 1,
  LOCAL_PROFILE_CUSTOMIZATION: 2,
  LOAD_SIGNIN: 3,
};

/**
 * Valid route pathnames.
 * @enum {string}
 */
export const Routes = {
  MAIN: 'main-view',
  NEW_PROFILE: 'new-profile',
};

/**
 * Valid profile creation flow steps.
 * @enum {string}
 */
export const ProfileCreationSteps = {
  PROFILE_TYPE_CHOICE: 'profileTypeChoice',
  // Not supported yet
  LOCAL_PROFILE_CUSTOMIZATION: 'localProfileCustomization',
  // Not supported yet
  LOAD_SIGNIN: 'loadSignIn',
};

/**
 * @param {!Routes} route
 */
function computeStep(route) {
  switch (route) {
    case Routes.MAIN:
      return 'mainView';
    case Routes.NEW_PROFILE:
      // TODO(msalama): Adjust once sign in profile creation is supported.
      // Check DisallowSignIn policy.
      if (!isSignInProfileCreationSupported()) {
        assert(!isForceSigninEnabled());
        return ProfileCreationSteps.LOCAL_PROFILE_CUSTOMIZATION;
      }
      if (isForceSigninEnabled()) {
        return ProfileCreationSteps.LOAD_SIGNIN;
      }
      return ProfileCreationSteps.PROFILE_TYPE_CHOICE;
    default:
      assertNotReached();
  }
}

// Sets up history state based on the url path, unless it's already set.
if (!history.state || !history.state.route || !history.state.step) {
  const path = window.location.pathname.replace(/\/$/, '');
  switch (path) {
    case `/${Routes.NEW_PROFILE}`:
      history.replaceState(
          {
            route: Routes.NEW_PROFILE,
            step: computeStep(Routes.NEW_PROFILE),
            isFirst: true,
          },
          '', path);
      break;
    default:
      history.replaceState(
          {route: Routes.MAIN, step: computeStep(Routes.MAIN), isFirst: true},
          '', '/');
  }
  recordNavigation();
}


function recordNavigation() {
  let page = /** @type {!Pages} */ (Pages.MAIN_VIEW);
  switch (history.state.step) {
    case 'mainView':
      page = Pages.MAIN_VIEW;
      break;
    case ProfileCreationSteps.PROFILE_TYPE_CHOICE:
      page = Pages.PROFILE_TYPE_CHOICE;
      break;
    case ProfileCreationSteps.LOCAL_PROFILE_CUSTOMIZATION:
      page = Pages.LOCAL_PROFILE_CUSTOMIZATION;
      break;
    case ProfileCreationSteps.LOAD_SIGNIN:
      page = Pages.LOAD_SIGNIN;
    default:
      assertNotReached();
  }
  chrome.metricsPrivate.recordEnumerationValue(
      'ProfilePicker.UiVisited', page, Object.keys(Pages).length);
}

/** @type {!Set<!PolymerElement>} */
const routeObservers = new Set();

// Notifies all the elements that extended NavigationBehavior.
function notifyObservers() {
  const route = /** @type {!Routes} */ (history.state.route);
  const step = history.state.step;
  routeObservers.forEach(observer => {
    (/** @type {{onRouteChange: Function}} */ (observer))
        .onRouteChange(route, step);
  });
}

// Notifies all elements when browser history is popped.
window.addEventListener('popstate', notifyObservers);

/**
 * @param {!Routes} route
 */
export function navigateTo(route) {
  assert([Routes.MAIN, Routes.NEW_PROFILE].includes(route));
  navigateToStep(route, computeStep(route));
}

/**
 * Navigates to the previous route if it belongs to the profile picker
 * otherwise to the main route.
 */
export function navigateToPreviousRoute() {
  // This can happen if the profile creation flow is opened directly from the
  // profile menu.
  if (history.state.isFirst) {
    navigateTo(Routes.MAIN);
  } else {
    window.history.back();
  }
}

/**
 * @param {!Routes} route
 * @param {string} step
 */
export function navigateToStep(route, step) {
  history.pushState(
      {
        route: route,
        step: step,
        isFirst: false,
      },
      '', route === Routes.MAIN ? '/' : `/${route}`);
  notifyObservers();
}

/** @polymerBehavior */
export const NavigationBehavior = {
  /** @override */
  attached() {
    assert(!routeObservers.has(this));
    routeObservers.add(this);

    // history state was set when page loaded, so when the element first
    // attaches, call the route-change handler to initialize first.
    this.onRouteChange(history.state.route, history.state.step);
  },

  /** @override */
  detached: function() {
    assert(routeObservers.delete(this));
  },

  /**
   * Elements can override onRouteChange to handle route changes.
   * @param {Routes} route
   * @param {string} step
   */
  onRouteChange: function(route, step) {},
};
