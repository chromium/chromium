// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {castExists} from './assert_extras.js';
import {OsSettingsRoutes} from './os_settings_routes.js';
import {RouteObserverMixinInterface} from './route_observer_mixin.js';

/** Class for navigable routes. */
export class Route {
  depth: number;
  isNavigableDialog: boolean;
  section: string;
  title: string|undefined;
  parent: Route|null;
  path: string;

  constructor(path: string, title?: string) {
    this.path = path;
    this.title = title;
    this.parent = null;
    this.depth = 0;

    /**
     * Whether this route corresponds to a navigable dialog. Those routes must
     * belong to a "section".
     */
    this.isNavigableDialog = false;

    // Below are all legacy properties to provide compatibility with the old
    // routing system.
    this.section = '';
  }

  /**
   * Returns a new Route instance that's a child of this route.
   */
  createChild(path: string, title?: string): Route {
    assert(path);

    // |path| extends this route's path if it doesn't have a leading slash.
    // If it does have a leading slash, it's just set as the child route's path
    const childPath = path[0] === '/' ? path : `${this.path}/${path}`;

    const route = new Route(childPath, title);
    route.parent = this;
    route.section = this.section;
    route.depth = this.depth + 1;
    return route;
  }

  /**
   * Returns a new Route instance that's a child section of this route.
   * TODO(tommycli): Remove once we've obsoleted the concept of sections.
   */
  createSection(path: string, section: string, title?: string): Route {
    const route = this.createChild(path, title);
    route.section = section;
    return route;
  }

  /**
   * Returns the absolute path string for this Route, assuming this function
   * has been called from within chrome://os-settings.
   */
  getAbsolutePath(): string {
    return window.location.origin + this.path;
  }

  /**
   * Returns true if this route matches or is an ancestor of the parameter.
   */
  contains(route: Route): boolean {
    for (let curr: Route|null = route; curr !== null; curr = curr.parent) {
      if (this === curr) {
        return true;
      }
    }
    return false;
  }

  /**
   * Returns true if this route is a subpage of a section.
   */
  isSubpage(): boolean {
    return !this.isNavigableDialog && !!this.parent && !!this.section &&
        this.parent.section === this.section;
  }
}

/**
 * Regular expression that captures the leading slash, the content and the
 * trailing slash in three different groups.
 */
const CANONICAL_PATH_REGEX = /(^\/)([\/-\w]+)(\/$)/;

/** The singleton instance. */
let routerInstance: Router|null = null;

export class Router {
  static getInstance(): Router {
    assert(routerInstance, 'Router instance has not been set yet.');
    return routerInstance;
  }

  static setInstance(instance: Router): void {
    assert(routerInstance === null, 'Router instance has already been set.');
    routerInstance = instance;
  }

  static resetInstanceForTesting(instance: Router): void {
    if (routerInstance) {
      instance.routeObservers_ = routerInstance.routeObservers_;
    }
    routerInstance = instance;
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

  getRouteForPath(path: string): Route|null {
    // Allow trailing slash in paths.
    const canonicalPath = path.replace(CANONICAL_PATH_REGEX, '$1$2');

    const matchingRoute = Object.values(this.routesMap_)
                              .find(route => route.path === canonicalPath);
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
    // The ADVANCED route only serves as a parent of subpages, and should not
    // be possible to navigate to it directly.
    if (route === this.routes_.ADVANCED) {
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

    // Never allow direct navigation to ADVANCED.
    if (route && route !== this.routes_.ADVANCED) {
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
