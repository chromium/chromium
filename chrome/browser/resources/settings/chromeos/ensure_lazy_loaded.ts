// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getTrustedScriptURL} from 'chrome://resources/js/static_types.js';

// List of pages (not subpages) that exist in the Advanced section.
const LAZY_LOAD_PAGES = [
  'settings-crostini-page',
  'settings-date-time-page',
  'os-settings-files-page',
  'os-settings-languages-section',
  'os-settings-printing-page',
  'os-settings-reset-page',
];

let lazyLoadPromise: Promise<CustomElementConstructor[]>|null = null;

/** @return Resolves when the lazy load module is imported. */
export function ensureLazyLoaded(): Promise<CustomElementConstructor[]> {
  if (!lazyLoadPromise) {
    const script = document.createElement('script');
    script.type = 'module';
    script.src = getTrustedScriptURL`./chromeos/lazy_load.js`;
    document.body.appendChild(script);

    lazyLoadPromise = Promise.all(
        LAZY_LOAD_PAGES.map((name) => customElements.whenDefined(name)));
  }
  return lazyLoadPromise;
}
