// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {waitForLazyRender} from '//resources/cr_components/searchbox/utils.js';
import {getTrustedScriptURL} from '//resources/js/static_types.js';

let instance: WindowProxy|null = null;
let lazyLoadPromise: Promise<void>|null = null;

/** Abstracts some builtin JS functions to mock them in tests. */
export class WindowProxy {
  static getInstance(): WindowProxy {
    return instance || (instance = new WindowProxy());
  }

  static setInstance(newInstance: WindowProxy) {
    instance = newInstance;
  }

  /** Returns promise that resolves when lazy rendering should be started. */
  waitForLazyRender(): Promise<void> {
    return waitForLazyRender();
  }

  /**
   * Adds a <script> tag that holds the lazy loaded code, if not already
   * added, and returns a promise that resolves when it has loaded.
   */
  ensureLazyLoaded(): Promise<void> {
    if (!lazyLoadPromise) {
      const script = document.createElement('script');
      script.type = 'module';
      script.src = getTrustedScriptURL`./lazy_load.js`;
      document.body.appendChild(script);
      lazyLoadPromise = new Promise<void>(resolve => {
        script.onload = () => resolve();
      });
    }
    return lazyLoadPromise;
  }
}
