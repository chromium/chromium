// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/test_external_cache.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/values.h"
#include "chrome/browser/ash/extensions/external_cache_delegate.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "extensions/browser/updater/extension_downloader_delegate.h"

namespace chromeos {

TestExternalCache::TestExternalCache(ExternalCacheDelegate* delegate,
                                     bool always_check_for_updates)
    : delegate_(delegate),
      always_check_for_updates_(always_check_for_updates) {}

TestExternalCache::~TestExternalCache() = default;

const base::Value::Dict& TestExternalCache::GetCachedExtensions() {
  return cached_extensions_;
}

void TestExternalCache::Shutdown(base::OnceClosure callback) {
  std::move(callback).Run();
}

void TestExternalCache::UpdateExtensionsList(base::Value::Dict prefs) {
  configured_extensions_ = std::move(prefs);
  cached_extensions_.clear();

  if (configured_extensions_.empty()) {
    delegate_->OnExtensionListsUpdated(cached_extensions_);
    return;
  }

  UpdateCachedExtensions();
}

void TestExternalCache::OnDamagedFileDetected(const base::FilePath& path) {
  for (const auto [id, value] : cached_extensions_) {
    const std::string* entry_path =
        value.is_dict() ? value.GetDict().FindString(
                              extensions::ExternalProviderImpl::kExternalCrx)
                        : nullptr;
    if (entry_path && *entry_path == path.value()) {
      RemoveExtensions({id});
      return;
    }
  }
}

void TestExternalCache::RemoveExtensions(const std::vector<std::string>& ids) {
  if (ids.empty())
    return;

  for (const auto& id : ids) {
    cached_extensions_.Remove(id);
    configured_extensions_.Remove(id);
    crx_cache_.erase(id);
  }

  delegate_->OnExtensionListsUpdated(cached_extensions_);
}

bool TestExternalCache::GetExtension(const std::string& id,
                                     base::FilePath* file_path,
                                     std::string* version) {
  if (!crx_cache_.count(id))
    return false;
  *file_path = base::FilePath(crx_cache_[id].path);
  *version = crx_cache_[id].version;
  return true;
}

bool TestExternalCache::ExtensionFetchPending(const std::string& id) {
  return configured_extensions_.Find(id) && !cached_extensions_.Find(id);
}

void TestExternalCache::PutExternalExtension(
    const std::string& id,
    const base::FilePath& crx_file_path,
    const std::string& version,
    PutExternalExtensionCallback callback) {
  AddEntryToCrxCache(id, crx_file_path.value(), version);
  std::move(callback).Run(id, true);
}

void TestExternalCache::SetBackoffPolicy(
    std::optional<net::BackoffEntry::Policy> new_backoff_policy) {
  backoff_policy_ = new_backoff_policy;
}

bool TestExternalCache::SimulateExtensionDownloadFinished(
    const std::string& id,
    const std::string& crx_path,
    const std::string& version,
    bool is_update) {
  if (!pending_downloads_.count(id))
    return false;

  AddEntryToCrxCache(id, crx_path, version);
  delegate_->OnExtensionLoadedInCache(id, is_update);
  return true;
}

bool TestExternalCache::SimulateExtensionDownloadFailed(
    const std::string& id,
    extensions::ExtensionDownloaderDelegate::Error error) {
  if (!pending_downloads_.count(id))
    return false;

  delegate_->OnExtensionDownloadFailed(id, error);
  return true;
}

void TestExternalCache::UpdateCachedExtensions() {
  for (const auto [id, value] : configured_extensions_) {
    DCHECK(value.is_dict());
    if (GetExtensionUpdateUrl(value.GetDict(), always_check_for_updates_)
            .is_valid()) {
      pending_downloads_.insert(id);
    }

    if (crx_cache_.count(id)) {
      cached_extensions_.Set(
          id, GetExtensionValueToCache(value.GetDict(), crx_cache_[id].path,
                                       crx_cache_[id].version));
    } else if (ShouldCacheImmediately(value.GetDict())) {
      cached_extensions_.Set(id, value.Clone());
    }
  }

  delegate_->OnExtensionListsUpdated(cached_extensions_);
}

void TestExternalCache::AddEntryToCrxCache(const std::string& id,
                                           const std::string& crx_path,
                                           const std::string& version) {
  crx_cache_[id] = {crx_path, version};

  if (const base::Value::Dict* extension =
          configured_extensions_.FindDict(id)) {
    cached_extensions_.Set(
        id, GetExtensionValueToCache(*extension, crx_path, version));
    delegate_->OnExtensionListsUpdated(cached_extensions_);
  }
}

}  // namespace chromeos
