// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ensureLazyLoaded() will dynamically add the lazy_load.js
 * script to the DOM, which will subsequently load all the Advanced section
 * pages and all subpages.
 */

import {getTrustedScriptURL} from 'chrome://resources/js/static_types.js';

const ADVANCED_SECTION_PAGES = [
  'settings-crostini-page',
  'settings-date-time-page',
  'os-settings-files-page',
  'os-settings-languages-section',
  'os-settings-printing-page',
  'os-settings-reset-page',
];

let lazyLoadPromise: Promise<CustomElementConstructor[]>|null = null;

/**
 * @return Resolves when the lazy load module is imported and all Advanced
 * section pages have been defined. The promise is not blocked by all subpages
 * loading, however.
 */
export function ensureLazyLoaded(): Promise<CustomElementConstructor[]> {
  if (!lazyLoadPromise) {
    const script = document.createElement('script');
    script.type = 'module';
    script.src = getTrustedScriptURL`./lazy_load.js`;
    document.body.appendChild(script);

    lazyLoadPromise = Promise.all(
        ADVANCED_SECTION_PAGES.map((name) => customElements.whenDefined(name)));
  }
  return lazyLoadPromise;
}
