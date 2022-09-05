// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/device_local_account_external_cache.h"

#include <memory>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/extensions/device_local_account_external_policy_loader.h"
#include "chrome/browser/chromeos/extensions/external_cache_impl.h"
#include "chrome/browser/extensions/external_loader.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromeos {

DeviceLocalAccountExternalCache::DeviceLocalAccountExternalCache(
    const base::FilePath& cache_dir)
    : cache_dir_(cache_dir) {
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
    auto val = std::make_unique<base::Value>(std::move(dict));
    external_cache_->UpdateExtensionsList(
        base::DictionaryValue::From(std::move(val)));
  }
}

void DeviceLocalAccountExternalCache::StopCache(base::OnceClosure callback) {
  if (external_cache_) {
    external_cache_->Shutdown(std::move(callback));
    external_cache_.reset();
  } else {
    std::move(callback).Run();
  }

  base::DictionaryValue empty_prefs;
  loader_->OnExtensionListsUpdated(&empty_prefs);
}

bool DeviceLocalAccountExternalCache::IsCacheRunning() const {
  return external_cache_ != nullptr;
}

void DeviceLocalAccountExternalCache::OnExtensionListsUpdated(
    const base::DictionaryValue* prefs) {
  // TODO(1323720): If this is Lacros, we need to call through mojom here
  loader_->OnExtensionListsUpdated(prefs);
}

scoped_refptr<extensions::ExternalLoader>
DeviceLocalAccountExternalCache::GetExtensionLoader() {
  return loader_;
}

}  // namespace chromeos
