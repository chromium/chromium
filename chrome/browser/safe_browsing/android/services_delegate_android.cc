// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/android/services_delegate_android.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/telemetry/android/android_telemetry_service.h"
#include "chrome/browser/safe_browsing/telemetry/telemetry_service.h"
#include "components/safe_browsing/android/remote_database_manager.h"
#include "components/safe_browsing/buildflags.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/preferences/public/mojom/tracked_preference_validation_delegate.mojom.h"

namespace safe_browsing {

// static
std::unique_ptr<ServicesDelegate> ServicesDelegate::Create(
    SafeBrowsingService* safe_browsing_service) {
  return base::WrapUnique(new ServicesDelegateAndroid(safe_browsing_service));
}

// static
std::unique_ptr<ServicesDelegate> ServicesDelegate::CreateForTest(
    SafeBrowsingService* safe_browsing_service,
    ServicesDelegate::ServicesCreator* services_creator) {
  NOTREACHED();
  return base::WrapUnique(new ServicesDelegateAndroid(safe_browsing_service));
}

ServicesDelegateAndroid::ServicesDelegateAndroid(
    SafeBrowsingService* safe_browsing_service)
    : ServicesDelegate(safe_browsing_service, /*services_creator=*/nullptr) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

ServicesDelegateAndroid::~ServicesDelegateAndroid() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void ServicesDelegateAndroid::InitializeCsdService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {}

const scoped_refptr<SafeBrowsingDatabaseManager>&
ServicesDelegateAndroid::database_manager() const {
  return database_manager_;
}

void ServicesDelegateAndroid::Initialize() {
  if (!database_manager_set_for_tests_) {
    database_manager_ =
        base::WrapRefCounted(new RemoteSafeBrowsingDatabaseManager());
  }
}

void ServicesDelegateAndroid::SetDatabaseManagerForTest(
    SafeBrowsingDatabaseManager* database_manager) {
  database_manager_set_for_tests_ = true;
  database_manager_ = database_manager;
}

void ServicesDelegateAndroid::ShutdownServices() {
  telemetry_service_.reset();
}

void ServicesDelegateAndroid::RefreshState(bool enable) {}

void ServicesDelegateAndroid::ProcessResourceRequest(
    const ResourceRequestInfo* request) {}

std::unique_ptr<prefs::mojom::TrackedPreferenceValidationDelegate>
ServicesDelegateAndroid::CreatePreferenceValidationDelegate(Profile* profile) {
  return nullptr;
}

void ServicesDelegateAndroid::RegisterDelayedAnalysisCallback(
    const DelayedAnalysisCallback& callback) {}

void ServicesDelegateAndroid::AddDownloadManager(
    content::DownloadManager* download_manager) {}

ClientSideDetectionService* ServicesDelegateAndroid::GetCsdService() {
  return nullptr;
}

DownloadProtectionService* ServicesDelegateAndroid::GetDownloadService() {
  return nullptr;
}

void ServicesDelegateAndroid::StartOnIOThread(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const V4ProtocolConfig& v4_config) {
  database_manager_->StartOnIOThread(url_loader_factory, v4_config);
}

void ServicesDelegateAndroid::StopOnIOThread(bool shutdown) {
  database_manager_->StopOnIOThread(shutdown);
}

void ServicesDelegateAndroid::CreateTelemetryService(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile);

  if (profile->IsOffTheRecord())
    return;

  DCHECK(!telemetry_service_);
  telemetry_service_ = std::make_unique<AndroidTelemetryService>(
      safe_browsing_service_, profile);
}

void ServicesDelegateAndroid::RemoveTelemetryService(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (telemetry_service_ && telemetry_service_->profile() == profile)
    telemetry_service_.reset();
}

void ServicesDelegateAndroid::CreateBinaryUploadService(Profile* profile) {}
void ServicesDelegateAndroid::RemoveBinaryUploadService(Profile* profile) {}
BinaryUploadService* ServicesDelegateAndroid::GetBinaryUploadService(
    Profile* profile) const {
  NOTIMPLEMENTED();
  return nullptr;
}

std::string ServicesDelegateAndroid::GetSafetyNetId() const {
  return database_manager_->GetSafetyNetId();
}

}  // namespace safe_browsing
