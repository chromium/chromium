// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';

import {Page} from './page.js';

/**
 * PageManager contains a list of root Page objects and handles "navigation"
 * by showing and hiding these pages. On initial load, PageManager can use
 * the path to open the correct hierarchy of pages.
 */
export class PageManager {
  /**
   * True if page is served from a dialog.
   */
  isDialog: boolean = false;

  /**
   * Root pages. Maps lower-case page names to the respective page object.
   */
  registeredPages: Map<string, Page> = new Map();

  /**
   * Observers will be notified when opening and closing overlays.
   */
  private observers_: PageManagerObserver[] = [];

  private defaultPage_: Page|null = null;

  constructor() {}

  /**
   * Initializes the complete page.
   * @param defaultPage The page to be shown when no
   *     page is specified in the path.
   */
  initialize(defaultPage: Page) {
    this.defaultPage_ = defaultPage;

    FocusOutlineManager.forDocument(document);
  }

  /**
   * Registers new page.
   * @param page Page to register.
   */
  register(page: Page) {
    this.registeredPages.set(page.name.toLowerCase(), page);
    page.addEventListener(
        'page-hash-changed', e => this.onPageHashChanged_(e as CustomEvent));
    page.initializePage();
  }

  /**
   * Unregisters an existing page.
   * @param page Page to unregister.
   */
  unregister(page: Page) {
    this.registeredPages.delete(page.name.toLowerCase());
  }

  /**
   * Shows the default page.
   * @param updateHistory If we should update the history after showing the page
   *     (defaults to true).
   */
  showDefaultPage(updateHistory?: boolean) {
    assert(
        this.defaultPage_ instanceof Page,
        'PageManager must be initialized with a default page.');
    this.showPageByName(this.defaultPage_.name, updateHistory);
  }

  /**
   * Shows a registered page.
   * @param pageName Page name.
   * @param updateHistory If we should update the history after showing the page
   *     (defaults to true).
   * @param propertyBag An optional bag of properties including replaceState (if
   *     history state should be replaced instead of pushed). hash (a hash state
   *     to attach to the page).
   */
  showPageByName(pageName: string, updateHistory: boolean = true, propertyBag: {
    replaceState?: boolean,
    hash?: string,
  } = {}) {
    // Find the currently visible root-level page.
    let rootPage: Page|null = null;
    for (const page of this.registeredPages.values()) {
      if (page.visible && !page.parentPage) {
        rootPage = page;
        break;
      }
    }

    // Find the target page.
    const targetPage =
        this.registeredPages.get(pageName.toLowerCase()) ?? this.defaultPage_!;

    pageName = targetPage.name.toLowerCase();
    const targetPageWasVisible = targetPage.visible;

    // Notify pages if they will be hidden.
    this.registeredPages.forEach(page => {
      if (page.name !== pageName && !this.isAncestorOfPage(page, targetPage)) {
        page.willHidePage();
      }
    });

    // Update the page's hash.
    targetPage.hash = propertyBag.hash || '';

    // Update visibilities to show only the hierarchy of the target page.
    this.registeredPages.forEach(page => {
      page.visible =
          page.name === pageName || this.isAncestorOfPage(page, targetPage);
    });

    // Update the history and current location.
    if (updateHistory) {
      this.updateHistoryState_(!!propertyBag.replaceState);
    }

    // Update focus if any other control was focused on the previous page,
    // or the previous page is not known.
    if (document.activeElement !== document.body &&
        (!rootPage || rootPage.pageDiv.contains(document.activeElement))) {
      targetPage.focus();
    }

    // Notify pages if they were shown.
    this.registeredPages.forEach(page => {
      if (!targetPageWasVisible &&
          (page.name === pageName || this.isAncestorOfPage(page, targetPage))) {
        page.didShowPage();
      }
    });

    // If the target page was already visible, notify it that its hash
    // changed externally.
    if (targetPageWasVisible) {
      targetPage.didChangeHash();
    }

    // Update the document title. Do this after didShowPage was called, in
    // case a page decides to change its title.
    this.updateTitle_();
  }

  /**
   * Returns the name of the page from the current path.
   * @return Name of the page specified by the current path.
   */
  getPageNameFromPath(): string {
    const path = location.pathname;
    if (path.length <= 1) {
      return this.defaultPage_!.name;
    }

    // Skip starting slash and remove trailing slash (if any).
    return path.slice(1).replace(/\/$/, '');
  }

  /**
   * Gets the level of the page. Root pages (e.g., BrowserOptions) are at
   * level 0.
   * @return How far down this page is from the root page.
   */
  getNestingLevel(page: Page): number {
    let level = 0;
    let parent = page.parentPage;
    while (parent) {
      level++;
      parent = parent.parentPage;
    }
    return level;
  }

  /**
   * Checks whether one page is an ancestor of the other page in terms of
   * subpage nesting.
   * @param potentialAncestor Potential ancestor.
   * @param potentialDescendent Potential descendent.
   * @return True if |potentialDescendent| is nested under |potentialAncestor|.
   */
  isAncestorOfPage(potentialAncestor: Page, potentialDescendent: Page):
      boolean {
    let parent = potentialDescendent.parentPage;
    while (parent) {
      if (parent === potentialAncestor) {
        return true;
      }
      parent = parent.parentPage;
    }
    return false;
  }

  /**
   * Called when a page's hash changes. If the page is the topmost visible
   * page, the history state is updated.
   */
  private onPageHashChanged_(e: CustomEvent) {
    const page = e.target as Page;
    if (page === this.getTopmostVisiblePage()) {
      this.updateHistoryState_(false);
    }
  }

  /**
   * @param observer The observer to register.
   */
  addObserver(observer: PageManagerObserver) {
    this.observers_.push(observer);
  }

  /**
   * Returns the topmost visible page.
   */
  private getTopmostVisiblePage(): Page|null {
    for (const page of this.registeredPages.values()) {
      if (page.visible) {
        return page;
      }
    }

    return null;
  }

  /**
   * Updates the title to the title of the current page, or of the topmost
   * visible page with a non-empty title.
   */
  private updateTitle_() {
    let page = this.getTopmostVisiblePage();
    while (page) {
      if (page.title) {
        for (const observer of this.observers_) {
          observer.updateTitle(page.title);
        }
        return;
      }
      page = page.parentPage;
    }
  }

  /**
   * Constructs a new path to push onto the history stack, using observers
   * to update the history.
   * @param replace If true, handlers should replace the current history event
   *     rather than create new ones.
   */
  private updateHistoryState_(replace: boolean) {
    if (this.isDialog) {
      return;
    }

    const page = this.getTopmostVisiblePage();
    assert(page);

    let path = window.location.pathname + window.location.hash;
    if (path) {
      // Remove trailing slash.
      path = path.slice(1).replace(/\/(?:#|$)/, '');
    }

    // If the page is already in history (the user may have clicked the same
    // link twice, or this is the initial load), do nothing.
    const newPath = (page === this.defaultPage_ ? '' : page.name) + page.hash;
    if (path === newPath) {
      return;
    }

    for (const observer of this.observers_) {
      observer.updateHistory(newPath, replace);
    }
  }

  static getInstance(): PageManager {
    return instance || (instance = new PageManager());
  }
}

let instance: PageManager|null = null;

/**
 * An observer of PageManager.
 */
export interface PageManagerObserver {
  /**
   * Called when a new title should be set.
   * @param title The title to set.
   */
  updateTitle(title: string): void;

  /**
   * Called when a page is navigated to.
   * @param path The path of the page being visited.
   * @param replace If true, allow no history events to be created.
   */
  updateHistory(path: string, replace: boolean): void;
}
