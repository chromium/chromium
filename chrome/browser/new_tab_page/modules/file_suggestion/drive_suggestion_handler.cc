// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/file_suggestion/drive_suggestion_handler.h"

#include "chrome/browser/new_tab_page/modules/file_suggestion/drive_service.h"
#include "chrome/browser/new_tab_page/modules/file_suggestion/drive_service_factory.h"

DriveSuggestionHandler::DriveSuggestionHandler(
    mojo::PendingReceiver<file_suggestion::mojom::DriveSuggestionHandler>
        handler,
    Profile* profile)
    : handler_(this, std::move(handler)), profile_(profile) {}

DriveSuggestionHandler::~DriveSuggestionHandler() = default;

void DriveSuggestionHandler::GetFiles(GetFilesCallback callback) {
  DriveServiceFactory::GetForProfile(profile_)->GetDriveFiles(
      std::move(callback));
}

void DriveSuggestionHandler::DismissModule() {
  DriveServiceFactory::GetForProfile(profile_)->DismissModule();
}

void DriveSuggestionHandler::RestoreModule() {
  DriveServiceFactory::GetForProfile(profile_)->RestoreModule();
}
