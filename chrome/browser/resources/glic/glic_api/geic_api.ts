// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CloseSignInTabOptions, CloseSignInTabResult, OpenSignInTabOptions, OpenSignInTabResult} from './glic_api_generated.js';

/**
 * Provides Gemini Enterprise in Chrome functionality to the web client.
 */
export interface GeicBrowserHost {
  /**
   * Opens the authentication endpoint in a new top-level browser tab
   * and captures the originating tab for focus restoration.
   *
   * @param options Optional parameters for opening the sign-in tab.
   * @returns The result of attempting to open the sign-in tab.
   */
  openSignInTab(options?: OpenSignInTabOptions):
      Promise<OpenSignInTabResult>;

  /**
   * Closes the active sign-in tab (if open) and restores focus to the
   * user's originating tab.
   *
   * @param options Optional parameters for closing the sign-in tab.
   * @returns The result of attempting to close the sign-in tab.
   */
  closeSignInTab(options?: CloseSignInTabOptions):
      Promise<CloseSignInTabResult>;
}
