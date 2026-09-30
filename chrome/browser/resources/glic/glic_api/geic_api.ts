// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {                       //
             CloseAuthTabOptions,   //
             CloseAuthTabResponse,  //
             CloseSignInTabOptions, //
             CloseSignInTabResult,  //
             OpenAuthTabOptions,    //
             OpenAuthTabResponse,   //
             OpenSignInTabOptions,  //
             OpenSignInTabResult,   //
} from './glic_api_generated.js';

/**
 * Provides Gemini Enterprise in Chrome functionality to the web client.
 */
export interface GeicBrowserHost {
  /**
   * Opens the authentication endpoint in a new top-level browser tab
   * and captures the originating tab for focus restoration.
   *
   * @deprecated Use `openAuthTab` with `AuthTabPurpose.SIGN_IN`.
   * @param options Optional parameters for opening the sign-in tab.
   * @returns The result of attempting to open the sign-in tab.
   */
  openSignInTab(options?: OpenSignInTabOptions): Promise<OpenSignInTabResult>;

  /**
   * Closes the active sign-in tab (if open) and restores focus to the
   * user's originating tab.
   *
   * @deprecated Use `closeAuthTab` with `AuthTabPurpose.SIGN_IN`.
   * @param options Optional parameters for closing the sign-in tab.
   * @returns The result of attempting to close the sign-in tab.
   */
  closeSignInTab(options?: CloseSignInTabOptions):
      Promise<CloseSignInTabResult>;

  /**
   * Opens `options.url` in a new top-level browser tab for `options.purpose`
   * and captures the originating tab for focus restoration. Chrome tracks at
   * most one tab per purpose; if it is still open and still on an origin
   * expected for the purpose, it is reused (for `CONNECTOR_OAUTH`, it is also
   * navigated to the new URL). Otherwise a new tab is opened.
   *
   * The URL is validated by Chrome against the purpose's allowlist:
   * - `SIGN_IN`: HTTPS on the GAIA or GEiC guest origin.
   * - `CONNECTOR_OAUTH`: HTTPS with path `/oauth-redirect` on a GE OAuth
   *   redirector origin (vertexaisearch.cloud.google.com) or the GEiC guest
   *   origin. The 3P provider URL in
   *   `continue_uri` is not validated.
   *
   * Not available in older Chrome versions.
   *
   * @param options The purpose and URL of the tab to open.
   * @returns The response, whose `result` is the outcome of the open attempt.
   */
  openAuthTab?(options: OpenAuthTabOptions): Promise<OpenAuthTabResponse>;

  /**
   * Closes the tab previously opened by `openAuthTab` for `options.purpose`,
   * if it is still open and still on an origin expected for the purpose. If it
   * was the active tab, focus is restored to the user's originating tab. Tabs
   * not opened by `openAuthTab` for the same purpose, or that the user has
   * navigated elsewhere (`NAVIGATED_AWAY`), are never closed.
   *
   * Not available in older Chrome versions.
   *
   * @param options The purpose of the tab to close.
   * @returns The response, whose `result` is the outcome of the close attempt.
   */
  closeAuthTab?(options: CloseAuthTabOptions): Promise<CloseAuthTabResponse>;
}
