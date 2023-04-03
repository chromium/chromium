// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';

export interface RouteObserver {
  onRouteChanged(url: URL): void;
}

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

  static resetInstanceForTesting(newInstance: Router): void {
    routerInstance = newInstance;
  }

  private routeObservers: Set<RouteObserver>;

  constructor() {
    this.routeObservers = new Set<RouteObserver>();
  }

  addObserver(observer: RouteObserver): void {
    assert(!this.routeObservers.has(observer));
    this.routeObservers.add(observer);
  }

  removeObserver(observer: RouteObserver): void {
    assert(this.routeObservers.delete(observer));
  }

  /**
   * Navigates to the given route, and notifies observers.
   */
  navigateTo(url: URL): void {
    window.history.pushState({}, '', url);
    this.routeObservers.forEach((observer) => {
      observer.onRouteChanged(url);
    });
  }
}

Router.setInstance(new Router());
