// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Small utilities to be used during auth factor setup in
 * chrome://os-settings and chrome://oobe, typically in conjunction with the
 * quickUnlockPrivate extension API or the authFactorConfig mojo service.
 */


/**
 * Type of the event signalling that an auth token used for quickUnlockPrivate
 * and authFactorConfig is invalid.
 */
export const AUTH_TOKEN_INVALID_EVENT_TYPE: string = 'auth-token-invalid';

/**
 * Create and dispatch an `AUTH_TOKEN_INVALID_EVENT_TYPE` event on the provided
 * element.
 */
export function fireAuthTokenInvalidEvent(el: Element): void {
  const ev = new CustomEvent(
      AUTH_TOKEN_INVALID_EVENT_TYPE, {bubbles: true, composed: true});
  el.dispatchEvent(ev);
}
