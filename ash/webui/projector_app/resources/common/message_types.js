// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Message definitions passed over the Projector app privileged/unprivileged
 * pipe.
 */

/**
 * Enum for tool types supported in the Ink engine.
 *
 * See http://go/ink-tools for details on each tool.
 *
 * @enum {string}
 */
export const AnnotatorToolType = {
  MARKER: 'marker',
  PEN: 'pen',
  HIGHLIGHTER: 'highlighter',
  ERASER: 'eraser',
};

/**
 * Enum for projector error types supported.
 * @enum {string}
 */
export const ProjectorError = {
  NONE: 'NONE',
  TOKEN_FETCH_FAILURE: 'TOKEN_FETCH_FAILURE',
  TOKEN_FETCH_ALREADY_IN_PROGRESS: 'TOKEN_FETCH_ALREADY_IN_PROGRESS',
  OTHER: 'OTHER',
};


/**
 * The new screencast button state. Corresponds to
 * NEW_SCREENCAST_PRECONDITION_STATE in Google3 deployed resources.
 * @enum {number}
 */
export const NewScreencastPreconditionState = {
  DISABLED: 1,
  ENABLED: 2,
  HIDDEN: 3,
};

/**
 * The new screencast precondition reason. Corresponds to
 * NEW_SCREENCAST_PRECONDITION_REASON in Google3 deployed resources.
 * @enum {number}
 */
export const NewScreencastPreconditionReason = {
  ON_DEVICE_RECOGNITION_NOT_SUPPORTED: 1,
  USER_LOCALE_NOT_SUPPORTED: 2,
  IN_PROJECTOR_SESSION: 3,
  SCREEN_RECORDING_IN_PROGRESS: 4,
  SODA_DOWNLOAD_IN_PROGRESS: 5,
  OUT_OF_DISK_SPACE: 6,
  NO_MIC: 7,
  DRIVE_FS_UNMOUNTED: 8,
  DRIVE_FS_MOUNT_FAILED: 9,
  OTHERS: 10,

  // Soda installation errors:
  SODA_INSTALLATION_ERROR_UNSPECIFIED_ERROR: 0,
  SODA_INSTALLATION_ERROR_NEEDS_REBOOT: 11,

  AUDIO_CAPTURE_DISABLED_BY_POLICY: 12,

  // Enabled reason:
  ENABLED_BY_SODA: 13,
  ENABLED_BY_SERVER_SIDE_SPEECH_RECOGNITION: 14,
};
