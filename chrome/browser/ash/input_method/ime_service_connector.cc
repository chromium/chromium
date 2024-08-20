// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/ime_service_connector.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "chromeos/ash/services/ime/constants.h"
#include "chromeos/ash/services/ime/public/mojom/ime_service.mojom.h"
#include "chromeos/ash/services/ime/public/mojom/input_method_user_data.mojom.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/service_process_host.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace ash {
namespace input_method {

namespace {

constexpr net::NetworkTrafficAnnotationTag traffic_annotation =
    net::DefineNetworkTrafficAnnotation("ime_url_downloader", R"(
    semantics {
      sender: "IME Service Downloader"
      description:
        "When user selects a new input method in ChromeOS, it may request a"
        "corresponding language module downloaded if it does not exist."
      trigger: "User switches to an input method without language module."
      data:
        "The language module download URL. No user identifier is sent."
      destination: GOOGLE_OWNED_SERVICE
    }
    policy {
      cookies_allowed: NO
      policy_exception_justification:
        "Not implemented, considered not useful."
    })");

bool IsDownloadPathValid(const base::FilePath& file_path) {
  // Only non-empty, relative path which doesn't reference a parent is allowed.
  if (file_path.empty() || file_path.IsAbsolute() ||
      file_path.ReferencesParent()) {
    return false;
  }

  // Target path must be restricted in the provided path.
  base::FilePath parent(ime::kInputMethodsDirName);
  parent = parent.Append(ime::kLanguageDataDirName);
  return parent.IsParent(file_path);
}

bool IsDownloadURLValid(const GURL& url) {
  // TODO(https://crbug.com/837156): Allowlist all URLs instead of some general
  // checks below.
  return url.SchemeIs(url::kHttpsScheme) &&
         url.DomainIs(ime::kGoogleKeyboardDownloadDomain);
}

bool ShouldUseUpdatedDownloadLogic() {
  return base::FeatureList::IsEnabled(features::kImeDownloaderUpdate);
}

std::unique_ptr<network::SimpleURLLoader> CreateUrlLoader(const GURL& url) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  // Disable cookies for this request.
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  auto url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  // TODO(https://crbug.com/971954): Allow the client to specify the timeout.
  url_loader->SetTimeoutDuration(base::Minutes(10));
  return url_loader;
}

}  // namespace

ImeServiceConnector::ImeServiceConnector(Profile* profile)
    : profile_(profile), url_loader_factory_(profile->GetURLLoaderFactory()) {
  profile_observation_.Observe(profile);
}

ImeServiceConnector::~ImeServiceConnector() = default;

void ImeServiceConnector::DownloadImeFileTo(
    const GURL& url,
    const base::FilePath& file_path,
    DownloadImeFileToCallback callback) {
  // Validate url and file_path, return an empty file path if not.
  if (!IsDownloadURLValid(url) || !IsDownloadPathValid(file_path)) {
    base::FilePath empty_path;
    std::move(callback).Run(empty_path);
    return;
  }

  if (ShouldUseUpdatedDownloadLogic()) {
    base::FilePath full_path = profile_->GetPath().Append(file_path);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ImeServiceConnector::MaybeTriggerDownload,
                                  weak_ptr_factory_.GetWeakPtr(), url,
                                  full_path, std::move(callback)));
    return;
  }

  // For now, we don't allow the client to download multi files at same time.
  // Downloading request will be aborted and return empty before the current
  // downloading task exits.
  // TODO(https://crbug.com/971954): Support multi downloads.
  // Validate url and file_path, return an empty file path if not.
  if (url_loader_) {
    base::FilePath empty_path;
    std::move(callback).Run(empty_path);
    return;
  }

  // Download the language module into a preconfigured ime folder of current
  // user's home which is allowed in IME service's sandbox.
  base::FilePath full_path = profile_->GetPath().Append(file_path);
  url_loader_ = CreateUrlLoader(url);
  url_loader_->DownloadToFile(
      url_loader_factory_.get(),
      base::BindOnce(&ImeServiceConnector::OnFileDownloadComplete,
                     base::Unretained(this), std::move(callback)),
      full_path);
}

void ImeServiceConnector::OnProfileWillBeDestroyed(Profile* profile) {
  profile_observation_.Reset();
  profile_ = nullptr;
}

void ImeServiceConnector::SetupImeService(
    mojo::PendingReceiver<ime::mojom::InputEngineManager> receiver) {
  if (!remote_service_) {
    content::ServiceProcessHost::Launch(
        remote_service_.BindNewPipeAndPassReceiver(),
        content::ServiceProcessHost::Options()
            .WithDisplayName(IDS_IME_SERVICE_DISPLAY_NAME)
            .Pass());
    remote_service_.reset_on_disconnect();

    platform_access_receiver_.reset();
    remote_service_->SetPlatformAccessProvider(
        platform_access_receiver_.BindNewPipeAndPassRemote());
  }

  remote_service_->BindInputEngineManager(std::move(receiver));
}

void ImeServiceConnector::BindInputMethodUserDataService(
    mojo::PendingReceiver<ime::mojom::InputMethodUserDataService> receiver) {
  if (!remote_service_) {
    content::ServiceProcessHost::Launch(
        remote_service_.BindNewPipeAndPassReceiver(),
        content::ServiceProcessHost::Options()
            .WithDisplayName(IDS_IME_SERVICE_DISPLAY_NAME)
            .Pass());
    remote_service_.reset_on_disconnect();

    platform_access_receiver_.reset();
    remote_service_->SetPlatformAccessProvider(
        platform_access_receiver_.BindNewPipeAndPassRemote());
  }

  remote_service_->BindInputMethodUserDataService(std::move(receiver));
}

void ImeServiceConnector::OnFileDownloadComplete(
    DownloadImeFileToCallback client_callback,
    base::FilePath path) {
  std::move(client_callback).Run(path);
  url_loader_.reset();
  return;
}

void ImeServiceConnector::MaybeTriggerDownload(
    GURL url,
    base::FilePath file_path,
    DownloadImeFileToCallback callback) {
  // Do not trigger a new download if one is already in progress for the same
  // url. Store the callback given and run it when the current download
  // finishes.
  if (url_loader_ && active_request_url_ && active_request_url_ == url.spec()) {
    download_callbacks_.emplace_back(std::move(callback));
    return;
  }

  // If the currently active request does not match the url being requested,
  // then revert to the previous logic of dropping new requests while the
  // current request is in progress.
  if (url_loader_) {
    base::FilePath empty_path;
    std::move(callback).Run(empty_path);
    return;
  }

  // Reset the download context before triggering a new download request.
  active_request_url_ = url.spec();
  download_callbacks_.clear();
  download_callbacks_.emplace_back(std::move(callback));
  url_loader_ = CreateUrlLoader(url);
  url_loader_->DownloadToFile(
      url_loader_factory_.get(),
      base::BindOnce(&ImeServiceConnector::HandleDownloadResponse,
                     weak_ptr_factory_.GetWeakPtr()),
      file_path);
}

void ImeServiceConnector::HandleDownloadResponse(base::FilePath file_path) {
  // Notify any download callbacks registered for the current request.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ImeServiceConnector::NotifyAllDownloadListeners,
                     weak_ptr_factory_.GetWeakPtr(), file_path));
}

void ImeServiceConnector::NotifyAllDownloadListeners(base::FilePath file_path) {
  while (!download_callbacks_.empty()) {
    std::move(download_callbacks_.back()).Run(file_path);
    download_callbacks_.pop_back();
  }

  // Clear the currently active request info.
  url_loader_.reset();
  active_request_url_ = std::nullopt;
}

}  // namespace input_method
}  // namespace ash
