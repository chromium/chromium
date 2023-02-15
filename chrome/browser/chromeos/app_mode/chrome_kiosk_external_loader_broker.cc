// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/chrome_kiosk_external_loader_broker.h"

#include "base/values.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "extensions/common/extension_urls.h"

namespace ash {

namespace {
static ChromeKioskExternalLoaderBroker* g_broker_instance = nullptr;
}

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
  primary_app_changed_handler_ = std::move(callback);

  if (primary_app_install_data_) {
    TriggerPrimaryAppInstall(primary_app_install_data_.value());
  }
}

void ChromeKioskExternalLoaderBroker::RegisterSecondaryAppInstallDataObserver(
    InstallDataChangeCallback callback) {
  secondary_apps_changed_handler_ = std::move(callback);

  if (secondary_app_ids_) {
    TriggerSecondaryAppInstall(secondary_app_ids_.value());
  }
}

void ChromeKioskExternalLoaderBroker::TriggerPrimaryAppInstall(
    const crosapi::mojom::AppInstallParams& install_data) {
  primary_app_install_data_.emplace(install_data);

  if (primary_app_changed_handler_) {
    primary_app_changed_handler_.Run(CreatePrimaryAppLoaderPrefs());
  }
}

void ChromeKioskExternalLoaderBroker::TriggerSecondaryAppInstall(
    std::vector<std::string> ids) {
  secondary_app_ids_ = ids;

  if (secondary_apps_changed_handler_) {
    secondary_apps_changed_handler_.Run(CreateSecondaryAppLoaderPrefs());
  }
}

base::Value::Dict ChromeKioskExternalLoaderBroker::CreatePrimaryAppLoaderPrefs()
    const {
  DCHECK(primary_app_install_data_.has_value());

  const std::string& id = primary_app_install_data_.value().id;
  base::Value::Dict prefs;

  base::Value::Dict extension_entry;
  if (primary_app_install_data_.value().is_store_app) {
    extension_entry.Set(extensions::ExternalProviderImpl::kIsFromWebstore,
                        true);
  }

  extension_entry.Set(extensions::ExternalProviderImpl::kExternalVersion,
                      primary_app_install_data_.value().version);
  extension_entry.Set(extensions::ExternalProviderImpl::kExternalCrx,
                      primary_app_install_data_.value().crx_file_location);
  prefs.Set(id, std::move(extension_entry));
  return prefs;
}

base::Value::Dict
ChromeKioskExternalLoaderBroker::CreateSecondaryAppLoaderPrefs() const {
  DCHECK(secondary_app_ids_.has_value());

  base::Value::Dict prefs;
  for (const std::string& id : secondary_app_ids_.value()) {
    base::Value::Dict extension_entry;
    extension_entry.Set(extensions::ExternalProviderImpl::kExternalUpdateUrl,
                        extension_urls::GetWebstoreUpdateUrl().spec());
    extension_entry.Set(extensions::ExternalProviderImpl::kIsFromWebstore,
                        true);
    prefs.Set(id, std::move(extension_entry));
  }
  return prefs;
}

}  // namespace ash
