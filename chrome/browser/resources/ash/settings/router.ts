// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {castExists} from './assert_extras.js';
import {isRevampWayfindingEnabled} from './common/load_time_booleans.js';
import {RouteObserverMixinInterface} from './common/route_observer_mixin.js';
import {createRoutes, OsSettingsRoutes, PATH_REDIRECTS, Route} from './os_settings_routes.js';

export {Route};

/**
 * Regular expression that captures the leading slash, the content and the
 * trailing slash in three different groups.
 */
const CANONICAL_PATH_REGEX = /(^\/)([\/-\w]+)(\/$)/;

/** The Router singleton instance. */
let routerInstance: Router|null = null;

/**
 * Represents the available set of routes for the Router singleton. This is
 * exported as a convenience to avoid always calling the more verbose way
 * `Router.getInstance().routes`.
 *
 * When the Router singleton is updated (i.e. from tests), this export should
 * also be updated to reflect the new set of routes.
 */
export let routes: OsSettingsRoutes;

export class Router {
  static getInstance(): Router {
    assert(routerInstance, 'Router instance has not been set yet.');
    return routerInstance;
  }

  static setInstance(instance: Router): void {
    assert(routerInstance === null, 'Router instance has already been set.');
    routerInstance = instance;
    routes = instance.routes;
  }

  static resetInstanceForTesting(instance: Router): void {
    if (routerInstance) {
      instance.routeObservers_ = routerInstance.routeObservers_;
    }
    routerInstance = instance;
    routes = instance.routes;
  }

  private currentRoute_: Route;
  private currentQueryParameters_: URLSearchParams;
  private initializeRouteFromUrlCalled_: boolean;
  private routes_: OsSettingsRoutes;
  private routeObservers_: Set<RouteObserverMixinInterface>;
  private lastRouteChangeWasPopstate_: boolean;

  constructor(availableRoutes: OsSettingsRoutes) {
    /**
     * List of available routes. This is populated taking into account current
     * state (like guest mode).
     */
    this.routes_ = availableRoutes;

    /**
     * The current active route. This updated is only by navigateTo() or
     * initializeRouteFromUrl().
     */
    this.currentRoute_ = this.routes_.BASIC;

    /**
     * The current query parameters. This is updated only by
     * settings.navigateTo or settings.initializeRouteFromUrl.
     */
    this.currentQueryParameters_ = new URLSearchParams();

    this.lastRouteChangeWasPopstate_ = false;

    this.initializeRouteFromUrlCalled_ = false;

    this.routeObservers_ = new Set<RouteObserverMixinInterface>();
  }

  // Convenience helper to index this.routes_ via bracket notation
  // e.g. this.routesMap_[routeName] // => Route
  private get routesMap_(): Record<string, Route> {
    return this.routes_ as unknown as Record<string, Route>;
  }

  get routes(): OsSettingsRoutes {
    return this.routes_;
  }

  get currentRoute(): Route {
    return this.currentRoute_;
  }

  addObserver(observer: RouteObserverMixinInterface): void {
    assert(!this.routeObservers_.has(observer));
    this.routeObservers_.add(observer);
  }

  removeObserver(observer: RouteObserverMixinInterface): void {
    assert(this.routeObservers_.delete(observer));
  }

  getRoute(route: string): Route {
    return this.routesMap_[route];
  }

  /**
   * Helper function to set the current route and notify all observers.
   */
  setCurrentRoute(
      route: Route, queryParameters: URLSearchParams,
      isPopstate: boolean): void {
    this.recordMetrics_(route.path);

    const prevRoute = this.currentRoute_;
    this.currentRoute_ = route;
    this.currentQueryParameters_ = queryParameters;
    this.lastRouteChangeWasPopstate_ = isPopstate;
    this.routeObservers_.forEach((observer) => {
      observer.currentRouteChanged(this.currentRoute_, prevRoute);
    });

    this.updateTitle_();
  }

  /**
   * Updates the page title to reflect the current route.
   */
  private updateTitle_(): void {
    if (this.currentRoute_.title) {
      document.title = loadTimeData.getStringF(
          'settingsAltPageTitle', this.currentRoute_.title);
    } else if (
        this.currentRoute_.isNavigableDialog &&
        this.currentRoute_.parent?.title) {
      document.title = loadTimeData.getStringF(
          'settingsAltPageTitle', this.currentRoute_.parent.title);
    } else if (
        !this.currentRoute_.isSubpage() &&
        !this.routes_.ABOUT.contains(this.currentRoute_)) {
      document.title = loadTimeData.getString('settings');
    }
  }

  getQueryParameters(): URLSearchParams {
    return new URLSearchParams(
        this.currentQueryParameters_);  // Defensive copy.
  }

  lastRouteChangeWasPopstate(): boolean {
    return this.lastRouteChangeWasPopstate_;
  }

  /**
   * @return a Route matching the |path| containing a leading "/",
   * or null if none matched.
   */
  getRouteForPath(path: string): Route|null {
    assert(path[0] === '/', 'Path must contain a leading slash.');

    // Remove any trailing slash.
    let canonicalPath = path.replace(CANONICAL_PATH_REGEX, '$1$2');

    // Handle redirects for obsolete paths.
    if (isRevampWayfindingEnabled()) {
      canonicalPath = PATH_REDIRECTS[canonicalPath] || canonicalPath;
    }

    const matchingRoute = Object.values(this.routes_).find(route => {
      return route.path === canonicalPath && isNavigableRoute(route);
    });
    return matchingRoute || null;
  }

  /**
   * Updates the URL parameters of the current route via exchanging the
   * window history state. This changes the Settings route path, but doesn't
   * change the route itself, hence does not push a new route history entry.
   * Notifies routeChangedObservers.
   */
  updateUrlParams(params: URLSearchParams): void {
    let url = this.currentRoute_.path;
    const queryString = params.toString();
    if (queryString) {
      url += '?' + queryString;
    }
    window.history.replaceState(window.history.state, '', url);

    // We can't call |setCurrentRoute()| for the following, as it would also
    // update |oldRoute| and |currentRoute|, which should not happen when
    // only the URL parameters are updated.
    this.currentQueryParameters_ = params;
    this.routeObservers_.forEach((observer) => {
      observer.currentRouteChanged(this.currentRoute_, this.currentRoute_);
    });
  }

  /**
   * Navigates to a canonical route and pushes a new history entry.
   * @param dynamicParameters Navigations to the same URL parameters in a
   *     different order will still push to history.
   * @param removeSearch Whether to strip the 'search' URL parameter during
   *     navigation. Defaults to false.
   */
  navigateTo(
      route: Route, dynamicParameters?: URLSearchParams,
      removeSearch?: boolean): void {
    if (!isNavigableRoute(route)) {
      route = this.routes_.BASIC;
    }

    const params = dynamicParameters || new URLSearchParams();

    const oldSearchParam = this.getQueryParameters().get('search') || '';
    const newSearchParam = params.get('search') || '';

    if (!removeSearch && oldSearchParam && !newSearchParam) {
      params.append('search', oldSearchParam);
    }

    let url = route.path;
    const queryString = params.toString();
    if (queryString) {
      url += '?' + queryString;
    }

    // History serializes the state, so we don't push the actual route object.
    window.history.pushState(this.currentRoute_.path, '', url);
    this.setCurrentRoute(route, params, false);
  }

  /**
   * Navigates to the previous route if it has an equal or lesser depth.
   * If there is no previous route in history meeting those requirements,
   * this navigates to the immediate parent. This will never exit Settings.
   */
  navigateToPreviousRoute(): void {
    let previousRoute = null;
    if (window.history.state) {
      previousRoute = castExists(this.getRouteForPath(window.history.state));
    }

    if (previousRoute && previousRoute.depth <= this.currentRoute_.depth) {
      window.history.back();
    } else {
      this.navigateTo(this.currentRoute_.parent || this.routes_.BASIC);
    }
  }

  /**
   * Initialize the route and query params from the URL.
   */
  initializeRouteFromUrl(): void {
    assert(
        !this.initializeRouteFromUrlCalled_,
        'initializeRouteFromUrl() can only be called once.');
    this.initializeRouteFromUrlCalled_ = true;

    const route = this.getRouteForPath(window.location.pathname);

    // Record all correct paths entered on the settings page, and
    // as all incorrect paths are routed to the main settings page,
    // record all incorrect paths as hitting the main settings page.
    this.recordMetrics_(route ? route.path : this.routes_.BASIC.path);

    if (route && isNavigableRoute(route)) {
      this.currentRoute_ = route;
      this.currentQueryParameters_ =
          new URLSearchParams(window.location.search);
    } else {
      window.history.replaceState(undefined, '', this.routes_.BASIC.path);
    }

    this.updateTitle_();
  }

  resetRouteForTesting(): void {
    this.initializeRouteFromUrlCalled_ = false;
    this.lastRouteChangeWasPopstate_ = false;
    this.currentRoute_ = this.routes_.BASIC;
    this.currentQueryParameters_ = new URLSearchParams();
  }

  /**
   * Make a UMA note about visiting this URL path.
   */
  private recordMetrics_(urlPath: string): void {
    assert(!urlPath.startsWith('chrome://'));
    assert(!urlPath.startsWith('os-settings'));
    assert(urlPath.startsWith('/'));
    assert(!urlPath.match(/\?/g));  // query params should not be included

    const METRIC_NAME = 'ChromeOS.Settings.PathVisited';
    chrome.metricsPrivate.recordSparseValueWithPersistentHash(
        METRIC_NAME, urlPath);
  }
}

/**
 * Creates a Router instance and returns it. Use `Router.setInstance()` with
 * this instance as an argument to set the singleton instance.
 *
 * Can be used from tests to re-create a Router with a new set of routes.
 */
export function createRouter(): Router {
  return new Router(createRoutes());
}

Router.setInstance(createRouter());

window.addEventListener('popstate', () => {
  // On pop state, do not push the state onto the window.history again.
  const router = Router.getInstance();
  router.setCurrentRoute(
      router.getRouteForPath(window.location.pathname) || routes.BASIC,
      new URLSearchParams(window.location.search), true);
});

/**
 * @returns true if this route exists under the Advanced section.
 */
export function isAdvancedRoute(route: Route|null): boolean {
  if (!route) {
    return false;
  }
  return routes.ADVANCED.contains(route);
}

/**
 * @returns true if this route exists under the Basic section (not advanced
 * section).
 */
export function isBasicRoute(route: Route|null): boolean {
  if (!route) {
    return false;
  }
  return routes.BASIC.contains(route);
}

/**
 * @returns true if this route exists under the About page
 */
export function isAboutRoute(route: Route|null): boolean {
  if (!route) {
    return false;
  }
  return routes.ABOUT.contains(route);
}

/**
 * @returns true if |route| is able to be directly navigated to (ie. there
 * is a dedicated page or subpage that exists for the given route).
 */
export function isNavigableRoute(route: Route|null): boolean {
  if (!route) {
    return false;
  }
  // The ADVANCED route is not navigable. It only serves as a parent to group
  // child routes.
  return route !== routes.ADVANCED;
}
