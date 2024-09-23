// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The reason a page was closed. Keep in sync with NearbyShareDialogUI.
 */
export enum CloseReason {
  UNKNOWN = 0,
  TRANSFER_STARTED = 1,
  TRANSFER_SUCCEEDED = 2,
  CANCELLED = 3,
  REJECTED = 4,
}
