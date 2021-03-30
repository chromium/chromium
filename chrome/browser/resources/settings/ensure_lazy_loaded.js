// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let lazyLoadPromise = null;

/** @return {!Promise<void>} Resolves when the lazy load module is imported. */
export function ensureLazyLoaded() {
  if (!lazyLoadPromise) {
    const script = document.createElement('script');
    script.type = 'module';
    script.src = './lazy_load.js';
    document.body.appendChild(script);

    lazyLoadPromise = Promise.all([
      'settings-appearance-page', 'settings-autofill-section',
      'settings-password-check', 'passwords-section',
      'settings-payments-section', 'settings-clear-browsing-data-dialog',
      'settings-search-engines-page',
      // <if expr="use_nss_certs">
      'certificate-manager',
      // </if>
      'settings-a11y-page', 'settings-downloads-page',
      'settings-languages-page', 'settings-reset-page',
      // <if expr="not chromeos and not lacros">
      'settings-system-page',
      // </if>
      // <if expr="not is_macosx">
      'settings-edit-dictionary-page',
      // </if>
    ].map(name => customElements.whenDefined(name)));
  }
  return lazyLoadPromise;
}
