// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getTrustedScriptURL} from 'chrome://resources/js/static_types.js';

let lazyLoadPromise: Promise<void>|null = null;

/** @return Resolves when the lazy load module is imported. */
export function ensureLazyLoaded(): Promise<void> {
  if (lazyLoadPromise === null) {
    const script = document.createElement('script');
    script.type = 'module';
    script.src = getTrustedScriptURL`./lazy_load.js`;
    document.body.appendChild(script);

    lazyLoadPromise =
        Promise
            .all([
              'settings-appearance-page', 'settings-autofill-section',
              'settings-payments-section',
              'settings-clear-browsing-data-dialog',
              'settings-search-engines-page',
              // <if expr="use_nss_certs">
              'certificate-manager',
              // </if>
              'settings-a11y-page', 'settings-downloads-page',
              // <if expr="not chromeos_ash">
              'settings-languages-page',
              // </if>
              'settings-reset-page',
              // <if expr="not chromeos_ash">
              'settings-system-page',
              // </if>
              // <if expr="not chromeos_ash and not is_macosx">
              'settings-edit-dictionary-page',
              // </if>
            ].map(name => customElements.whenDefined(name)))
            .then(() => {});
  }
  return lazyLoadPromise!;
}
