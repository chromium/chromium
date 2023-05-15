// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_FILES_INTERNALS_FILES_INTERNALS_UI_DELEGATE_H_
#define ASH_WEBUI_FILES_INTERNALS_FILES_INTERNALS_UI_DELEGATE_H_

#include "base/values.h"

namespace ash {

// Delegate to expose //chrome services to //ash/webui FilesInternalsUI.
class FilesInternalsUIDelegate {
 public:
  virtual ~FilesInternalsUIDelegate() = default;

  virtual base::Value GetDebugJSON() const = 0;

  virtual bool GetSmbfsEnableVerboseLogging() const = 0;
  virtual void SetSmbfsEnableVerboseLogging(bool enabled) = 0;

  virtual std::string GetOfficeFileHandlers() const = 0;
  virtual void ClearOfficeFileHandlers() = 0;

  virtual bool GetMoveConfirmationShownForDrive() const = 0;
  virtual bool GetMoveConfirmationShownForOneDrive() const = 0;

  virtual bool GetMoveConfirmationShownForLocalToDrive() const = 0;
  virtual bool GetMoveConfirmationShownForLocalToOneDrive() const = 0;
  virtual bool GetMoveConfirmationShownForCloudToDrive() const = 0;
  virtual bool GetMoveConfirmationShownForCloudToOneDrive() const = 0;

  virtual bool GetAlwaysMoveOfficeFilesToDrive() const = 0;
  virtual void SetAlwaysMoveOfficeFilesToDrive(bool always_move) = 0;

  virtual bool GetAlwaysMoveOfficeFilesToOneDrive() const = 0;
  virtual void SetAlwaysMoveOfficeFilesToOneDrive(bool always_move) = 0;
};

}  // namespace ash

#endif  // ASH_WEBUI_FILES_INTERNALS_FILES_INTERNALS_UI_DELEGATE_H_
