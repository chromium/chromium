// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview PDF plugin native API.
 */

/** A more specific interface for the PDF plugin. */
export interface PdfPluginElement extends HTMLEmbedElement {
  postMessage(message: any, transfer?: Transferable[]): void;
}
