// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Unseasoned PDF native API.
 *
 * This can't use the externs tag because Closure Compiler will complain about
 * ES6 modules in externs. Note that optimize_webui() doesn't use Closure
 * Compiler, so this has no impact on name mangling anyway.
 */

/**
 * A more specific interface for the unseasoned PDF plugin.
 */
export class UnseasonedPdfPluginElement extends HTMLEmbedElement {
  /**
   * @param {*} message
   * @param {!Array<!Transferable>=} transfer
   */
  postMessage(message, transfer) {}
}
