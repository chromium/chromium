// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {signal} from '../reactive/signal.js';

export const currentRoute = signal<URL|null>(null);

function updateRoute() {
  currentRoute.value = new URL(window.location.href);
}

/**
 * Navigates to the given "path".
 *
 * Note that this should only be used for pages under Recorder App, and not
 * external link. Since we do client side navigation via URL hash (see the
 * reason in pages/recorder-app.ts), the path is put into URL hash.
 */
export function navigateTo(path: string): void {
  window.history.pushState({}, '', `#${path}`);
  updateRoute();
}

/**
 * Installs handler that intercept click on <a> to do client side navigation.
 */
export function installRouter(): void {
  document.body.addEventListener('click', (e) => {
    if (e.defaultPrevented || e.button !== 0 || e.altKey || e.ctrlKey ||
        e.shiftKey || e.metaKey) {
      return;
    }

    const anchor = e.composedPath().find((el): el is HTMLAnchorElement => {
      return el instanceof Node && el.nodeName === 'A';
    });
    if (anchor === undefined || anchor.target !== '' ||
        anchor.download !== '' || anchor.rel === 'external') {
      return;
    }

    const href = anchor.href;
    let url: URL;
    try {
      url = new URL(href);
      if (url.origin !== window.location.origin) {
        return;
      }
    } catch (_e) {
      // Error when parsing the anchor href, ignore it.
      return;
    }

    e.preventDefault();
    if (href !== window.location.href) {
      navigateTo(url.pathname);
    }
  });

  window.addEventListener('popstate', () => {
    updateRoute();
  });
  updateRoute();
}
