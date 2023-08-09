// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/chrome_kiosk_external_loader_broker.h"

#include "base/functional/callback.h"
#include "base/values.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "extensions/common/extension_urls.h"

namespace chromeos {

namespace {
static ChromeKioskExternalLoaderBroker* g_broker_instance = nullptr;

base::Value::Dict CreatePrimaryAppLoaderPrefs(
    const crosapi::mojom::AppInstallParams& primary_app_data) {
  return base::Value::Dict()  //
      .Set(primary_app_data.id,
           base::Value::Dict()
               .Set(extensions::ExternalProviderImpl::kExternalVersion,
                    primary_app_data.version)
               .Set(extensions::ExternalProviderImpl::kExternalCrx,
                    primary_app_data.crx_file_location)
               .Set(extensions::ExternalProviderImpl::kIsFromWebstore,
                    primary_app_data.is_store_app));
}

base::Value::Dict CreateSecondaryAppLoaderPrefs(
    const std::vector<std::string>& secondary_app_ids) {
  base::Value::Dict prefs;
  for (const std::string& id : secondary_app_ids) {
    prefs.Set(
        id, base::Value::Dict()
                .Set(extensions::ExternalProviderImpl::kExternalUpdateUrl,
                     extension_urls::GetWebstoreUpdateUrl().spec())
                .Set(extensions::ExternalProviderImpl::kIsFromWebstore, true));
  }
  return prefs;
}
}  // namespace

// static
ChromeKioskExternalLoaderBroker* ChromeKioskExternalLoaderBroker::Get() {
  if (!g_broker_instance) {
    g_broker_instance = new ChromeKioskExternalLoaderBroker();
  }
  return g_broker_instance;
}

// static
void ChromeKioskExternalLoaderBroker::Shutdown() {
  delete g_broker_instance;
}

ChromeKioskExternalLoaderBroker::ChromeKioskExternalLoaderBroker() = default;
ChromeKioskExternalLoaderBroker::~ChromeKioskExternalLoaderBroker() {
  g_broker_instance = nullptr;
}

void ChromeKioskExternalLoaderBroker::RegisterPrimaryAppInstallDataObserver(
    InstallDataChangeCallback callback) {
  primary_app_observer_ = std::move(callback);

  CallPrimaryAppObserver();
}

void ChromeKioskExternalLoaderBroker::RegisterSecondaryAppInstallDataObserver(
    InstallDataChangeCallback callback) {
  secondary_apps_observer_ = std::move(callback);

  CallSecondaryAppObserver();
}

void ChromeKioskExternalLoaderBroker::TriggerPrimaryAppInstall(
    const crosapi::mojom::AppInstallParams& install_data) {
  primary_app_data_ = install_data;

  CallPrimaryAppObserver();
}

void ChromeKioskExternalLoaderBroker::TriggerSecondaryAppInstall(
    const std::vector<std::string>& ids) {
  secondary_app_ids_ = ids;

  CallSecondaryAppObserver();
}

void ChromeKioskExternalLoaderBroker::CallPrimaryAppObserver() {
  if (primary_app_observer_ && primary_app_data_) {
    primary_app_observer_.Run(
        CreatePrimaryAppLoaderPrefs(primary_app_data_.value()));
  }
}

void ChromeKioskExternalLoaderBroker::CallSecondaryAppObserver() {
  if (secondary_apps_observer_ && secondary_app_ids_) {
    secondary_apps_observer_.Run(
        CreateSecondaryAppLoaderPrefs(secondary_app_ids_.value()));
  }
}

}  // namespace chromeos
