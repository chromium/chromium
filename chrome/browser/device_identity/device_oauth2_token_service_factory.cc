// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"

#include <memory>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/device_identity/device_oauth2_token_service.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/device_identity/chromeos/device_oauth2_token_store_chromeos.h"
#else
#include "chrome/browser/device_identity/device_oauth2_token_store_desktop.h"
#endif

namespace {

static DeviceOAuth2TokenService* g_device_oauth2_token_service_ = nullptr;

std::unique_ptr<DeviceOAuth2TokenStore> CreatePlatformTokenStore(
    PrefService* local_state) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return std::make_unique<chromeos::DeviceOAuth2TokenStoreChromeOS>(
      local_state);
#elif BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
  return std::make_unique<DeviceOAuth2TokenStoreDesktop>(local_state);
#else
  NOTREACHED_IN_MIGRATION();
  return nullptr;
#endif
}

}  // namespace

// static
DeviceOAuth2TokenService* DeviceOAuth2TokenServiceFactory::Get() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return g_device_oauth2_token_service_;
}

// static
void DeviceOAuth2TokenServiceFactory::Initialize(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService* local_state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!g_device_oauth2_token_service_);
  g_device_oauth2_token_service_ = new DeviceOAuth2TokenService(
      url_loader_factory, CreatePlatformTokenStore(local_state));
}

// static
void DeviceOAuth2TokenServiceFactory::Shutdown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (g_device_oauth2_token_service_) {
    delete g_device_oauth2_token_service_;
    g_device_oauth2_token_service_ = nullptr;
  }
}
