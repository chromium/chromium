// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {debugLog, DebugLogTag, log, warnLog} from './logging.js';

const FILE = 'PageContextManager';

/**
 * Object used to track the current page's context.
 */
export interface PageContext {
  /**
   * Page URL, always available and cannot change within a given PageContext.
   */
  readonly url: string;

  /**
   * Page title as seen in the tab title. Can be null as the page is loading
   * until the title is parsed.
   */
  title: string|null;

  /**
   * Markdown representation of the current page content, potentially truncated
   * in PageContextMonitor on the C++ side. Can be null when loading a new page
   * until content becomes available.
   */
  content: string|null;

  /**
   * Tracks whether the PageContext has ever been contentful.
   */
  hasHadContent: boolean;
}

export enum PageContextChangeType {
  UPDATE_CURRENT = 'update_current',
  NEW_PAGE = 'new_page',
}

export interface PageContextChangeEvent {
  type: PageContextChangeType;
  newContext: PageContext;
  oldContext: PageContext|null;
}

export type PageContextListener = (event: PageContextChangeEvent) => void;

// Some pages fire load before real content is actually rendered. Try to avoid
// considering such pages as having content.
function isNonEmpty(str: string|null): boolean {
  const kMinimumContentThreshold = 100;

  if (!str) {
    return false;
  }
  // Consider only alphanumeric characters to look for real content.
  return str.replace(/[^a-zA-Z0-9]/g, '').length > kMinimumContentThreshold;
}

/**
 * PageContextManager maintains state about the current page context as
 * provided by the browser.
 */
export class PageContextManager {
  /**
   * Context for the current page. The object is replaced with a new one
   * whenever the current page is navigated to a new document. Content updates
   * to the current document are done in-place.
   */
  private context: PageContext|null = null;
  private listeners: PageContextListener[] = [];

  constructor() {}

  get pageContext(): PageContext|null {
    return this.context;
  }

  registerListener(listener: PageContextListener) {
    this.listeners.push(listener);
  }

  updateCurrentPageContext(title: string, content: string) {
    log(FILE, 'updateCurrentPageContext', title);
    debugLog(
        FILE, DebugLogTag.PAGE_CONTENT, 'PageContextManager: Update', title,
        content);
    if (!this.context) {
      warnLog(FILE, 'updateCurrentPageContext called without context');
      return;
    }

    const oldContext = {...this.context};
    this.context.title = title;
    this.context.content = content;
    this.context.hasHadContent ||= isNonEmpty(content);

    for (const listener of this.listeners) {
      const event: PageContextChangeEvent = {
        type: PageContextChangeType.UPDATE_CURRENT,
        newContext: {...this.context},
        oldContext,
      };
      listener(event);
    }
  }

  createNewPageContext(url: string, title: string|null, content: string|null) {
    log(FILE, 'CreateNewPageContext', title, url);
    debugLog(FILE, DebugLogTag.PAGE_CONTENT, 'Content', content);

    const oldContext = this.context ? {...this.context} : null;
    this.context = {url, title, content, hasHadContent: isNonEmpty(content)};

    for (const listener of this.listeners) {
      const event: PageContextChangeEvent = {
        type: PageContextChangeType.NEW_PAGE,
        newContext: {...this.context},
        oldContext,
      };
      listener(event);
    }
  }
}
