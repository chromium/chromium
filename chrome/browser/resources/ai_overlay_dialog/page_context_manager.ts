// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
    console.info(
        'PageContextManager: Update', title, content.substring(0, 200));
    if (this.context) {
      this.context.title = title;
      this.context.content = content;
    } else {
      console.warn('updateCurrentPageContext called without context');
    }
    this.isStale = false;

    if (this.onDidUpdatePageContent) {
      this.onDidUpdatePageContent();
    }
  }

  didChangePage(url: string, title: string|null, content: string|null) {
    console.info(
        'PageContextManager: didChangePage', url, title,
        content?.substring(0, 200));
    this.context = {url, title, content};
    this.isStale = content === null;
  }
}
