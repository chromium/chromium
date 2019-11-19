// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_app_id_provider_impl.h"

#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"

namespace arc {

ArcAppIdProviderImpl::ArcAppIdProviderImpl() = default;
ArcAppIdProviderImpl::~ArcAppIdProviderImpl() = default;

std::string ArcAppIdProviderImpl::GetAppIdByPackageName(
    const std::string& package_name) {
  return ArcAppListPrefs::Get(ArcSessionManager::Get()->profile())
      ->GetAppIdByPackageName(package_name);
}

}  // namespace arc
