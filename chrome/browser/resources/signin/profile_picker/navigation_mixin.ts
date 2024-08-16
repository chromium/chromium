// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import type {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {isBrowserSigninAllowed, isForceSigninEnabled, isSignInProfileCreationSupported} from './policy_helper.js';

/**
 * ProfilePickerPages enum.
 * These values are persisted to logs and should not be renumbered or
 * re-used.
 * See tools/metrics/histograms/enums.xml.
 */
enum Pages {
  MAIN_VIEW = 0,
  PROFILE_TYPE_CHOICE = 1,
  LOCAL_PROFILE_CUSTOMIZATION = 2,
  LOAD_SIGNIN = 3,
  LOAD_FORCE_SIGNIN = 4,
  PROFILE_SWITCH = 5,
  // <if expr="chromeos_lacros">
  ACCOUNT_SELECTION_LACROS = 6,
  // </if>
}

/**
 * Valid route pathnames.
 */
export enum Routes {
  MAIN = 'main-view',
  NEW_PROFILE = 'new-profile',
  PROFILE_SWITCH = 'profile-switch',
  // <if expr="chromeos_lacros">
  ACCOUNT_SELECTION_LACROS = 'account-selection-lacros',
  // </if>
}

/**
 * Valid profile creation flow steps.
 */
export enum ProfileCreationSteps {
  PROFILE_TYPE_CHOICE = 'profileTypeChoice',
  LOCAL_PROFILE_CUSTOMIZATION = 'localProfileCustomization',
  LOAD_SIGNIN = 'loadSignIn',
  LOAD_FORCE_SIGNIN = 'loadForceSignIn',
}

function computeStep(route: Routes): string {
  switch (route) {
    case Routes.MAIN:
      return 'mainView';
    case Routes.NEW_PROFILE:
      if (isForceSigninEnabled()) {
        return ProfileCreationSteps.LOAD_FORCE_SIGNIN;
      }
      // TODO(msalama): Adjust once sign in profile creation is supported.
      if (!isSignInProfileCreationSupported() || !isBrowserSigninAllowed()) {
        return ProfileCreationSteps.LOCAL_PROFILE_CUSTOMIZATION;
      }
      return ProfileCreationSteps.PROFILE_TYPE_CHOICE;
    case Routes.PROFILE_SWITCH:
      return 'profileSwitch';
    // <if expr="chromeos_lacros">
    case Routes.ACCOUNT_SELECTION_LACROS:
      return 'accountSelectionLacros';
    // </if>
    default:
      assertNotReached();
  }
}

// Sets up history state based on the url path, unless it's already set.
if (!history.state || !history.state.route || !history.state.step) {
  const path = window.location.pathname.replace(/\/$/, '');
  switch (path) {
    case `/${Routes.NEW_PROFILE}`:
      assert(history.length === 1);
      // Enable accessing the main page when navigating back.
      history.replaceState(
          {route: Routes.MAIN, step: computeStep(Routes.MAIN), isFirst: true},
          '', '/');
      history.pushState(
          {
            route: Routes.NEW_PROFILE,
            step: computeStep(Routes.NEW_PROFILE),
            isFirst: false,
          },
          '', path);
      break;
    case `/${Routes.PROFILE_SWITCH}`:
      history.replaceState(
          {
            route: Routes.PROFILE_SWITCH,
            step: computeStep(Routes.PROFILE_SWITCH),
            isFirst: true,
          },
          '', path);
      break;
    // <if expr="chromeos_lacros">
    case `/${Routes.ACCOUNT_SELECTION_LACROS}`:
      history.replaceState(
          {
            route: Routes.ACCOUNT_SELECTION_LACROS,
            step: computeStep(Routes.ACCOUNT_SELECTION_LACROS),
            isFirst: true,
          },
          '', path);
      break;
    // </if>
    default:
      history.replaceState(
          {route: Routes.MAIN, step: computeStep(Routes.MAIN), isFirst: true},
          '', '/');
  }
  recordPageVisited(history.state.step);
}

export function recordPageVisited(step: string) {
  let page = Pages.MAIN_VIEW;
  switch (step) {
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
      break;
    case ProfileCreationSteps.LOAD_FORCE_SIGNIN:
      page = Pages.LOAD_FORCE_SIGNIN;
      break;
    case 'profileSwitch':
      page = Pages.PROFILE_SWITCH;
      break;
    // <if expr="chromeos_lacros">
    case 'accountSelectionLacros':
      page = Pages.ACCOUNT_SELECTION_LACROS;
      break;
    // </if>
    default:
      assertNotReached();
  }
  chrome.metricsPrivate.recordEnumerationValue(
      'ProfilePicker.UiVisited', page, Object.keys(Pages).length);
}

const routeObservers: Set<NavigationMixinInterface> = new Set();

// Notifies all the elements that extended NavigationBehavior.
function notifyObservers() {
  const route = history.state.route;
  const step = history.state.step;
  recordPageVisited(step);
  routeObservers.forEach(observer => observer.onRouteChange(route, step));
}

// Notifies all elements when browser history is popped.
window.addEventListener('popstate', notifyObservers);

export function navigateTo(route: Routes) {
  assert([
    // <if expr="chromeos_lacros">
    Routes.ACCOUNT_SELECTION_LACROS,
    // </if>
    Routes.MAIN,
    Routes.NEW_PROFILE,
    Routes.PROFILE_SWITCH,
  ].includes(route));
  navigateToStep(route, computeStep(route));
}

/**
 * Navigates to the previous route if it belongs to the profile picker.
 */
export function navigateToPreviousRoute() {
  window.history.back();
}

/**
 * Returns whether there's a previous route. This is true iff some navigation
 * within the app already took place.
 */
export function hasPreviousRoute() {
  return !history.state.isFirst;
}

export function navigateToStep(route: Routes, step: string) {
  history.pushState(
      {
        route: route,
        step: step,
        isFirst: false,
      },
      '', route === Routes.MAIN ? '/' : `/${route}`);
  notifyObservers();
}

type Constructor<T> = new (...args: any[]) => T;

export const NavigationMixin =
    <T extends Constructor<CrLitElement>>(superClass: T): T&
    Constructor<NavigationMixinInterface> => {
      class NavigationMixin extends superClass {
        override connectedCallback() {
          super.connectedCallback();

          assert(!routeObservers.has(this));
          routeObservers.add(this);

          // history state was set when page loaded, so when the element first
          // attaches, call the route-change handler to initialize first.
          this.onRouteChange(history.state.route, history.state.step);
        }

        override disconnectedCallback() {
          super.disconnectedCallback();

          assert(routeObservers.delete(this));
        }

        /**
         * Elements can override onRouteChange to handle route changes.
         */
        onRouteChange(_route: Routes, _step: string) {}
      }

      return NavigationMixin;
    };

export interface NavigationMixinInterface {
  onRouteChange(route: Routes, step: string): void;
}
