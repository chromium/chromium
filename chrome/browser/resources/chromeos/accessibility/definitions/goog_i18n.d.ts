// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions for goog.i18n API.
 */
declare namespace goog {
  export namespace i18n {
    export class MessageFormat {
      constructor(message: string);
      format(args: Object): string;
    }
  }
}
