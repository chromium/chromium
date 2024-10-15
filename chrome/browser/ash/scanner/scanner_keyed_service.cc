// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanner/scanner_keyed_service.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/scanner/scanner_action.h"
#include "base/check_deref.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/drive/service/drive_api_service.h"
#include "components/drive/service/drive_service_interface.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/drive/drive_api_url_generator.h"
#include "google_apis/gaia/gaia_urls.h"

namespace {

constexpr auto kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("chromeos_scanner_drive", R"(
      semantics {
        sender: "ChromeOS Scanner"
        description:
          "Uploads files created by the Scanner service to Google Drive as "
          "Drive documents."
        trigger:
          "User selecting to create a new Drive document from the Scanner UI."
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
        last_reviewed: "2024-10-08"
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
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});

  const GURL& base_url = GaiaUrls::GetInstance()->google_apis_origin_url();
  GURL base_thumbnail_url(
      google_apis::DriveApiUrlGenerator::kBaseThumbnailUrlForProduction);

  drive_service_ = std::make_unique<drive::DriveAPIService>(
      identity_manager, std::move(url_loader_factory),
      blocking_task_runner.get(), base_url, base_thumbnail_url,
      /*custom_user_agent=*/"", kTrafficAnnotation);
  drive_service_->Initialize(
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin));
}

ScannerKeyedService::~ScannerKeyedService() = default;

ash::ScannerSystemState ScannerKeyedService::GetSystemState() const {
  return system_state_provider_.GetSystemState();
}

void ScannerKeyedService::FetchActionsForImage(
    scoped_refptr<base::RefCountedMemory> jpeg_bytes,
    base::OnceCallback<void(ash::ScannerActionsResponse)> callback) {
  action_provider_.FetchActionsForImage(jpeg_bytes, std::move(callback));
}

drive::DriveServiceInterface* ScannerKeyedService::GetDriveService() {
  return drive_service_.get();
}

void ScannerKeyedService::Shutdown() {}
