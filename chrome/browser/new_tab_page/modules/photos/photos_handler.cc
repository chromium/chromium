// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/photos/photos_handler.h"

#include "chrome/browser/new_tab_page/modules/photos/photos_service.h"
#include "chrome/browser/new_tab_page/modules/photos/photos_service_factory.h"
#include "content/public/browser/web_contents.h"

PhotosHandler::PhotosHandler(
    mojo::PendingReceiver<photos::mojom::PhotosHandler> handler,
    Profile* profile,
    content::WebContents* web_contents)
    : handler_(this, std::move(handler)),
      profile_(profile),
      web_contents_(web_contents) {}

PhotosHandler::~PhotosHandler() = default;

void PhotosHandler::GetMemories(GetMemoriesCallback callback) {
  PhotosServiceFactory::GetForProfile(profile_)->GetMemories(
      std::move(callback));
}

void PhotosHandler::DismissModule() {
  PhotosServiceFactory::GetForProfile(profile_)->DismissModule();
}

void PhotosHandler::RestoreModule() {
  PhotosServiceFactory::GetForProfile(profile_)->RestoreModule();
}

void PhotosHandler::ShouldShowOptInScreen(
    ShouldShowOptInScreenCallback callback) {
  std::move(callback).Run(
      PhotosServiceFactory::GetForProfile(profile_)->ShouldShowOptInScreen());
}

void PhotosHandler::OnUserOptIn(bool accept) {
  PhotosServiceFactory::GetForProfile(profile_)->OnUserOptIn(
      accept, web_contents_, profile_);
}

void PhotosHandler::OnMemoryOpen() {
  PhotosServiceFactory::GetForProfile(profile_)->OnMemoryOpen();
}

void PhotosHandler::ShouldShowSoftOptOutButton(
    ShouldShowSoftOptOutButtonCallback callback) {
  std::move(callback).Run(PhotosServiceFactory::GetForProfile(profile_)
                              ->ShouldShowSoftOptOutButton());
}

void PhotosHandler::SoftOptOut() {
  PhotosServiceFactory::GetForProfile(profile_)->SoftOptOut();
}

void PhotosHandler::GetOptInTitleText(
    std::vector<photos::mojom::MemoryPtr> memories,
    GetOptInTitleTextCallback callback) {
  std::move(callback).Run(
      PhotosServiceFactory::GetForProfile(profile_)->GetOptInTitleText(
          std::move(memories)));
}
