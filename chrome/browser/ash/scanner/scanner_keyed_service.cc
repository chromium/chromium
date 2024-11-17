// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanner/scanner_keyed_service.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/drive/service/drive_api_service.h"
#include "components/drive/service/drive_service_interface.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/scanner.pb.h"
#include "components/manta/scanner_provider.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/common/auth_service.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/drive/drive_api_url_generator.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace {

constexpr auto kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("chromeos_scanner", R"(
      semantics {
        sender: "ChromeOS Scanner"
        description:
          "Creates or mutates various Google entities - Docs, Sheets, "
          "Contacts, etc. - for the user. These operations are specified by "
          "responses from the Scanner service."
        trigger:
          "User selecting to create or mutate entities from the Scanner UI."
        internal {
          contacts {
            email: "e14s-eng@google.com"
          }
        }
        user_data {
          type: USER_CONTENT
        }
        data:
          "Files created by the Scanner service."
        destination: GOOGLE_OWNED_SERVICE
        last_reviewed: "2024-10-21"
      }
      policy {
        cookies_allowed: NO
        setting:
          "No setting. Users must take explicit action to trigger the feature."
        policy_exception_justification:
          "Not implemented, not considered useful. This request is part of a "
          "flow which is user-initiated."
      }
    )");

}  // namespace

ScannerKeyedService::ScannerKeyedService(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<manta::ScannerProvider> scanner_provider)
    : identity_manager_(identity_manager),
      scanner_provider_(std::move(scanner_provider)) {
  if (identity_manager_ != nullptr) {
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner =
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
             base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});
    CoreAccountId account_id =
        identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);

    const GURL& base_url = GaiaUrls::GetInstance()->google_apis_origin_url();
    GURL base_thumbnail_url(
        google_apis::DriveApiUrlGenerator::kBaseThumbnailUrlForProduction);

    drive_service_ = std::make_unique<drive::DriveAPIService>(
        identity_manager_, url_loader_factory, blocking_task_runner.get(),
        base_url, base_thumbnail_url,
        /*custom_user_agent=*/"", kTrafficAnnotation);
    drive_service_->Initialize(account_id);

    auto auth_service = std::make_unique<google_apis::AuthService>(
        identity_manager_, account_id, url_loader_factory,
        std::vector<std::string>{GaiaConstants::kContactsOAuth2Scope});
    request_sender_ = std::make_unique<google_apis::RequestSender>(
        std::move(auth_service), url_loader_factory, blocking_task_runner,
        /*custom_user_agent=*/"", kTrafficAnnotation);
  }
}

ScannerKeyedService::~ScannerKeyedService() = default;

ash::ScannerSystemState ScannerKeyedService::GetSystemState() const {
  return system_state_provider_.GetSystemState();
}

void ScannerKeyedService::FetchActionsForImage(
    scoped_refptr<base::RefCountedMemory> jpeg_bytes,
    manta::ScannerProvider::ScannerProtoResponseCallback callback) {
  if (!scanner_provider_) {
    // `scanner_provider_` can only be nullptr when there is no identity
    // manager to instantiate the provider.
    std::move(callback).Run(
        nullptr,
        manta::MantaStatus(manta::MantaStatusCode::kNoIdentityManager));
    return;
  }
  manta::proto::ScannerInput scanner_input;
  scanner_input.set_image(std::string(base::as_string_view(*jpeg_bytes)));
  scanner_provider_->Call(scanner_input, std::move(callback));
}

void ScannerKeyedService::FetchActionDetailsForImage(
    scoped_refptr<base::RefCountedMemory> jpeg_bytes,
    manta::proto::ScannerAction selected_action,
    manta::ScannerProvider::ScannerProtoResponseCallback callback) {
  if (!scanner_provider_) {
    // `scanner_provider_` can only be nullptr when there is no identity
    // manager to instantiate the provider.
    std::move(callback).Run(
        nullptr,
        manta::MantaStatus(manta::MantaStatusCode::kNoIdentityManager));
    return;
  }
  manta::proto::ScannerInput scanner_input;
  scanner_input.set_image(std::string(base::as_string_view(*jpeg_bytes)));
  *scanner_input.mutable_selected_action() = std::move(selected_action);
  scanner_provider_->Call(scanner_input, std::move(callback));
}

drive::DriveServiceInterface* ScannerKeyedService::GetDriveService() {
  return drive_service_.get();
}

google_apis::RequestSender* ScannerKeyedService::GetGoogleApisRequestSender() {
  return request_sender_.get();
}

bool ScannerKeyedService::IsGoogler() {
  return identity_manager_ != nullptr &&
         gaia::IsGoogleInternalAccountEmail(
             identity_manager_
                 ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
                 .email);
}

void ScannerKeyedService::Shutdown() {}
