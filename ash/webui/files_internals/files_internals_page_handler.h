// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_FILES_INTERNALS_FILES_INTERNALS_PAGE_HANDLER_H_
#define ASH_WEBUI_FILES_INTERNALS_FILES_INTERNALS_PAGE_HANDLER_H_

#include "ash/webui/files_internals/mojom/files_internals.mojom.h"
#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash {

class FilesInternalsUI;

// Implements the mojom::files_internals::PageHandler interface.
class FilesInternalsPageHandler : public mojom::files_internals::PageHandler {
 public:
  FilesInternalsPageHandler(
      FilesInternalsUI* files_internals_ui,
      mojo::PendingReceiver<mojom::files_internals::PageHandler> receiver);
  FilesInternalsPageHandler(const FilesInternalsPageHandler&) = delete;
  FilesInternalsPageHandler& operator=(const FilesInternalsPageHandler&) =
      delete;
  ~FilesInternalsPageHandler() override;

  // mojom::files_internals::PageHandler overrides.
  void GetSmbfsEnableVerboseLogging(
      GetSmbfsEnableVerboseLoggingCallback callback) override;
  void SetSmbfsEnableVerboseLogging(bool enabled) override;
  void GetOfficeFileHandlers(GetOfficeFileHandlersCallback callback) override;
  void ClearOfficeFileHandlers() override;
  void GetMoveConfirmationShownForDrive(
      GetMoveConfirmationShownForDriveCallback callback) override;
  void GetMoveConfirmationShownForOneDrive(
      GetMoveConfirmationShownForOneDriveCallback callback) override;
  void GetMoveConfirmationShownForLocalToDrive(
      GetMoveConfirmationShownForLocalToDriveCallback callback) override;
  void GetMoveConfirmationShownForLocalToOneDrive(
      GetMoveConfirmationShownForLocalToOneDriveCallback callback) override;
  void GetMoveConfirmationShownForCloudToDrive(
      GetMoveConfirmationShownForCloudToDriveCallback callback) override;
  void GetMoveConfirmationShownForCloudToOneDrive(
      GetMoveConfirmationShownForCloudToOneDriveCallback callback) override;
  void GetAlwaysMoveOfficeFilesToDrive(
      GetAlwaysMoveOfficeFilesToDriveCallback callback) override;
  void SetAlwaysMoveOfficeFilesToDrive(bool always_move) override;
  void GetAlwaysMoveOfficeFilesToOneDrive(
      GetAlwaysMoveOfficeFilesToOneDriveCallback callback) override;
  void SetAlwaysMoveOfficeFilesToOneDrive(bool always_move) override;

 private:
  raw_ptr<FilesInternalsUI> files_internals_ui_;  // Owns |this|.
  mojo::Receiver<mojom::files_internals::PageHandler> receiver_;
};

}  // namespace ash

#endif  // ASH_WEBUI_FILES_INTERNALS_FILES_INTERNALS_PAGE_HANDLER_H_
