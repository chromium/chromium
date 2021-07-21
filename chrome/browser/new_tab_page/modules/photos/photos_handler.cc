// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/photos/photos_handler.h"

#include "chrome/browser/new_tab_page/modules/photos/photos_service.h"
#include "chrome/browser/new_tab_page/modules/photos/photos_service_factory.h"

PhotosHandler::PhotosHandler(
    mojo::PendingReceiver<photos::mojom::PhotosHandler> handler,
    Profile* profile)
    : handler_(this, std::move(handler)), profile_(profile) {}

PhotosHandler::~PhotosHandler() = default;

void PhotosHandler::GetMemories(GetMemoriesCallback callback) {
  PhotosServiceFactory::GetForProfile(profile_)->GetMemories(
      std::move(callback));
}
