// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let lazyLoadPromise: Promise<CustomElementConstructor[]>|null = null;

/** @return Resolves when the lazy load module is imported. */
export function ensureLazyLoaded(): Promise<CustomElementConstructor[]> {
  if (!lazyLoadPromise) {
    const script = document.createElement('script');
    script.type = 'module';
    script.src = './chromeos/lazy_load.js';
    document.body.appendChild(script);
    const lazyLoadPages = [
      'settings-crostini-page',
      'settings-date-time-page',
      'os-settings-a11y-page',
      'os-settings-files-page',
      'os-settings-languages-section',
      'os-settings-printing-page',
      'os-settings-reset-page',
    ];

    lazyLoadPromise = Promise.all(
        lazyLoadPages.map((name) => customElements.whenDefined(name)));
  }
  return lazyLoadPromise;
}
