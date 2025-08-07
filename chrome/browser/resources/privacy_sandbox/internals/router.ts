// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface RouteObserver {
  onRouteChanged(pageName: string|null, searchQuery: string|null): void;
}

export class Router {
  private static instance: Router;
  private observers: Set<RouteObserver> = new Set();
  private currentPage: string|null = null;
  private currentSearchQuery: string|null = null;

  private constructor() {
    window.addEventListener('popstate', () => {
      const {page, searchQuery} = this.getRouteFromUrl();
      this.setRoute(page, searchQuery);
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
    if (pageName === this.currentPage) {
      return;
    }
    const url = new URL(window.location.href);
    url.searchParams.set('page', pageName);
    url.searchParams.delete('search');
    history.pushState({page: pageName}, '', url.toString());
    this.setRoute(pageName, null);
  }

  setSearchQuery(query: string) {
    const newQuery = query?.trim() || null;
    if (newQuery === this.currentSearchQuery) {
      return;
    }

    const url = new URL(window.location.href);

    if (newQuery) {
      url.searchParams.set('search', newQuery);
    } else {
      url.searchParams.delete('search');
    }
    // Use replaceState to avoid polluting history for every character typed
    history.replaceState(
        {page: this.currentPage, search: newQuery}, '', url.toString());
    this.setRoute(this.currentPage, newQuery);
  }

  processInitialRoute(defaultPage: string) {
    const routeParams = this.getRouteFromUrl();
    let page = routeParams.page;
    const searchQuery = routeParams.searchQuery;

    // If no page is in the URL, use the default and update the URL.
    if (!page) {
      page = defaultPage;
      const url = new URL(window.location.href);
      url.searchParams.set('page', page);
      history.replaceState(
          {page: page, search: searchQuery}, '', url.toString());
    }

    this.setRoute(page, searchQuery);
  }

  addObserver(observer: RouteObserver) {
    this.observers.add(observer);
  }

  removeObserver(observer: RouteObserver) {
    this.observers.delete(observer);
  }

  // Reads the page and search query from the current window URL.
  private getRouteFromUrl(): {page: string|null, searchQuery: string|null} {
    const params = new URLSearchParams(window.location.search);
    const page = params.get('page');
    const searchQuery = params.get('search');
    return {page, searchQuery};
  }

  // Sets the internal state and notifies observers of the route change.
  private setRoute(pageName: string|null, searchQuery: string|null) {
    if (this.currentPage === pageName &&
        this.currentSearchQuery === searchQuery) {
      return;
    }
    this.currentPage = pageName;
    this.currentSearchQuery = searchQuery;
    for (const observer of this.observers) {
      observer.onRouteChanged(this.currentPage, this.currentSearchQuery);
    }
  }
}
