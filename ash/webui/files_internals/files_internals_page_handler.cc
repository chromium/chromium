// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/files_internals/files_internals_page_handler.h"

#include "ash/webui/files_internals/files_internals_ui.h"

namespace ash {

FilesInternalsPageHandler::FilesInternalsPageHandler(
    FilesInternalsUI* files_internals_ui,
    mojo::PendingReceiver<mojom::files_internals::PageHandler> receiver)
    : files_internals_ui_(files_internals_ui),
      receiver_(this, std::move(receiver)) {}

FilesInternalsPageHandler::~FilesInternalsPageHandler() = default;

void FilesInternalsPageHandler::GetSmbfsEnableVerboseLogging(
    GetSmbfsEnableVerboseLoggingCallback callback) {
  std::move(callback).Run(
      files_internals_ui_->delegate()->GetSmbfsEnableVerboseLogging());
}

void FilesInternalsPageHandler::SetSmbfsEnableVerboseLogging(bool enabled) {
  files_internals_ui_->delegate()->SetSmbfsEnableVerboseLogging(enabled);
}

void FilesInternalsPageHandler::GetOfficeFileHandlers(
    GetOfficeFileHandlersCallback callback) {
  std::move(callback).Run(
      files_internals_ui_->delegate()->GetOfficeFileHandlers());
}

void FilesInternalsPageHandler::ClearOfficeFileHandlers() {
  files_internals_ui_->delegate()->ClearOfficeFileHandlers();
}

void FilesInternalsPageHandler::GetMoveConfirmationShownForDrive(
    GetMoveConfirmationShownForDriveCallback callback) {
  std::move(callback).Run(
      files_internals_ui_->delegate()->GetMoveConfirmationShownForDrive());
}

void FilesInternalsPageHandler::GetMoveConfirmationShownForOneDrive(
    GetMoveConfirmationShownForOneDriveCallback callback) {
  std::move(callback).Run(
      files_internals_ui_->delegate()->GetMoveConfirmationShownForOneDrive());
}

void FilesInternalsPageHandler::GetMoveConfirmationShownForLocalToDrive(
    GetMoveConfirmationShownForLocalToDriveCallback callback) {
  std::move(callback).Run(files_internals_ui_->delegate()
                              ->GetMoveConfirmationShownForLocalToDrive());
}

void FilesInternalsPageHandler::GetMoveConfirmationShownForLocalToOneDrive(
    GetMoveConfirmationShownForLocalToOneDriveCallback callback) {
  std::move(callback).Run(files_internals_ui_->delegate()
                              ->GetMoveConfirmationShownForLocalToOneDrive());
}

void FilesInternalsPageHandler::GetMoveConfirmationShownForCloudToDrive(
    GetMoveConfirmationShownForCloudToDriveCallback callback) {
  std::move(callback).Run(files_internals_ui_->delegate()
                              ->GetMoveConfirmationShownForCloudToDrive());
}

void FilesInternalsPageHandler::GetMoveConfirmationShownForCloudToOneDrive(
    GetMoveConfirmationShownForCloudToOneDriveCallback callback) {
  std::move(callback).Run(files_internals_ui_->delegate()
                              ->GetMoveConfirmationShownForCloudToOneDrive());
}

void FilesInternalsPageHandler::GetAlwaysMoveOfficeFilesToDrive(
    GetAlwaysMoveOfficeFilesToDriveCallback callback) {
  std::move(callback).Run(
      files_internals_ui_->delegate()->GetAlwaysMoveOfficeFilesToDrive());
}

void FilesInternalsPageHandler::SetAlwaysMoveOfficeFilesToDrive(
    bool always_move) {
  files_internals_ui_->delegate()->SetAlwaysMoveOfficeFilesToDrive(always_move);
}

void FilesInternalsPageHandler::GetAlwaysMoveOfficeFilesToOneDrive(
    GetAlwaysMoveOfficeFilesToOneDriveCallback callback) {
  std::move(callback).Run(
      files_internals_ui_->delegate()->GetAlwaysMoveOfficeFilesToOneDrive());
}

void FilesInternalsPageHandler::SetAlwaysMoveOfficeFilesToOneDrive(
    bool always_move) {
  files_internals_ui_->delegate()->SetAlwaysMoveOfficeFilesToOneDrive(
      always_move);
}

}  // namespace ash
