// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/arc/arc_icon_cache.h"

#include "base/callback.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/arc/common/intent_helper/link_handler_model.h"

ArcIconCache::ArcIconCache() = default;

ArcIconCache::~ArcIconCache() {
  arc::LinkHandlerModel::SetDelegate(nullptr);
}

void ArcIconCache::Start() {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service->IsAvailable<crosapi::mojom::Arc>()) {
    LOG(WARNING) << "ARC is not supported in Lacros.";
    return;
  }
  arc::LinkHandlerModel::SetDelegate(this);
  lacros_service->GetRemote<crosapi::mojom::Arc>()->AddObserver(
      receiver_.BindNewPipeAndPassRemoteWithVersion());
}

void ArcIconCache::OnIconInvalidated(const std::string& package_name) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  icon_loader_.InvalidateIcons(package_name);
}

ArcIconCache::GetResult ArcIconCache::GetActivityIcons(
    const std::vector<ActivityName>& activities,
    OnIconsReadyCallback cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return icon_loader_.GetActivityIcons(activities, std::move(cb));
}

bool ArcIconCache::RequestUrlHandlerList(
    const std::string& url,
    RequestUrlHandlerListCallback callback) {
  auto* service = chromeos::LacrosService::Get();
  if (!service || !service->IsAvailable<crosapi::mojom::Arc>()) {
    LOG(WARNING) << "ARC is not supported in Lacros.";
    return false;
  }

  if (service->GetInterfaceVersion(crosapi::mojom::Arc::Uuid_) <
      int{crosapi::mojom::Arc::MethodMinVersions::
              kRequestUrlHandlerListMinVersion}) {
    LOG(WARNING) << "RequestUrlHandlerList is not supported in Lacros.";
    return false;
  }

  service->GetRemote<crosapi::mojom::Arc>()->RequestUrlHandlerList(
      url, base::BindOnce(&ArcIconCache::OnRequestUrlHandlerList,
                          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  return true;
}

void ArcIconCache::OnRequestUrlHandlerList(
    RequestUrlHandlerListCallback callback,
    std::vector<crosapi::mojom::IntentHandlerInfoPtr> handlers,
    crosapi::mojom::RequestUrlHandlerListStatus status) {
  if (status == crosapi::mojom::RequestUrlHandlerListStatus::kArcNotAvailable) {
    LOG(WARNING) << "Faild to connect to ARC in ash-chrome.";
    return;
  }

  std::vector<IntentHandlerInfo> converted_handlers;
  for (auto const& handler : handlers) {
    converted_handlers.push_back(IntentHandlerInfo(
        handler->name, handler->package_name, handler->activity_name));
  }
  std::move(callback).Run(std::move(converted_handlers));
}

bool ArcIconCache::HandleUrl(const std::string& url,
                             const std::string& package_name) {
  auto* service = chromeos::LacrosService::Get();
  if (!service || !service->IsAvailable<crosapi::mojom::Arc>()) {
    LOG(WARNING) << "ARC is not supported in Lacros.";
    return false;
  }

  if (service->GetInterfaceVersion(crosapi::mojom::Arc::Uuid_) <
      int{crosapi::mojom::Arc::MethodMinVersions::kHandleUrlMinVersion}) {
    LOG(WARNING) << "HandleUrl is not supported in Lacros.";
    return false;
  }

  service->GetRemote<crosapi::mojom::Arc>()->HandleUrl(url, package_name);
  return true;
}
