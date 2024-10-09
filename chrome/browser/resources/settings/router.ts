// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import type {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {dedupingMixin} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from './i18n_setup.js';

/**
 * Specifies all possible routes in settings.
 */
export interface SettingsRoutes {
  ABOUT: Route;
  ACCESSIBILITY: Route;
  ADDRESSES: Route;
  ADVANCED: Route;
  AI: Route;
  AI_TAB_ORGANIZATION: Route;
  APPEARANCE: Route;
  AUTOFILL: Route;
  AUTOFILL_PREDICTION_IMPROVEMENTS: Route;
  BASIC: Route;
  CAPTIONS: Route;
  CERTIFICATES: Route;
  CHROME_CLEANUP: Route;
  CLEAR_BROWSER_DATA: Route;
  COOKIES: Route;
  DEFAULT_BROWSER: Route;
  DOWNLOADS: Route;
  EDIT_DICTIONARY: Route;
  FONTS: Route;
  HISTORY_SEARCH: Route;
  INCOMPATIBLE_APPLICATIONS: Route;
  LANGUAGES: Route;
  MANAGE_PROFILE: Route;
  OFFER_WRITING_HELP: Route;
  ON_STARTUP: Route;
  PAGE_CONTENT: Route;
  PASSKEYS: Route;
  PAYMENTS: Route;
  PEOPLE: Route;
  PERFORMANCE: Route;
  PRELOADING: Route;
  PRIVACY: Route;
  PRIVACY_GUIDE: Route;
  PRIVACY_SANDBOX: Route;
  PRIVACY_SANDBOX_AD_MEASUREMENT: Route;
  PRIVACY_SANDBOX_FLEDGE: Route;
  PRIVACY_SANDBOX_TOPICS: Route;
  PRIVACY_SANDBOX_MANAGE_TOPICS: Route;
  RESET: Route;
  RESET_DIALOG: Route;
  SAFETY_CHECK: Route;
  SAFETY_HUB: Route;
  SEARCH: Route;
  SEARCH_ENGINES: Route;
  SECURITY: Route;
  SECURITY_KEYS: Route;
  SECURITY_KEYS_PHONES: Route;
  SITE_SETTINGS: Route;
  SITE_SETTINGS_ADS: Route;
  SITE_SETTINGS_ALL: Route;
  SITE_SETTINGS_AR: Route;
  SITE_SETTINGS_AUTOMATIC_DOWNLOADS: Route;
  SITE_SETTINGS_AUTOMATIC_FULLSCREEN: Route;
  SITE_SETTINGS_AUTO_PICTURE_IN_PICTURE: Route;
  SITE_SETTINGS_AUTO_VERIFY: Route;
  SITE_SETTINGS_BACKGROUND_SYNC: Route;
  SITE_SETTINGS_BLUETOOTH_DEVICES: Route;
  SITE_SETTINGS_BLUETOOTH_SCANNING: Route;
  SITE_SETTINGS_CAMERA: Route;
  SITE_SETTINGS_CAPTURED_SURFACE_CONTROL: Route;
  SITE_SETTINGS_CLIPBOARD: Route;
  SITE_SETTINGS_COOKIES: Route;
  SITE_SETTINGS_FEDERATED_IDENTITY_API: Route;
  SITE_SETTINGS_HANDLERS: Route;
  SITE_SETTINGS_HAND_TRACKING: Route;
  SITE_SETTINGS_HID_DEVICES: Route;
  SITE_SETTINGS_IDLE_DETECTION: Route;
  SITE_SETTINGS_IMAGES: Route;
  SITE_SETTINGS_KEYBOARD_LOCK: Route;
  SITE_SETTINGS_LOCAL_FONTS: Route;
  SITE_SETTINGS_MIXEDSCRIPT: Route;
  SITE_SETTINGS_JAVASCRIPT: Route;
  SITE_SETTINGS_JAVASCRIPT_OPTIMIZER: Route;
  SITE_SETTINGS_SENSORS: Route;
  SITE_SETTINGS_SOUND: Route;
  SITE_SETTINGS_LOCATION: Route;
  SITE_SETTINGS_MICROPHONE: Route;
  SITE_SETTINGS_MIDI_DEVICES: Route;
  SITE_SETTINGS_FILE_SYSTEM_WRITE: Route;
  SITE_SETTINGS_FILE_SYSTEM_WRITE_DETAILS: Route;
  SITE_SETTINGS_NOTIFICATIONS: Route;
  SITE_SETTINGS_PAYMENT_HANDLER: Route;
  SITE_SETTINGS_PDF_DOCUMENTS: Route;
  SITE_SETTINGS_POINTER_LOCK: Route;
  SITE_SETTINGS_POPUPS: Route;
  SITE_SETTINGS_PROTECTED_CONTENT: Route;
  SITE_SETTINGS_SERIAL_PORTS: Route;
  SITE_SETTINGS_SMART_CARD_READERS: Route;
  SITE_SETTINGS_SITE_DATA: Route;
  SITE_SETTINGS_SITE_DETAILS: Route;
  SITE_SETTINGS_STORAGE_ACCESS: Route;
  SITE_SETTINGS_USB_DEVICES: Route;
  SITE_SETTINGS_VR: Route;
  SITE_SETTINGS_WEB_APP_INSTALLATION: Route;
  SITE_SETTINGS_WINDOW_MANAGEMENT: Route;
  SITE_SETTINGS_ZOOM_LEVELS: Route;
  SITE_SETTINGS_WEB_PRINTING: Route;
  SPELL_CHECK: Route;
  SYNC: Route;
  SYNC_ADVANCED: Route;
  SYSTEM: Route;
  TRACKING_PROTECTION: Route;
  TRIGGERED_RESET_DIALOG: Route;

  // <if expr="not chromeos_ash">
  IMPORT_DATA: Route;
  SIGN_OUT: Route;
  // </if>
}

/** Class for navigable routes. */
export class Route {
  path: string;
  parent: Route|null = null;
  depth: number = 0;
  title: string|undefined;

  /**
   * Whether this route corresponds to a navigable dialog. Those routes must
   * belong to a "section".
   */
  isNavigableDialog: boolean = false;

  // Legacy property to provide compatibility with the old routing system.
  section: string = '';

  constructor(path: string, title?: string) {
    this.path = path;
    this.title = title;
  }

  /**
   * Returns a new Route instance that's a child of this route.
   * @param path Extends this route's path if it doesn't contain a
   *     leading slash.
   */
  createChild(path: string, title?: string): Route {
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
   */
  createSection(path: string, section: string, title?: string): Route {
    const route = this.createChild(path, title);
    route.section = section;
    return route;
  }

  /**
   * Returns the absolute path string for this Route, assuming this function
   * has been called from within chrome://settings.
   */
  getAbsolutePath(): string {
    return window.location.origin + this.path;
  }

  /**
   * Returns true if this route matches or is an ancestor of the parameter.
   */
  contains(route: Route): boolean {
    for (let r: Route|null = route; r != null; r = r.parent) {
      if (this === r) {
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
const CANONICAL_PATH_REGEX: RegExp = /(^\/)([\/-\w]+)(\/$)/;

let routerInstance: Router|null = null;

export class Router {
  /**
   * List of available routes. This is populated taking into account current
   * state (like guest mode).
   */
  private routes_: SettingsRoutes;

  /**
   * The current active route. This updated is only by settings.navigateTo
   * or settings.initializeRouteFromUrl.
   */
  currentRoute: Route;

  /**
   * The current query parameters. This is updated only by
   * settings.navigateTo or settings.initializeRouteFromUrl.
   */
  private currentQueryParameters_: URLSearchParams = new URLSearchParams();

  private wasLastRouteChangePopstate_: boolean = false;

  private initializeRouteFromUrlCalled_: boolean = false;

  private routeObservers_: Set<RouteObserverMixinInterface> = new Set();

  /** @return The singleton instance. */
  static getInstance(): Router {
    assert(routerInstance);
    return routerInstance;
  }

  static setInstance(instance: Router) {
    assert(!routerInstance);
    routerInstance = instance;
  }

  static resetInstanceForTesting(instance: Router) {
    if (routerInstance) {
      instance.routeObservers_ = routerInstance.routeObservers_;
    }
    routerInstance = instance;
  }

  constructor(availableRoutes: SettingsRoutes) {
    this.routes_ = availableRoutes;
    this.currentRoute = this.routes_.BASIC;
  }

  addObserver(observer: RouteObserverMixinInterface) {
    assert(!this.routeObservers_.has(observer));
    this.routeObservers_.add(observer);
  }

  removeObserver(observer: RouteObserverMixinInterface) {
    assert(this.routeObservers_.delete(observer));
  }

  getRoute(routeName: string): Route {
    return this.routeDictionary_()[routeName];
  }

  getRoutes(): SettingsRoutes {
    return this.routes_;
  }

  /**
   * Helper function to set the current route and notify all observers.
   */
  setCurrentRoute(
      route: Route, queryParameters: URLSearchParams, isPopstate: boolean) {
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
   */
  private updateTitle_() {
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

  getCurrentRoute(): Route {
    return this.currentRoute;
  }

  getQueryParameters(): URLSearchParams {
    return new URLSearchParams(
        this.currentQueryParameters_);  // Defensive copy.
  }

  lastRouteChangeWasPopstate(): boolean {
    return this.wasLastRouteChangePopstate_;
  }

  private routeDictionary_(): {[key: string]: Route} {
    return this.routes_ as unknown as {[key: string]: Route};
  }

  /**
   * @return The matching canonical route, or null if none matches.
   */
  getRouteForPath(path: string): Route|null {
    // Allow trailing slash in paths.
    const canonicalPath = path.replace(CANONICAL_PATH_REGEX, '$1$2');

    // TODO(tommycli): Use Object.values once Closure compilation supports it.
    const matchingKey =
        Object.keys(this.routes_)
            .find((key) => this.routeDictionary_()[key].path === canonicalPath);

    return matchingKey ? this.routeDictionary_()[matchingKey] : null;
  }

  /**
   * Updates the URL parameters of the current route via exchanging the
   * window history state. This changes the Settings route path, but doesn't
   * change the route itself, hence does not push a new route history entry.
   * Notifies routeChangedObservers.
   */
  updateRouteParams(params: URLSearchParams) {
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
   * @param dynamicParameters Navigations to the same
   *     URL parameters in a different order will still push to history.
   * @param removeSearch Whether to strip the 'search' URL
   *     parameter during navigation. Defaults to false.
   */
  navigateTo(
      route: Route, dynamicParameters?: URLSearchParams,
      removeSearch: boolean = false) {
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
    let previousRoute = null;
    if (window.history.state) {
      previousRoute = this.getRouteForPath(window.history.state);
      assert(previousRoute);
    }

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
   * @param urlPath The url path (only).
   */
  recordMetrics(urlPath: string) {
    assert(!urlPath.startsWith('chrome://'));
    assert(!urlPath.startsWith('settings'));
    assert(urlPath.startsWith('/'));
    assert(!urlPath.match(/\?/g));

    const metricName = 'WebUI.Settings.PathVisited';
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

type Constructor<T> = new (...args: any[]) => T;

export const RouteObserverMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<RouteObserverMixinInterface> => {
      class RouteObserverMixin extends superClass implements
          RouteObserverMixinInterface {
        override connectedCallback() {
          super.connectedCallback();

          assert(routerInstance);
          routerInstance.addObserver(this);

          // Emulating Polymer data bindings, the observer is called when the
          // element starts observing the route.
          this.currentRouteChanged(routerInstance.currentRoute, undefined);
        }

        override disconnectedCallback() {
          super.disconnectedCallback();

          assert(routerInstance);
          routerInstance.removeObserver(this);
        }

        currentRouteChanged(_newRoute: Route, _oldRoute?: Route) {
          assertNotReached();
        }
      }
      return RouteObserverMixin;
    });

export interface RouteObserverMixinInterface {
  currentRouteChanged(newRoute: Route, oldRoute?: Route): void;
}
