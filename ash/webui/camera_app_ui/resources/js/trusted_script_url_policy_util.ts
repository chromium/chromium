// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertExists} from './assert.js';
import {expandPath} from './util.js';


// Trusted script URLs used by the Camera app.
const ALLOWED_SCRIPT_URLS = new Set([
  '/js/test_bridge.js',
  '/js/main.js',
  '/js/models/barcode_worker.js',
  '/js/multi_window_manager.js',
].map(expandPath));

// Create a TrustedTypes script URL policy from a list of allowed sources.
const trustedScriptUrlPolicy =
    assertExists(window.trustedTypes)
        .createPolicy('camera-app-trusted-script', {
          // eslint-disable-next-line @typescript-eslint/naming-convention
          createScriptURL: (url: string) => {
            if (!ALLOWED_SCRIPT_URLS.has(url)) {
              throw new Error('Script URL not allowed: ' + url);
            }
            return url;
          },
        });

/**
 * Create a TrustedTypes script URL policy from a list of allowed sources, and
 * return a sanitized script URL using this policy.
 *
 * TODO(crbug.com/1466042): Change the return type to `TrustedScriptURL` once
 * Worker/SharedWorker ts type and setAttribute() accept `TrustedScriptURL`.
 *
 * @param url Script URL to be sanitized.
 */
export function getSanitizedScriptUrl(url: string): string {
  // eslint-disable-next-line @typescript-eslint/consistent-type-assertions
  return assertExists(trustedScriptUrlPolicy)
             .createScriptURL(expandPath(url)) as unknown as string;
}
