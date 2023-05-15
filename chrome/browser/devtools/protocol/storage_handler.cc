// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/storage_handler.h"

#include "chrome/browser/dips/dips_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

StorageHandler::StorageHandler(content::WebContents* web_contents,
                               protocol::UberDispatcher* dispatcher)
    : web_contents_(web_contents->GetWeakPtr()) {
  protocol::Storage::Dispatcher::wire(dispatcher, this);
}

StorageHandler::~StorageHandler() = default;

void StorageHandler::RunBounceTrackingMitigations(
    std::unique_ptr<RunBounceTrackingMitigationsCallback> callback) {
  DIPSService* dips_service =
      web_contents_ ? DIPSService::Get(web_contents_->GetBrowserContext())
                    : nullptr;

  if (!dips_service) {
    callback->sendFailure(protocol::Response::ServerError("No DIPSService"));
    return;
  }

  dips_service->DeleteEligibleSitesImmediately(
      base::BindOnce(&StorageHandler::GotDeletedSites, std::move(callback)));
}

/* static */
void StorageHandler::GotDeletedSites(
    std::unique_ptr<RunBounceTrackingMitigationsCallback> callback,
    const std::vector<std::string>& sites) {
  auto deleted_sites =
      std::make_unique<protocol::Array<protocol::String>>(sites);
  callback->sendSuccess(std::move(deleted_sites));
}
