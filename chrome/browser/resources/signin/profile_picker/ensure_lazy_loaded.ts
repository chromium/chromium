// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getTrustedScriptURL} from 'chrome://resources/js/static_types.js';

let lazyLoadPromise: Promise<void>|null = null;

/**
 * @return Resolves when the lazy load module is imported.
 */
export function ensureLazyLoaded(): Promise<void> {
  if (!lazyLoadPromise) {
    const script = document.createElement('script');
    script.type = 'module';
    script.src = getTrustedScriptURL`./lazy_load.js`;
    document.body.appendChild(script);

    lazyLoadPromise = Promise
                          .all([
                            'profile-type-choice', 'profile-switch',
                            // <if expr="chromeos_lacros">
                            'account-selection-lacros',
                            // </if>
                          ].map(name => customElements.whenDefined(name)))
                          .then(() => {});
  }
  return lazyLoadPromise;
}
