// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {debugLog, DebugLogTag, log, warnLog} from './logging.js';

const FILE = 'PageContextManager';

export interface PageContext {
  url: string;
  title: string|null;
  content: string|null;
}

/**
 * PageContextManager maintains state about the current page context as
 * provided by the browser.
 */
export class PageContextManager {
  private readonly onDidUpdatePageContent?: (() => void);
  private context: PageContext|null = null;
  private isStale: boolean = true;

  constructor(onDidUpdatePageContent?: () => void) {
    this.onDidUpdatePageContent = onDidUpdatePageContent;
  }

  get pageContext(): PageContext|null {
    return this.context;
  }

  get stale(): boolean {
    return this.isStale;
  }

  updateCurrentPageContext(title: string, content: string) {
    log(FILE, 'updateCurrentPageContext', title);
    debugLog(
        FILE, DebugLogTag.PAGE_CONTENT, 'PageContextManager: Update', title,
        content);
    if (this.context) {
      this.context.title = title;
      this.context.content = content;
    } else {
      warnLog(FILE, 'updateCurrentPageContext called without context');
    }
    this.isStale = false;

    if (this.onDidUpdatePageContent) {
      this.onDidUpdatePageContent();
    }
  }

  didChangePage(url: string, title: string|null, content: string|null) {
    log(FILE, 'DidChangePage', title, url);
    debugLog(FILE, DebugLogTag.PAGE_CONTENT, 'Content', content);

    this.context = {url, title, content};
    this.isStale = content === null;
  }
}
