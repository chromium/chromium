// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';

/**
 * @fileoverview This file provides shared utility functions used by the custom
 * elements in the Memories landing page.
 */

/**
 * Converts a Mojo String16 to a JS string.
 * @param {?String16} str
 * @return {string}
 */
export function decodeMojoString16(str) {
  return str ? str.data.map(ch => String.fromCodePoint(ch)).join('') : '';
}
