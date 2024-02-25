// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_download_observer.h"

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/download/simple_download_manager_coordinator_factory.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "components/download/content/public/all_download_item_notifier.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_save_item_data.h"
#include "components/keyed_service/core/simple_factory_key.h"
#include "content/public/browser/browser_context.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace policy {
namespace {

GURL GeneralizeURL(const GURL& url, const download::DownloadItem& item) {
  // data: urls cannot be matched against dlp rules. We use the embedding tab
  // url as source.
  GURL generalized_url = url;
  if (url.SchemeIs(url::kDataScheme)) {
    generalized_url = item.GetTabUrl();
  }
  if (generalized_url.SchemeIs(url::kBlobScheme)) {
    return url::Origin::Create(generalized_url).GetURL();
  }
  return generalized_url;
}

}  // namespace

DlpDownloadObserver::DlpDownloadObserver(SimpleFactoryKey* key) : key_(key) {
  auto* registry =
      SimpleDownloadManagerCoordinatorFactory::GetInstance()->GetForKey(key);
  CHECK(registry);
  registry->AddObserver(this);
}

DlpDownloadObserver::~DlpDownloadObserver() = default;

void DlpDownloadObserver::Shutdown() {
  auto* registry =
      SimpleDownloadManagerCoordinatorFactory::GetInstance()->GetForKey(key_);
  if (registry) {
    registry->RemoveObserver(this);
  }
}

void DlpDownloadObserver::OnDownloadCreated(download::DownloadItem* item) {
  chromeos::DlpClient* dlp_client = chromeos::DlpClient::Get();
  if (!dlp_client || !dlp_client->IsAlive()) {
    return;
  }
  item->AddObserver(this);
}

void DlpDownloadObserver::OnDownloadUpdated(download::DownloadItem* item) {
  if (item->GetState() == download::DownloadItem::DownloadState::CANCELLED ||
      item->GetState() == download::DownloadItem::DownloadState::COMPLETE) {
    item->RemoveObserver(this);
  }
  if (item->GetState() != download::DownloadItem::DownloadState::COMPLETE ||
      item->GetFullPath().empty()) {
    return;
  }

  chromeos::DlpClient* dlp_client = chromeos::DlpClient::Get();
  if (!dlp_client || !dlp_client->IsAlive()) {
    return;
  }

  ::dlp::AddFilesRequest dlp_request;
  if (item->IsSavePackageDownload() &&
      download::DownloadSaveItemData::GetItemData(item)) {
    std::vector<download::DownloadSaveItemData::ItemInfo>* item_infos =
        download::DownloadSaveItemData::GetItemData(item);
    // The list of items might be empty. E.g. when the page is saved as MHTML.
    // In this case the download item refers holds the information for the one
    // file created.
    for (auto save_item : *item_infos) {
      auto* request = dlp_request.add_add_file_requests();
      request->set_file_path(save_item.file_path.value());
      request->set_source_url(GeneralizeURL(save_item.url, *item).spec());
      request->set_referrer_url(save_item.referrer_url.spec());
    }
  }
  if (dlp_request.add_file_requests().empty()) {
    auto* request = dlp_request.add_add_file_requests();
    request->set_file_path(item->GetFullPath().value());
    request->set_source_url(GeneralizeURL(item->GetURL(), *item).spec());
    request->set_referrer_url(item->GetReferrerUrl().spec());
  }
  dlp_client->AddFiles(dlp_request, base::DoNothing());
}

}  // namespace policy
