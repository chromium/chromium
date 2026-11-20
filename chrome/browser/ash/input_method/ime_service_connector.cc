// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/ime_service_connector.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "chromeos/ash/services/ime/constants.h"
#include "chromeos/ash/services/ime/public/mojom/ime_service.mojom.h"
#include "chromeos/ash/services/ime/public/mojom/input_method_user_data.mojom.h"
#include "chromeos/resources/grit/chromeos_strings.h"
#include "content/public/browser/service_process_host.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

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
  // Only non-empty, relative file paths that do not reference a parent or
  // current directory or end with a separator are allowed.
  if (file_path.empty() || file_path.IsAbsolute() ||
      file_path.ReferencesParent() || file_path.EndsWithSeparator() ||
      file_path.BaseName().value() == base::FilePath::kCurrentDirectory) {
    return false;
  }

  // Target path must be restricted in the provided path.
  base::FilePath parent(ime::kInputMethodsDirName);
  parent = parent.Append(ime::kLanguageDataDirName);
  return parent.IsParent(file_path);
}

bool IsDownloadURLValid(const GURL& url) {
  return url.SchemeIs(url::kHttpsScheme) &&
         (url.DomainIs("dl.google.com") || url.DomainIs("edgedl.me.gvt1.com"));
}

std::unique_ptr<network::SimpleURLLoader> CreateUrlLoader(const GURL& url) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  // Disable cookies for this request.
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  // Raise from the default net::IDLE so SimpleURLLoader's FileWriter runs at
  // USER_VISIBLE rather than BEST_EFFORT during heavy startup/page-load work.
  resource_request->priority = net::LOW;

  auto url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  url_loader->SetRetryOptions(
      /*max_retries=*/2,
      network::SimpleURLLoader::RETRY_ON_5XX |
          network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE |
          network::SimpleURLLoader::RETRY_ON_NAME_NOT_RESOLVED);
  // TODO(https://crbug.com/162559903): Allow the client to specify the timeout.
  url_loader->SetTimeoutDuration(base::Minutes(10));
  return url_loader;
}

bool FinalizeDownloadedFile(const base::FilePath& temp_path,
                            const base::FilePath& target_path) {
  base::File::Error replace_error = base::File::FILE_OK;
  if (!base::ReplaceFile(temp_path, target_path, &replace_error)) {
    LOG(WARNING) << "Failed to replace IME file " << target_path << ": "
                 << base::File::ErrorToString(replace_error);
    base::DeleteFile(temp_path);
    return false;
  }
  return true;
}

}  // namespace

ImeServiceConnector::ActiveDownloadRequest::ActiveDownloadRequest(
    uint64_t id,
    GURL url,
    base::FilePath target_path)
    : id(id), url(std::move(url)), target_path(std::move(target_path)) {}

ImeServiceConnector::ActiveDownloadRequest::~ActiveDownloadRequest() = default;

ImeServiceConnector::ImeServiceConnector(Profile* profile)
    : profile_(profile),
      url_loader_factory_(profile->GetURLLoaderFactory()),
      file_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE})) {
  profile_observation_.Observe(profile);
}

ImeServiceConnector::~ImeServiceConnector() = default;

void ImeServiceConnector::DownloadImeFileTo(
    const GURL& url,
    const base::FilePath& file_path,
    DownloadImeFileToCallback callback) {
  // Validate `profile_`, `url`, and `file_path`, returning an empty file path
  // if any are invalid.
  if (!profile_ || !IsDownloadURLValid(url) ||
      !IsDownloadPathValid(file_path)) {
    std::move(callback).Run(base::FilePath());
    return;
  }

  base::FilePath full_path = profile_->GetPath().Append(file_path);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ImeServiceConnector::MaybeTriggerDownload,
                                weak_ptr_factory_.GetWeakPtr(), url, full_path,
                                std::move(callback)));
}

void ImeServiceConnector::OnProfileWillBeDestroyed(Profile* profile) {
  // Resolve any in-flight Mojo callbacks before `profile_` is torn down while
  // `platform_access_receiver_` is still bound.
  NotifyAllDownloadListeners(base::FilePath());
  url_loader_factory_.reset();
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

void ImeServiceConnector::MaybeTriggerDownload(
    GURL url,
    base::FilePath file_path,
    DownloadImeFileToCallback callback) {
  if (!profile_ || !url_loader_factory_) {
    std::move(callback).Run(base::FilePath());
    return;
  }

  // Do not trigger a new download if one is already in progress (including
  // final file replacement) for the same URL. Store the callback given and run
  // it when the current download finishes.
  if (active_request_ && active_request_->url == url) {
    active_request_->callbacks.emplace_back(std::move(callback));
    return;
  }

  // Switching input methods tears down the previous decoder instance and its
  // pending download in the IME service while leaving an active download in the
  // browser process (e.g. a background download of `en-840` started at login).
  // Abort the superseded download and notify its listeners before starting the
  // new request so the newly activated input method's language pack download is
  // not rejected.
  if (active_request_) {
    NotifyAllDownloadListeners(base::FilePath());
  }

  // Download to a per-attempt temporary path in the target directory and
  // atomically rename on completion so partial archives are never visible at
  // `file_path` and delayed cleanup of a superseded `SimpleURLLoader` cannot
  // delete a newer attempt's file.
  const uint64_t request_id = ++next_request_id_;
  const base::FilePath temp_path = file_path.AddExtensionASCII(
      base::StrCat({"tmp.", base::NumberToString(request_id)}));
  CHECK(!temp_path.empty());

  active_request_.emplace(request_id, std::move(url), std::move(file_path));
  active_request_->callbacks.emplace_back(std::move(callback));
  active_request_->url_loader = CreateUrlLoader(active_request_->url);
  active_request_->url_loader->SetOnRedirectCallback(base::BindRepeating(
      &ImeServiceConnector::OnRedirect, weak_ptr_factory_.GetWeakPtr()));
  active_request_->url_loader->DownloadToFile(
      url_loader_factory_.get(),
      base::BindOnce(&ImeServiceConnector::HandleDownloadResponse,
                     weak_ptr_factory_.GetWeakPtr(), request_id),
      temp_path);
}

void ImeServiceConnector::HandleDownloadResponse(
    uint64_t request_id,
    base::FilePath downloaded_path) {
  if (!active_request_ || active_request_->id != request_id) {
    return;
  }

  if (downloaded_path.empty()) {
    const int net_error = active_request_->url_loader->NetError();
    const network::mojom::URLResponseHead* response_info =
        active_request_->url_loader->ResponseInfo();
    if (response_info && response_info->headers) {
      LOG(WARNING) << "Failed to download IME file from "
                   << active_request_->url << ": "
                   << net::ErrorToString(net_error) << " (HTTP "
                   << response_info->headers->response_code() << ")";
    } else {
      LOG(WARNING) << "Failed to download IME file from "
                   << active_request_->url << ": "
                   << net::ErrorToString(net_error);
    }
    NotifyAllDownloadListeners(base::FilePath());
    return;
  }

  active_request_->url_loader.reset();
  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FinalizeDownloadedFile, std::move(downloaded_path),
                     active_request_->target_path),
      base::BindOnce(&ImeServiceConnector::OnFileFinalized,
                     weak_ptr_factory_.GetWeakPtr(), request_id));
}

void ImeServiceConnector::OnFileFinalized(uint64_t request_id, bool success) {
  if (!active_request_ || active_request_->id != request_id) {
    return;
  }
  NotifyAllDownloadListeners(success ? std::move(active_request_->target_path)
                                     : base::FilePath());
}

void ImeServiceConnector::NotifyAllDownloadListeners(base::FilePath file_path) {
  if (!active_request_) {
    return;
  }

  // Clear the currently active request (resetting `url_loader`) before running
  // callbacks in case a listener synchronously triggers a new download request.
  std::vector<DownloadImeFileToCallback> callbacks =
      std::move(active_request_->callbacks);
  active_request_.reset();

  for (auto& callback : callbacks) {
    std::move(callback).Run(file_path);
  }
}

void ImeServiceConnector::OnRedirect(
    const GURL& url_before_redirect,
    const net::RedirectInfo& redirect_info,
    const network::mojom::URLResponseHead& response_head,
    std::vector<std::string>* removed_headers) {
  if (!IsDownloadURLValid(redirect_info.new_url)) {
    NotifyAllDownloadListeners(base::FilePath());
  }
}

}  // namespace input_method
}  // namespace ash
