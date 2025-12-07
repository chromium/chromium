// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {MicrosoftAuthUntrustedDocumentRemote} from '../ntp_microsoft_auth_shared_ui.mojom-webui.js';

/**
 * Implements ParentTrustedPage interface to handle requests from the child
 * page. This is only connected to the Microsoft Auth iframe.
 */

let instance: ParentTrustedDocumentProxy|null = null;

export class ParentTrustedDocumentProxy {
  static getInstance(): ParentTrustedDocumentProxy|null {
    return instance;
  }

  static setInstance(childDocument: MicrosoftAuthUntrustedDocumentRemote) {
    instance = new ParentTrustedDocumentProxy(childDocument);
  }

  private childDocument_: MicrosoftAuthUntrustedDocumentRemote;

  private constructor(childDocument: MicrosoftAuthUntrustedDocumentRemote) {
    this.childDocument_ = childDocument;
  }

  getChildDocument(): MicrosoftAuthUntrustedDocumentRemote {
    return this.childDocument_;
  }
}
