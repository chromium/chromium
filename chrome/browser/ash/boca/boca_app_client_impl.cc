// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/boca_app_client_impl.h"

#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash::boca {
BocaAppClientImpl::BocaAppClientImpl() = default;

BocaAppClientImpl::~BocaAppClientImpl() = default;

signin::IdentityManager* BocaAppClientImpl::GetIdentityManager() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  return IdentityManagerFactory::GetForProfile(profile);
}

scoped_refptr<network::SharedURLLoaderFactory>
BocaAppClientImpl::GetURLLoaderFactory() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  return profile->GetURLLoaderFactory();
}

std::string BocaAppClientImpl::GetDeviceId() {
  if (!ash::DeviceSettingsService::IsInitialized()) {
    return std::string();
  }
  if (auto* policy = ash::DeviceSettingsService::Get()->policy_data()) {
    return policy->device_id();
  }
  return std::string();
}
}  // namespace ash::boca
