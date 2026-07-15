// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PageContentNode} from './ai_overlay_dialog.mojom-webui.js';
import {log, warnLog} from './logging.js';

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
   * Structured document tree of the current page content from Mojo IPC.
   * Can be null when loading a new page until content becomes available.
   */
  content: PageContentNode|null;

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

  updateCurrentPageContext(title: string, rootNode: PageContentNode|string|null = null) {
    log(FILE, 'updateCurrentPageContext', title);
    if (!this.context) {
      warnLog(FILE, 'updateCurrentPageContext called without context');
      return;
    }

    const oldContext = {...this.context};
    this.context.title = title;
    const node = typeof rootNode === 'string' ? null : (rootNode ?? null);
    this.context.content = node;
    this.context.hasHadContent ||= Boolean(this.context.content);

    for (const listener of this.listeners) {
      const event: PageContextChangeEvent = {
        type: PageContextChangeType.UPDATE_CURRENT,
        newContext: {...this.context},
        oldContext,
      };
      listener(event);
    }
  }

  createNewPageContext(url: string, title: string|null, content: PageContentNode|string|null = null) {
    log(FILE, 'CreateNewPageContext', title, url);

    const oldContext = this.context ? {...this.context} : null;
    const node = typeof content === 'string' ? null : (content ?? null);
    this.context = {url, title, content: node, hasHadContent: Boolean(node)};

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
