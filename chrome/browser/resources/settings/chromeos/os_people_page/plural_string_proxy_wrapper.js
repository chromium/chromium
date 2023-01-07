// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper function used to get a pluralized string using the
 * PluralStringProxy.
 * TODO (https://crbug.com/1315757): Use PluralStringProxyImpl directly in
 * lock_screen.js, once it is no longer being typechecked by the closure
 * compiler.
 */

import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';

/**
 * @param {string} message
 * @param {number} count
 * @return {!Promise<string>}
 */
export function getPluralStringFromProxy(message, count) {
  return PluralStringProxyImpl.getInstance().getPluralString(message, count);
}
