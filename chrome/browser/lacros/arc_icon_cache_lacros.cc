// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/arc_icon_cache_lacros.h"

#include "chromeos/lacros/lacros_service.h"

ArcIconCacheLacros::ArcIconCacheLacros() = default;
ArcIconCacheLacros::~ArcIconCacheLacros() = default;

void ArcIconCacheLacros::Start() {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service->IsAvailable<crosapi::mojom::Arc>()) {
    LOG(WARNING) << "ARC is not supported in Lacros.";
    return;
  }
  lacros_service->GetRemote<crosapi::mojom::Arc>()->AddObserver(
      receiver_.BindNewPipeAndPassRemoteWithVersion());
}

void ArcIconCacheLacros::OnIconInvalidated(const std::string& package_name) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  icon_loader_.InvalidateIcons(package_name);
}
