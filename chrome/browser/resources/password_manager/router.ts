// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * The different pages that can be shown at a time.
 */
export enum Page {
  PASSWORDS = 'passwords',
  CHECKUP = 'checkup',
  SETTINGS = 'settings',
}

/**
 * A helper object to manage in-page navigations. Since the Password Manager
 * page needs to support different urls for different subpages (like the checkup
 * page), we use this object to manage the history and url conversions.
 */
export class Router {
  static getInstance(): Router {
    return routerInstance || (routerInstance = new Router());
  }

  private currentPage_: Page = Page.PASSWORDS;
  private routeObservers_: Set<RouteObserverMixinInterface> = new Set();

  constructor() {
    this.processRoute_();

    window.addEventListener('popstate', () => {
      this.processRoute_();
    });
  }

  addObserver(observer: RouteObserverMixinInterface) {
    assert(!this.routeObservers_.has(observer));
    this.routeObservers_.add(observer);
  }

  removeObserver(observer: RouteObserverMixinInterface) {
    assert(this.routeObservers_.delete(observer));
  }

  get currentPage(): Page {
    return this.currentPage_;
  }

  /**
   * Navigates to a page and pushes a new history entry.
   */
  navigateTo(page: Page) {
    if (page === this.currentPage_) {
      return;
    }

    this.currentPage_ = page;
    const path = '/' + page;
    const state = {url: path};
    history.pushState(state, '', path);
    this.notifyObservers_();
  }

  private notifyObservers_() {
    this.routeObservers_.forEach((observer) => {
      observer.currentRouteChanged(this.currentPage_);
    });
  }

  /**
   * Helper function to set the current page and notify all observers.
   */
  private processRoute_() {
    const section = location.pathname.substring(1).split('/')[0] || '';

    switch (section) {
      case Page.PASSWORDS:
        this.currentPage_ = Page.PASSWORDS;
        break;
      case Page.CHECKUP:
        this.currentPage_ = Page.CHECKUP;
        break;
      case Page.SETTINGS:
        this.currentPage_ = Page.SETTINGS;
        break;
      default:
        history.replaceState({}, '', this.currentPage_);
    }
    this.notifyObservers_();
  }
}

let routerInstance: Router|null = null;

type Constructor<T> = new (...args: any[]) => T;

export const RouteObserverMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<RouteObserverMixinInterface> => {
      class RouteObserverMixin extends superClass {
        override connectedCallback() {
          super.connectedCallback();

          Router.getInstance().addObserver(this);

          this.currentRouteChanged(Router.getInstance().currentPage);
        }

        override disconnectedCallback() {
          super.disconnectedCallback();

          Router.getInstance().removeObserver(this);
        }

        currentRouteChanged(_: Page): void {
          assertNotReached();
        }
      }

      return RouteObserverMixin;
    });

export interface RouteObserverMixinInterface {
  currentRouteChanged(page: Page): void;
}
