// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/device_local_account_external_cache.h"

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/ash/extensions/external_cache_impl.h"
#include "chrome/browser/browser_process.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromeos {

namespace {

base::Value::Dict Merge(base::Value::Dict first, base::Value::Dict second) {
  first.Merge(std::move(second));
  return first;
}

std::set<std::string> GetKeys(const base::Value::Dict& dict) {
  std::set<std::string> keys;
  for (auto [key, _] : dict) {
    keys.insert(key);
  }
  return keys;
}

base::Value::Dict FilterOnKeys(const base::Value::Dict& dict,
                               const std::set<std::string>& keys_to_keep) {
  base::Value::Dict result;
  for (auto [key, value] : dict) {
    if (keys_to_keep.contains(key)) {
      result.Set(key, value.Clone());
    }
  }
  return result;
}

}  // namespace

DeviceLocalAccountExternalCache::DeviceLocalAccountExternalCache(
    ExtensionListCallback ash_loader,
    ExtensionListCallback lacros_loader,
    const std::string& user_id,
    const base::FilePath& cache_dir)
    : user_id_(user_id),
      cache_dir_(cache_dir),
      ash_loader_(ash_loader),
      lacros_loader_(lacros_loader) {}

DeviceLocalAccountExternalCache::~DeviceLocalAccountExternalCache() = default;

void DeviceLocalAccountExternalCache::StartCache(
    const scoped_refptr<base::SequencedTaskRunner>& cache_task_runner) {
  DCHECK(!external_cache_);

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory =
      g_browser_process->shared_url_loader_factory();
  external_cache_ = std::make_unique<ExternalCacheImpl>(
      cache_dir_, std::move(shared_url_loader_factory), cache_task_runner,
      /*delegate=*/this,
      /*always_check_updates=*/true,
      /*wait_for_cache_initialization=*/false,
      /*allow_scheduled_updates=*/false);
}

void DeviceLocalAccountExternalCache::UpdateExtensionsList(
    base::Value::Dict ash_extensions,
    base::Value::Dict lacros_extensions) {
  ash_extension_keys_ = GetKeys(ash_extensions);
  lacros_extension_keys_ = GetKeys(lacros_extensions);

  if (external_cache_) {
    external_cache_->UpdateExtensionsList(
        Merge(std::move(ash_extensions), std::move(lacros_extensions)));
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
  ash_loader_.Run(user_id_, empty_prefs.Clone());
  lacros_loader_.Run(user_id_, empty_prefs.Clone());
}

bool DeviceLocalAccountExternalCache::IsCacheRunning() const {
  return external_cache_ != nullptr;
}

void DeviceLocalAccountExternalCache::OnExtensionListsUpdated(
    const base::Value::Dict& prefs) {
  lacros_loader_.Run(user_id_, FilterOnKeys(prefs, lacros_extension_keys_));
  ash_loader_.Run(user_id_, FilterOnKeys(prefs, ash_extension_keys_));
}

bool DeviceLocalAccountExternalCache::IsRollbackAllowed() const {
  return true;
}

bool DeviceLocalAccountExternalCache::CanRollbackNow() const {
  // Allow immediate rollback only if current user is not this device local
  // account.
  if (auto* user = user_manager::UserManager::Get()->GetPrimaryUser()) {
    return user_id_ != user->GetAccountId().GetUserEmail();
  }
  return true;
}

base::Value::Dict
DeviceLocalAccountExternalCache::GetCachedExtensionsForTesting() const {
  return external_cache_->GetCachedExtensions().Clone();
}

void DeviceLocalAccountExternalCache::SetCacheResponseForTesting(
    const base::Value::Dict& cached_extensions) {
  OnExtensionListsUpdated(cached_extensions);
}

}  // namespace chromeos
