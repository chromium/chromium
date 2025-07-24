// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface RouteObserver {
  onRouteChanged(pageName: string|null): void;
}

export class Router {
  private static instance: Router;
  private observers: Set<RouteObserver> = new Set();
  private currentPage: string|null = null;

  private constructor() {
    window.addEventListener('popstate', (event: PopStateEvent) => {
      const pageName = (event.state && event.state.page) ||
          new URLSearchParams(window.location.search).get('page');
      this.notifyObservers(pageName);
    });
  }

  static resetInstanceForTesting(): void {
    Router.instance = undefined as any;
  }

  static getInstance(): Router {
    if (!Router.instance) {
      Router.instance = new Router();
    }
    return Router.instance;
  }

  navigateTo(pageName: string) {
    if (this.currentPage === pageName) {
      return;
    }
    const url = new URL(window.location.href);
    url.searchParams.set('page', pageName);
    history.pushState({page: pageName}, '', url.toString());
    this.notifyObservers(pageName);
  }

  processInitialRoute(defaultPageFromDom: string) {
    const params = new URLSearchParams(window.location.search);
    const pageFromUrl = params.get('page');
    const targetPage = pageFromUrl || defaultPageFromDom;
    if (!pageFromUrl) {
      this.navigateTo(defaultPageFromDom);
    }
    this.notifyObservers(targetPage);
  }

  private notifyObservers(pageName: string|null) {
    if (this.currentPage === pageName) {
      return;
    }
    this.currentPage = pageName;
    for (const observer of this.observers) {
      observer.onRouteChanged(this.currentPage);
    }
  }

  addObserver(observer: RouteObserver) {
    this.observers.add(observer);
  }

  removeObserver(observer: RouteObserver) {
    this.observers.delete(observer);
  }
}
