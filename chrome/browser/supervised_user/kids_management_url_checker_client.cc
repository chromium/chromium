// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/kids_management_url_checker_client.h"

#include <utility>

#include "base/callback.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/supervised_user/kids_chrome_management/kids_chrome_management_client_factory.h"
#include "url/gurl.h"

namespace {

using kids_chrome_management::ClassifyUrlResponse;

safe_search_api::ClientClassification ToSafeSearchClientClassification(
    ClassifyUrlResponse* classify_url_response) {
  switch (classify_url_response->display_classification()) {
    case ClassifyUrlResponse::UNKNOWN_DISPLAY_CLASSIFICATION:
      return safe_search_api::ClientClassification::kUnknown;
    case ClassifyUrlResponse::RESTRICTED:
      return safe_search_api::ClientClassification::kRestricted;
    case ClassifyUrlResponse::ALLOWED:
      return safe_search_api::ClientClassification::kAllowed;
  }
}

}  // namespace

KidsManagementURLCheckerClient::KidsManagementURLCheckerClient(
    const std::string& country)
    : country_(country) {}

KidsManagementURLCheckerClient::~KidsManagementURLCheckerClient() = default;

void KidsManagementURLCheckerClient::CheckURL(const GURL& url,
                                              ClientCheckCallback callback) {
  auto classify_url_request =
      std::make_unique<kids_chrome_management::ClassifyUrlRequest>();
  classify_url_request->set_url(url.spec());
  classify_url_request->set_region_code(country_);

  KidsChromeManagementClient* kids_chrome_management_client =
      KidsChromeManagementClientFactory::GetInstance()->GetForBrowserContext(
          ProfileManager::GetActiveUserProfile());

  kids_chrome_management_client->ClassifyURL(
      std::move(classify_url_request),
      base::BindOnce(&KidsManagementURLCheckerClient::ConvertResponseCallback,
                     base::Unretained(this), url, std::move(callback)));
}

void KidsManagementURLCheckerClient::ConvertResponseCallback(
    const GURL& url,
    ClientCheckCallback client_callback,
    std::unique_ptr<google::protobuf::MessageLite> response_proto,
    KidsChromeManagementClient::ErrorCode error_code) {
  ClassifyUrlResponse* classify_url_response =
      static_cast<ClassifyUrlResponse*>(response_proto.get());

  DVLOG(1) << "URL classification = "
           << classify_url_response->display_classification();

  if (error_code != KidsChromeManagementClient::ErrorCode::kSuccess) {
    DVLOG(1) << "ClassifyUrl request failed.";
    std::move(client_callback)
        .Run(url, safe_search_api::ClientClassification::kUnknown);
    return;
  }

  std::move(client_callback)
      .Run(url, ToSafeSearchClientClassification(classify_url_response));
}
