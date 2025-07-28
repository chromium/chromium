// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * This file contains enums for Glic FRE metrics recorded from the WebUI.
 * These values are persisted to logs and should not be renumbered or reused.
 * See tools/metrics/histograms/metadata/glic/enums.xml.
 */

// LINT.IfChange(GlicFreWebviewLoadAbortReason)
export enum GlicFreWebviewLoadAbortReason {
  UNKNOWN = 0,
  ERR_ABORTED = 1,
  ERR_INVALID_URL = 2,
  ERR_DISALLOWED_URL_SCHEME = 3,
  ERR_BLOCKED_BY_CLIENT = 4,
  ERR_ADDRESS_UNREACHABLE = 5,
  ERR_EMPTY_RESPONSE = 6,
  ERR_FILE_NOT_FOUND = 7,
  ERR_UNKNOWN_URL_SCHEME = 8,
  ERR_TIMED_OUT = 9,
  ERR_HTTP_RESPONSE_CODE_FAILURE = 10,
  // Add new values above this line.
  MAX_VALUE = ERR_HTTP_RESPONSE_CODE_FAILURE,
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicFreWebviewLoadAbortReason)
