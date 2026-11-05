// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getTrustedScriptURL} from '//resources/js/static_types.js';

let lazyLoadPromise: Promise<void>|null = null;

/** @return Resolves when the lazy load module is imported. */
export function ensureLazyLoaded(): Promise<void> {
  if (lazyLoadPromise === null) {
    const script = document.createElement('script');
    script.type = 'module';
    script.src = getTrustedScriptURL`./lazy_load.js`;
    document.body.appendChild(script);

    lazyLoadPromise = Promise
                          .all([
                            'app-menu-button',
                            'avatar-button',
                            'battery-saver-button',
                            'webui-toolbar-extensions',
                            'glic-button',
                            'location-bar',
                            'media-button',
                            'overflow-button',
                            'performance-intervention-button',
                            'pinned-toolbar-actions',
                          ].map(name => customElements.whenDefined(name)))
                          .then(() => {});
  }
  return lazyLoadPromise;
}
