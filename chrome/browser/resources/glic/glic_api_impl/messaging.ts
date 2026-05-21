// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Defines a request and optionally a corresponding response messages.
 */
export interface RequestDef {
  // The type of payload sent. Defaults to 'undefined', which means the request
  // has no request payload.
  request?: unknown;
  // The type of response payload. Defaults to 'void', which means the request
  // sends no response payload.
  response?: unknown;
  /**
   * Whether the request can be processed in the background.
   *
   * If true, the request is allowed to be sent and serviced in the
   * background.
   * If false (the default if omitted):
   * For Host requests, `BACKGROUND_RESPONSES` defines how these are handled.
   * For Client requests, it affects usage of `GatedSender`.
   */
  backgroundAllowed?: boolean;
}

// Validates each key is a RequestDef.
export type ValidateRequestMap<T extends Record<string, RequestDef>> = T;

/* eslint-disable-next-line @typescript-eslint/naming-convention */
export function assertNever<_T extends never>() {}
