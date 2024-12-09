// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Browser Proxy for Certificate Viewer. Helps make testing
 * easier.
 */

import {sendWithPromise} from 'chrome://resources/js/cr.js';

export interface CertMetadataChangeResult {
  success: boolean;
  errorMessage?: string;
}

export interface CertViewerBrowserProxy {
  updateTrustState(newTrust: number): Promise<CertMetadataChangeResult>;
}

export class CertViewerBrowserProxyImpl implements CertViewerBrowserProxy {
  updateTrustState(newTrust: number): Promise<CertMetadataChangeResult> {
    return sendWithPromise('updateTrustState', newTrust);
  }

  static getInstance(): CertViewerBrowserProxy {
    return instance || (instance = new CertViewerBrowserProxyImpl());
  }

  static setInstance(obj: CertViewerBrowserProxyImpl) {
    instance = obj;
  }
}

let instance: CertViewerBrowserProxyImpl|null = null;
