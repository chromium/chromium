// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface RouteObserver {
  onRouteChanged(pageName: string|null): void;
}

export class Router {
  private observers: Set<RouteObserver> = new Set();
  private currentPage: string|null = null;

  private constructor() {
    window.addEventListener('popstate', () => {
      const page = this.getRouteFromUrl();
      this.setRoute(page);
    });
  }

  navigateTo(pageName: string) {
    if (pageName === this.currentPage) {
      return;
    }
    const url = new URL(window.location.href);
    url.searchParams.set('page', pageName);
    history.pushState({page: pageName}, '', url.toString());
    this.setRoute(pageName);
  }

  processInitialRoute(defaultPage: string) {
    let page = this.getRouteFromUrl();

    // If no page is in the URL, use the default and update the URL.
    if (!page) {
      page = defaultPage;
      const url = new URL(window.location.href);
      url.searchParams.set('page', page);
      history.replaceState({page: page}, '', url.toString());
    }

    this.setRoute(page);
  }

  addObserver(observer: RouteObserver) {
    this.observers.add(observer);
  }

  removeObserver(observer: RouteObserver) {
    this.observers.delete(observer);
  }

  // Reads the page from the current window URL.
  private getRouteFromUrl(): string|null {
    const params = new URLSearchParams(window.location.search);
    return params.get('page');
  }

  // Sets the internal state and notifies observers of the route change.
  private setRoute(pageName: string|null) {
    if (this.currentPage === pageName) {
      return;
    }
    this.currentPage = pageName;
    for (const observer of this.observers) {
      observer.onRouteChanged(this.currentPage);
    }
  }

  static getInstance(): Router {
    return instance || (instance = new Router());
  }

  static resetInstanceForTesting(): void {
    instance = null;
  }
}

let instance: Router|null = null;
