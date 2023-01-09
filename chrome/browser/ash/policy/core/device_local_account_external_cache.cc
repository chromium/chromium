// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/device_local_account_external_cache.h"

#include <memory>

#include "base/check_is_test.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/device_local_account_extension_service_ash.h"
#include "chrome/browser/ash/extensions/external_cache_impl.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/extensions/device_local_account_external_policy_loader.h"
#include "chrome/browser/extensions/external_loader.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromeos {

DeviceLocalAccountExternalCache::DeviceLocalAccountExternalCache(
    const std::string& user_id,
    const base::FilePath& cache_dir)
    : user_id_(user_id), cache_dir_(cache_dir) {
  loader_ = base::MakeRefCounted<DeviceLocalAccountExternalPolicyLoader>();
}

DeviceLocalAccountExternalCache::~DeviceLocalAccountExternalCache() = default;

void DeviceLocalAccountExternalCache::StartCache(
    const scoped_refptr<base::SequencedTaskRunner>& cache_task_runner) {
  DCHECK(!external_cache_);

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory =
      g_browser_process->shared_url_loader_factory();
  external_cache_ = std::make_unique<ExternalCacheImpl>(
      cache_dir_, std::move(shared_url_loader_factory), cache_task_runner, this,
      /*always_check_updates=*/true,
      /*wait_for_cache_initialization=*/false,
      /*allow_scheduled_updates=*/false);
}

void DeviceLocalAccountExternalCache::UpdateExtensionsList(
    base::Value::Dict dict) {
  if (external_cache_) {
    external_cache_->UpdateExtensionsList(std::move(dict));
  }
}

void DeviceLocalAccountExternalCache::StopCache(base::OnceClosure callback) {
  if (external_cache_) {
    external_cache_->Shutdown(std::move(callback));
    external_cache_.reset();
  } else {
    std::move(callback).Run();
  }

  base::Value::Dict empty_prefs;
  loader_->OnExtensionListsUpdated(empty_prefs);
}

bool DeviceLocalAccountExternalCache::IsCacheRunning() const {
  return external_cache_ != nullptr;
}

void DeviceLocalAccountExternalCache::OnExtensionListsUpdated(
    const base::Value::Dict& prefs) {
  if (crosapi::CrosapiManager::IsInitialized()) {
    crosapi::CrosapiManager::Get()
        ->crosapi_ash()
        ->device_local_account_extension_service()
        ->SetForceInstallExtensionsFromCache(user_id_, prefs.Clone());
  } else {
    CHECK_IS_TEST();
  }
  loader_->OnExtensionListsUpdated(prefs);
}

scoped_refptr<extensions::ExternalLoader>
DeviceLocalAccountExternalCache::GetExtensionLoader() {
  return loader_;
}

base::Value::Dict DeviceLocalAccountExternalCache::GetCachedExtensions() const {
  return external_cache_->GetCachedExtensions().Clone();
}

}  // namespace chromeos
