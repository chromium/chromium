// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface PageContext {
  url: string;
  title: string;
  content: string;
}

/**
 * PageContextManager maintains state about the current page context as
 * provided by the browser.
 */
export class PageContextManager {
  private context: PageContext|null = null;
  private isStale: boolean = true;

  get pageContext(): PageContext|null {
    return this.context;
  }

  get stale(): boolean {
    return this.isStale;
  }

  updateCurrentPageContext(url: string, title: string, content: string) {
    this.context = {url, title, content};
    this.isStale = false;
  }

  invalidatePageContext() {
    this.isStale = true;
  }
}
