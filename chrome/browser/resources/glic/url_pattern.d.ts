// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// https://developer.mozilla.org/en-US/docs/Web/API/URLPattern
/* eslint-disable @typescript-eslint/naming-convention */
declare interface URLPatternOptions {
  ignoreCase?: boolean;
}

/* eslint-disable @typescript-eslint/naming-convention */
declare interface URLPatternInit {
  protocol?: string;
  username?: string;
  password?: string;
  hostname?: string;
  port?: string;
  pathname?: string;
  search?: string;
  hash?: string;
  baseURL?: string;
}

/* eslint-disable @typescript-eslint/naming-convention */
declare class URLPattern {
  constructor(
      input: string|URLPatternInit, baseURL?: string,
      options?: URLPatternOptions);
  constructor(input: string|URLPatternInit, options?: URLPatternOptions);

  readonly hash: string;
  readonly hostname: string;
  readonly password: string;
  readonly pathname: string;
  readonly port: string;
  readonly protocol: string;
  readonly search: string;
  readonly username: string;

  test(url: string): boolean;
}
