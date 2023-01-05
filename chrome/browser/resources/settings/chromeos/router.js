// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';

/**
 * @typedef {{
 *   BASIC: !Route,
 *   ADVANCED: !Route,
 *   ABOUT: !Route,
 * }}
 */
export let MinimumRoutes;

/** Class for navigable routes. */
export class Route {
  /**
   * @param {string} path
   * @param {string=} title
   */
  constructor(path, title) {
    /** @type {string} */
    this.path = path;

    /** @type {?Route} */
    this.parent = null;

    /** @type {number} */
    this.depth = 0;

    /** @type {string|undefined} */
    this.title = title;

    /**
     * @type {boolean} Whether this route corresponds to a navigable
     *     dialog. Those routes must belong to a "section".
     */
    this.isNavigableDialog = false;

    // Below are all legacy properties to provide compatibility with the old
    // routing system.

    /** @type {string} */
    this.section = '';
  }

  /**
   * Returns a new Route instance that's a child of this route.
   * @param {string} path Extends this route's path if it doesn't contain a
   *     leading slash.
   * @param {string=} title
   * @return {!Route}
   */
  createChild(path, title) {
    assert(path);

    // |path| extends this route's path if it doesn't have a leading slash.
    // If it does have a leading slash, it's just set as the new route's URL.
    const newUrl = path[0] === '/' ? path : `${this.path}/${path}`;

    const route = new Route(newUrl, title);
    route.parent = this;
    route.section = this.section;
    route.depth = this.depth + 1;

    return route;
  }

  /**
   * Returns a new Route instance that's a child section of this route.
   * TODO(tommycli): Remove once we've obsoleted the concept of sections.
   * @param {string} path
   * @param {string} section
   * @param {string=} title
   * @return {!Route}
   */
  createSection(path, section, title) {
    const route = this.createChild(path, title);
    route.section = section;
    return route;
  }

  /**
   * Returns the absolute path string for this Route, assuming this function
   * has been called from within chrome://settings.
   * @return {string}
   */
  getAbsolutePath() {
    return window.location.origin + this.path;
  }

  /**
   * Returns true if this route matches or is an ancestor of the parameter.
   * @param {!Route} route
   * @return {boolean}
   */
  contains(route) {
    for (let r = route; r != null; r = r.parent) {
      if (this === r) {
        return true;
      }
    }
    return false;
  }

  /**
   * Returns true if this route is a subpage of a section.
   * @return {boolean}
   */
  isSubpage() {
    return !this.isNavigableDialog && !!this.parent && !!this.section &&
        this.parent.section === this.section;
  }
}

/**
 * Regular expression that captures the leading slash, the content and the
 * trailing slash in three different groups.
 * @type {!RegExp}
 */
const CANONICAL_PATH_REGEX = /(^\/)([\/-\w]+)(\/$)/;

/** @type {?Router} */
let routerInstance = null;

export class Router {
  /** @return {!Router} The singleton instance. */
  static getInstance() {
    return assert(routerInstance);
  }

  /** @param {!Router} instance */
  static setInstance(instance) {
    assert(!routerInstance);
    routerInstance = instance;
  }

  /** @param {!Router} instance */
  static resetInstanceForTesting(instance) {
    if (routerInstance) {
      instance.routeObservers_ = routerInstance.routeObservers_;
    }
    routerInstance = instance;
  }

  /** @param {!MinimumRoutes} availableRoutes */
  constructor(availableRoutes) {
    /**
     * List of available routes. This is populated taking into account current
     * state (like guest mode).
     * @private {!MinimumRoutes}
     */
    this.routes_ = availableRoutes;

    /**
     * The current active route. This updated is only by settings.navigateTo
     * or settings.initializeRouteFromUrl.
     * @type {!Route}
     */
    this.currentRoute = this.routes_.BASIC;

    /**
     * The current query parameters. This is updated only by
     * settings.navigateTo or settings.initializeRouteFromUrl.
     * @private {!URLSearchParams}
     */
    this.currentQueryParameters_ = new URLSearchParams();

    /** @private {boolean} */
    this.wasLastRouteChangePopstate_ = false;

    /** @private {boolean}*/
    this.initializeRouteFromUrlCalled_ = false;

    /** @private {!Set<Object>} */
    this.routeObservers_ = new Set();
  }

  /** @param {Object} observer */
  addObserver(observer) {
    assert(!this.routeObservers_.has(observer));
    this.routeObservers_.add(observer);
  }

  /** @param {Object} observer */
  removeObserver(observer) {
    assert(this.routeObservers_.delete(observer));
  }

  /** @return {Route} */
  getRoute(routeName) {
    return this.routes_[routeName];
  }

  /** @return {!Object} */
  getRoutes() {
    return this.routes_;
  }

  /**
   * Helper function to set the current route and notify all observers.
   * @param {!Route} route
   * @param {!URLSearchParams} queryParameters
   * @param {boolean} isPopstate
   */
  setCurrentRoute(route, queryParameters, isPopstate) {
    this.recordMetrics(route.path);

    const oldRoute = this.currentRoute;
    this.currentRoute = route;
    this.currentQueryParameters_ = queryParameters;
    this.wasLastRouteChangePopstate_ = isPopstate;
    new Set(this.routeObservers_).forEach((observer) => {
      observer.currentRouteChanged(this.currentRoute, oldRoute);
    });

    this.updateTitle_();
  }

  /**
   * Updates the page title to reflect the current route.
   * @private
   */
  updateTitle_() {
    if (this.currentRoute.title) {
      document.title = loadTimeData.getStringF(
          'settingsAltPageTitle', this.currentRoute.title);
    } else if (
        this.currentRoute.isNavigableDialog && this.currentRoute.parent &&
        this.currentRoute.parent.title) {
      document.title = loadTimeData.getStringF(
          'settingsAltPageTitle', this.currentRoute.parent.title);
    } else if (
        !this.currentRoute.isSubpage() &&
        !this.routes_.ABOUT.contains(this.currentRoute)) {
      document.title = loadTimeData.getString('settings');
    }
  }

  /** @return {!Route} */
  getCurrentRoute() {
    return this.currentRoute;
  }

  /** @return {!URLSearchParams} */
  getQueryParameters() {
    return new URLSearchParams(
        this.currentQueryParameters_);  // Defensive copy.
  }

  /** @return {boolean} */
  lastRouteChangeWasPopstate() {
    return this.wasLastRouteChangePopstate_;
  }

  /**
   * @param {string} path
   * @return {?Route} The matching canonical route, or null if none
   *     matches.
   */
  getRouteForPath(path) {
    // Allow trailing slash in paths.
    const canonicalPath = path.replace(CANONICAL_PATH_REGEX, '$1$2');

    // TODO(tommycli): Use Object.values once Closure compilation supports it.
    const matchingKey =
        Object.keys(this.routes_)
            .find((key) => this.routes_[key].path === canonicalPath);

    return matchingKey ? this.routes_[matchingKey] : null;
  }

  /**
   * Updates the URL parameters of the current route via exchanging the
   * window history state. This changes the Settings route path, but doesn't
   * change the route itself, hence does not push a new route history entry.
   * Notifies routeChangedObservers.
   * @param {!URLSearchParams} params
   */
  updateRouteParams(params) {
    let url = this.currentRoute.path;
    const queryString = params.toString();
    if (queryString) {
      url += '?' + queryString;
    }
    window.history.replaceState(window.history.state, '', url);

    // We can't call |setCurrentRoute()| for the following, as it would also
    // update |oldRoute| and |currentRoute|, which should not happen when
    // only the URL parameters are updated.
    this.currentQueryParameters_ = params;
    new Set(this.routeObservers_).forEach((observer) => {
      observer.currentRouteChanged(this.currentRoute, this.currentRoute);
    });
  }

  /**
   * Navigates to a canonical route and pushes a new history entry.
   * @param {!Route} route
   * @param {URLSearchParams=} dynamicParameters Navigations to the same
   *     URL parameters in a different order will still push to history.
   * @param {boolean=} removeSearch Whether to strip the 'search' URL
   *     parameter during navigation. Defaults to false.
   */
  navigateTo(route, dynamicParameters, removeSearch = false) {
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
    window.history.pushState(this.currentRoute.path, '', url);
    this.setCurrentRoute(route, params, false);
  }

  /**
   * Navigates to the previous route if it has an equal or lesser depth.
   * If there is no previous route in history meeting those requirements,
   * this navigates to the immediate parent. This will never exit Settings.
   */
  navigateToPreviousRoute() {
    const previousRoute = window.history.state &&
        assert(this.getRouteForPath(
            /** @type {string} */ (window.history.state)));

    if (previousRoute && previousRoute.depth <= this.currentRoute.depth) {
      window.history.back();
    } else {
      this.navigateTo(this.currentRoute.parent || this.routes_.BASIC);
    }
  }

  /**
   * Initialize the route and query params from the URL.
   */
  initializeRouteFromUrl() {
    assert(!this.initializeRouteFromUrlCalled_);
    this.initializeRouteFromUrlCalled_ = true;

    const route = this.getRouteForPath(window.location.pathname);

    // Record all correct paths entered on the settings page, and
    // as all incorrect paths are routed to the main settings page,
    // record all incorrect paths as hitting the main settings page.
    this.recordMetrics(route ? route.path : this.routes_.BASIC.path);

    // Never allow direct navigation to ADVANCED.
    if (route && route !== this.routes_.ADVANCED) {
      this.currentRoute = route;
      this.currentQueryParameters_ =
          new URLSearchParams(window.location.search);
    } else {
      window.history.replaceState(undefined, '', this.routes_.BASIC.path);
    }

    this.updateTitle_();
  }

  /**
   * Make a UMA note about visiting this URL path.
   * @param {string} urlPath The url path (only).
   */
  recordMetrics(urlPath) {
    assert(!urlPath.startsWith('chrome://'));
    assert(!urlPath.startsWith('settings'));
    assert(urlPath.startsWith('/'));
    assert(!urlPath.match(/\?/g));

    const metricName = 'ChromeOS.Settings.PathVisited';
    chrome.metricsPrivate.recordSparseValueWithPersistentHash(
        metricName, urlPath);
  }

  resetRouteForTesting() {
    this.initializeRouteFromUrlCalled_ = false;
    this.wasLastRouteChangePopstate_ = false;
    this.currentRoute = this.routes_.BASIC;
    this.currentQueryParameters_ = new URLSearchParams();
  }
}
