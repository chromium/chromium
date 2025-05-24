// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Browser Proxy for Certificate Viewer. Helps make testing
 * easier.
 */

import {sendWithPromise} from 'chrome://resources/js/cr.js';

export interface CertMetadataChangeResult {
  // TODO(crbug.com/40928765): consider adding enum of possible errors instead
  // of having C++ return an error message.
  success: boolean;
  errorMessage?: string;
}

export interface ConstraintChangeResult {
  status: CertMetadataChangeResult;
  constraints?: string[];
}

export interface CertViewerBrowserProxy {
  updateTrustState(newTrust: number): Promise<CertMetadataChangeResult>;

  addConstraint(constraint: string): Promise<ConstraintChangeResult>;

  deleteConstraint(constraint: string): Promise<ConstraintChangeResult>;
}

export class CertViewerBrowserProxyImpl implements CertViewerBrowserProxy {
  updateTrustState(newTrust: number): Promise<CertMetadataChangeResult> {
    return sendWithPromise('updateTrustState', newTrust);
  }

  addConstraint(constraint: string): Promise<ConstraintChangeResult> {
    return sendWithPromise('addConstraint', constraint);
  }

  deleteConstraint(constraint: string): Promise<ConstraintChangeResult> {
    return sendWithPromise('deleteConstraint', constraint);
  }

  static getInstance(): CertViewerBrowserProxy {
    return instance || (instance = new CertViewerBrowserProxyImpl());
  }

  static setInstance(obj: CertViewerBrowserProxyImpl) {
    instance = obj;
  }
}

let instance: CertViewerBrowserProxyImpl|null = null;
