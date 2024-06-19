// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {signal} from '../reactive/signal.js';

export const currentRoute = signal<URL|null>(null);

function updateRoute() {
  currentRoute.value = new URL(window.location.href);
}

/**
 * Navigates to the given path.
 */
export function navigateTo(path: string): void {
  window.history.pushState({}, '', path);
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
    try {
      const url = new URL(href);
      if (url.origin !== window.location.origin) {
        return;
      }
    } catch (_e) {
      // Error when parsing the anchor href, ignore it.
      return;
    }

    e.preventDefault();
    if (href !== window.location.href) {
      navigateTo(href);
    }
  });

  window.addEventListener('popstate', () => {
    updateRoute();
  });
  updateRoute();
}
