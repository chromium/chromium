// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/drive/drive_handler.h"

#include "chrome/browser/search/drive/drive_service.h"
#include "chrome/browser/search/drive/drive_service_factory.h"

DriveHandler::DriveHandler(
    mojo::PendingReceiver<drive::mojom::DriveHandler> handler,
    Profile* profile)
    : handler_(this, std::move(handler)), profile_(profile) {}

DriveHandler::~DriveHandler() = default;

void DriveHandler::GetFiles(GetFilesCallback callback) {
  DriveServiceFactory::GetForProfile(profile_)->GetDriveFiles(
      std::move(callback));
}
