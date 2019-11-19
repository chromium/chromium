// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/deferred_client_wrapper.h"

#include <vector>

#include "base/bind.h"
#include "build/build_config.h"
#include "chrome/browser/android/startup_bridge.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/transition_manager/full_browser_transition_manager.h"
#include "components/download/internal/background_service/stats.h"
#include "components/download/public/background_service/clients.h"
#include "components/download/public/background_service/download_metadata.h"
#include "components/keyed_service/core/simple_factory_key.h"

namespace download {

DeferredClientWrapper::DeferredClientWrapper(DownloadClient client_id,
                                             ClientFactory client_factory,
                                             SimpleFactoryKey* key)
    : client_factory_(std::move(client_factory)), key_(key) {
#if defined(OS_ANDROID)
  client_id_ = client_id;
  full_browser_requested_ = false;
#endif

  FullBrowserTransitionManager::Get()->RegisterCallbackOnProfileCreation(
      key_, base::BindOnce(&DeferredClientWrapper::InflateClient,
                           weak_ptr_factory_.GetWeakPtr()));
#if !defined(OS_ANDROID)
  // On non-android platforms we can only be running in full browser mode. In
  // full browser mode, FullBrowserTransitionManager synchronously calls the
  // callback when it is registered.
  DCHECK(wrapped_client_);
#endif
}

DeferredClientWrapper::~DeferredClientWrapper() = default;

void DeferredClientWrapper::OnServiceInitialized(
    bool state_lost,
    const std::vector<DownloadMetaData>& downloads) {
  base::OnceClosure callback =
      base::BindOnce(&DeferredClientWrapper::ForwardOnServiceInitialized,
                     weak_ptr_factory_.GetWeakPtr(), state_lost, downloads);
  deferred_closures_.push_back(std::move(callback));

  bool force_inflate = downloads.size() > 0 || state_lost;
  RunDeferredClosures(force_inflate);
}

void DeferredClientWrapper::OnServiceUnavailable() {
  base::OnceClosure callback =
      base::BindOnce(&DeferredClientWrapper::ForwardOnServiceUnavailable,
                     weak_ptr_factory_.GetWeakPtr());
  deferred_closures_.push_back(std::move(callback));
  RunDeferredClosures(false /*force_inflate*/);
}

void DeferredClientWrapper::OnDownloadStarted(
    const std::string& guid,
    const std::vector<GURL>& url_chain,
    const scoped_refptr<const net::HttpResponseHeaders>& headers) {
  base::OnceClosure callback =
      base::BindOnce(&DeferredClientWrapper::ForwardOnDownloadStarted,
                     weak_ptr_factory_.GetWeakPtr(), guid, url_chain, headers);
  deferred_closures_.push_back(std::move(callback));
  RunDeferredClosures(true /*force_inflate*/);
}

void DeferredClientWrapper::OnDownloadUpdated(const std::string& guid,
                                              uint64_t bytes_uploaded,
                                              uint64_t bytes_downloaded) {
  base::OnceClosure callback = base::BindOnce(
      &DeferredClientWrapper::ForwardOnDownloadUpdated,
      weak_ptr_factory_.GetWeakPtr(), guid, bytes_uploaded, bytes_downloaded);
  deferred_closures_.push_back(std::move(callback));
  RunDeferredClosures(true /*force_inflate*/);
}

void DeferredClientWrapper::OnDownloadFailed(
    const std::string& guid,
    const download::CompletionInfo& info,
    FailureReason reason) {
  base::OnceClosure callback =
      base::BindOnce(&DeferredClientWrapper::ForwardOnDownloadFailed,
                     weak_ptr_factory_.GetWeakPtr(), guid, info, reason);
  deferred_closures_.push_back(std::move(callback));
  RunDeferredClosures(true /*force_inflate*/);
}

void DeferredClientWrapper::OnDownloadSucceeded(
    const std::string& guid,
    const CompletionInfo& completion_info) {
  base::OnceClosure callback =
      base::BindOnce(&DeferredClientWrapper::ForwardOnDownloadSucceeded,
                     weak_ptr_factory_.GetWeakPtr(), guid, completion_info);
  deferred_closures_.push_back(std::move(callback));
  RunDeferredClosures(true /*force_inflate*/);
}

bool DeferredClientWrapper::CanServiceRemoveDownloadedFile(
    const std::string& guid,
    bool force_delete) {
  base::OnceClosure callback = base::BindOnce(
      &DeferredClientWrapper::ForwardCanServiceRemoveDownloadedFile,
      weak_ptr_factory_.GetWeakPtr(), guid, force_delete);
  deferred_closures_.push_back(std::move(callback));
  RunDeferredClosures(force_delete /*force_inflate*/);
  return false;
}

void DeferredClientWrapper::GetUploadData(
    const std::string& guid,
    GetUploadDataCallback upload_callback) {
  base::OnceClosure callback = base::BindOnce(
      &DeferredClientWrapper::ForwardGetUploadData,
      weak_ptr_factory_.GetWeakPtr(), guid, std::move(upload_callback));
  deferred_closures_.push_back(std::move(callback));
  RunDeferredClosures(true /*force_inflate*/);
}

void DeferredClientWrapper::ForwardOnServiceInitialized(
    bool state_lost,
    const std::vector<DownloadMetaData>& downloads) {
  wrapped_client_->OnServiceInitialized(state_lost, downloads);
}

void DeferredClientWrapper::ForwardOnServiceUnavailable() {
  wrapped_client_->OnServiceUnavailable();
}

void DeferredClientWrapper::ForwardOnDownloadStarted(
    const std::string& guid,
    const std::vector<GURL>& url_chain,
    const scoped_refptr<const net::HttpResponseHeaders>& headers) {
  wrapped_client_->OnDownloadStarted(guid, url_chain, headers);
}

void DeferredClientWrapper::ForwardOnDownloadUpdated(
    const std::string& guid,
    uint64_t bytes_uploaded,
    uint64_t bytes_downloaded) {
  wrapped_client_->OnDownloadUpdated(guid, bytes_uploaded, bytes_downloaded);
}

void DeferredClientWrapper::ForwardOnDownloadFailed(
    const std::string& guid,
    const download::CompletionInfo& info,
    FailureReason reason) {
  wrapped_client_->OnDownloadFailed(guid, info, reason);
}

void DeferredClientWrapper::ForwardOnDownloadSucceeded(
    const std::string& guid,
    const CompletionInfo& completion_info) {
  wrapped_client_->OnDownloadSucceeded(guid, completion_info);
}

void DeferredClientWrapper::ForwardCanServiceRemoveDownloadedFile(
    const std::string& guid,
    bool force_delete) {
  wrapped_client_->CanServiceRemoveDownloadedFile(guid, force_delete);
}

void DeferredClientWrapper::ForwardGetUploadData(
    const std::string& guid,
    GetUploadDataCallback callback) {
  wrapped_client_->GetUploadData(guid, std::move(callback));
}

void DeferredClientWrapper::RunDeferredClosures(bool force_inflate) {
  if (wrapped_client_) {
    DoRunDeferredClosures();
  } else if (force_inflate) {
#if defined(OS_ANDROID)
    // The constructor registers InflateClient as a callback with
    // FullBrowserTransitionManager on Profile creation. We just need to trigger
    // loading full browser. Once full browser is loaded and  profile is
    // created, FullBrowserTransitionManager will call InflateClient.
    LaunchFullBrowser();
#else
    // For platforms that do not implement reduced mode (i.e. non-android), the
    // wrapped client should have been inflated in the constructor.
    NOTREACHED();
#endif
  }
}

void DeferredClientWrapper::DoRunDeferredClosures() {
  DCHECK(wrapped_client_);
  for (auto& closure : deferred_closures_) {
    std::move(closure).Run();
  }
  deferred_closures_.clear();
}

void DeferredClientWrapper::InflateClient(Profile* profile) {
  DCHECK(profile);
  DCHECK(client_factory_);
  wrapped_client_ = std::move(client_factory_).Run(profile);
  DoRunDeferredClosures();
}

#if defined(OS_ANDROID)
void DeferredClientWrapper::LaunchFullBrowser() {
  if (full_browser_requested_)
    return;
  full_browser_requested_ = true;
  stats::LogDownloadClientInflatedFullBrowser(client_id_);
  android_startup::LoadFullBrowser();
}
#endif

}  // namespace download
