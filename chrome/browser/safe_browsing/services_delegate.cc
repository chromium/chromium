// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/services_delegate.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/telemetry/telemetry_service.h"
#include "chrome/common/chrome_switches.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/db/v4_local_database_manager.h"
#include "components/safe_browsing/verdict_cache_manager.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/preferences/public/mojom/tracked_preference_validation_delegate.mojom.h"

namespace safe_browsing {

ServicesDelegate::ServicesDelegate(SafeBrowsingService* safe_browsing_service,
                                   ServicesCreator* services_creator)
    : safe_browsing_service_(safe_browsing_service),
      services_creator_(services_creator) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

ServicesDelegate::~ServicesDelegate() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void ServicesDelegate::CreatePasswordProtectionService(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile);
  auto it = password_protection_service_map_.find(profile);
  DCHECK(it == password_protection_service_map_.end());
  auto service = std::make_unique<ChromePasswordProtectionService>(
      safe_browsing_service_, profile);
  password_protection_service_map_[profile] = std::move(service);
}

void ServicesDelegate::RemovePasswordProtectionService(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile);
  auto it = password_protection_service_map_.find(profile);
  if (it != password_protection_service_map_.end())
    password_protection_service_map_.erase(it);
}

PasswordProtectionService* ServicesDelegate::GetPasswordProtectionService(
    Profile* profile) const {
  DCHECK(profile);
  auto it = password_protection_service_map_.find(profile);
  return it != password_protection_service_map_.end() ? it->second.get()
                                                      : nullptr;
}

void ServicesDelegate::CreateVerdictCacheManager(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile);
  auto it = cache_manager_map_.find(profile);
  DCHECK(it == cache_manager_map_.end());
  auto cache_manager = std::make_unique<VerdictCacheManager>(
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS),
      HostContentSettingsMapFactory::GetForProfile(profile));
  cache_manager_map_[profile] = std::move(cache_manager);
}

void ServicesDelegate::RemoveVerdictCacheManager(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile);
  auto it = cache_manager_map_.find(profile);
  if (it != cache_manager_map_.end())
    cache_manager_map_.erase(it);
}

VerdictCacheManager* ServicesDelegate::GetVerdictCacheManager(
    Profile* profile) const {
  DCHECK(profile);
  auto it = cache_manager_map_.find(profile);
  DCHECK(it != cache_manager_map_.end());
  return it->second.get();
}

}  // namespace safe_browsing
