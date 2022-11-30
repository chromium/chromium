// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_CONFIRMATION_REASON_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_CONFIRMATION_REASON_H_

// Reason why DownloadTargetDeterminer requested additional confirmation for the
// target path via RequestConfirmation delegate method.
enum class DownloadConfirmationReason {
  NONE,

  // Unexpected error.
  UNEXPECTED,

  // "Save as" or "Save link as".
  SAVE_AS,

  // The user has set a preference requiring prompts for all downloads.
  PREFERENCE,

  // The target name was too long and couldn't be truncated.
  NAME_TOO_LONG,

  // There were unresolved conflicts with the target path.
  TARGET_CONFLICT,

  // The target path isn't writeable. Also may indicate that a previous attempt
  // to write to the path failed.
  TARGET_PATH_NOT_WRITEABLE,

  // The target path cannot accommodate a file of this size.
  TARGET_NO_SPACE,

  // The target is blocked by DLP so a dialog should be shown.
  DLP_BLOCKED,
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_CONFIRMATION_REASON_H_
