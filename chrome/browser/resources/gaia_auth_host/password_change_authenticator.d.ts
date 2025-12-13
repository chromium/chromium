// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Minimal TypeScript definitions to satisfy cases where
 * password_change_authenticator.js is used from TypeScript files.
 */

export interface PasswordChangeEventData {
  old_passwords: string[];
  new_passwords: string[];
}

export interface LoadParams {
  userName: string;
}

export class PasswordChangeAuthenticator extends EventTarget {
  constructor(webview: HTMLElement);

  load(data: LoadParams): void;
}
