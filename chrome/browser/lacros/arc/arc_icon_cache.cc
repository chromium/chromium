// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/arc/arc_icon_cache.h"

#include "chromeos/lacros/lacros_service.h"

ArcIconCache::ArcIconCache() = default;

ArcIconCache::~ArcIconCache() = default;

void ArcIconCache::Start() {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service->IsAvailable<crosapi::mojom::Arc>()) {
    LOG(WARNING) << "ARC is not supported in Lacros.";
    return;
  }
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
