// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/file_suggestion/file_suggestion_handler.h"

#include "chrome/browser/new_tab_page/modules/file_suggestion/drive_service.h"
#include "chrome/browser/new_tab_page/modules/file_suggestion/drive_service_factory.h"

FileSuggestionHandler::FileSuggestionHandler(
    mojo::PendingReceiver<file_suggestion::mojom::FileSuggestionHandler>
        handler,
    Profile* profile)
    : handler_(this, std::move(handler)), profile_(profile) {}

FileSuggestionHandler::~FileSuggestionHandler() = default;

void FileSuggestionHandler::GetFiles(GetFilesCallback callback) {
  DriveServiceFactory::GetForProfile(profile_)->GetDriveFiles(
      std::move(callback));
}

void FileSuggestionHandler::DismissModule() {
  DriveServiceFactory::GetForProfile(profile_)->DismissModule();
}

void FileSuggestionHandler::RestoreModule() {
  DriveServiceFactory::GetForProfile(profile_)->RestoreModule();
}
