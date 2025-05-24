// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface GaiaActionButtonsElement extends HTMLElement {
  setAuthenticatorForTest(authenticator: Object): void;
}

declare global {
  interface HTMLElementTagNameMap {
    'gaia-action-buttons': GaiaActionButtonsElement;
  }
}
