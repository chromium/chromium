// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/session/arc_app_id_provider_impl.h"

#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/profiles/profile.h"

namespace arc {

ArcAppIdProviderImpl::ArcAppIdProviderImpl() = default;
ArcAppIdProviderImpl::~ArcAppIdProviderImpl() = default;

std::string ArcAppIdProviderImpl::GetAppIdByPackageName(
    const std::string& package_name) {
  return ArcAppListPrefs::Get(ArcSessionManager::Get()->profile())
      ->GetAppIdByPackageName(package_name);
}

}  // namespace arc
