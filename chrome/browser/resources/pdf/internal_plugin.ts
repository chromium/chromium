// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Unseasoned PDF native API.
 */

/** A more specific interface for the unseasoned PDF plugin. */
export interface UnseasonedPdfPluginElement extends HTMLEmbedElement {
  postMessage(message: any, transfer?: Transferable[]): void;
}
