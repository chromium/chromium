// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/test_external_cache.h"

#include <utility>

#include "base/callback.h"
#include "base/values.h"
#include "chrome/browser/chromeos/extensions/external_cache_delegate.h"
#include "chrome/browser/extensions/external_provider_impl.h"

namespace chromeos {

TestExternalCache::TestExternalCache(ExternalCacheDelegate* delegate,
                                     bool always_check_for_updates)
    : delegate_(delegate),
      always_check_for_updates_(always_check_for_updates) {}

TestExternalCache::~TestExternalCache() = default;

const base::DictionaryValue* TestExternalCache::GetCachedExtensions() {
  return &cached_extensions_;
}

void TestExternalCache::Shutdown(base::OnceClosure callback) {
  std::move(callback).Run();
}

void TestExternalCache::UpdateExtensionsList(
    std::unique_ptr<base::DictionaryValue> prefs) {
  DCHECK(prefs);

  configured_extensions_ = std::move(prefs);
  cached_extensions_.Clear();

  if (configured_extensions_->empty()) {
    delegate_->OnExtensionListsUpdated(&cached_extensions_);
    return;
  }

  UpdateCachedExtensions();
}

void TestExternalCache::OnDamagedFileDetected(const base::FilePath& path) {
  for (const auto& entry : cached_extensions_.DictItems()) {
    const base::Value* entry_path = entry.second.FindKeyOfType(
        extensions::ExternalProviderImpl::kExternalCrx,
        base::Value::Type::STRING);
    if (entry_path && entry_path->GetString() == path.value()) {
      RemoveExtensions({entry.first});
      return;
    }
  }
}

void TestExternalCache::RemoveExtensions(const std::vector<std::string>& ids) {
  if (ids.empty())
    return;

  for (const auto& id : ids) {
    cached_extensions_.RemoveKey(id);
    configured_extensions_->RemoveKey(id);
    crx_cache_.erase(id);
  }

  delegate_->OnExtensionListsUpdated(&cached_extensions_);
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
  return configured_extensions_->FindKey(id) && !cached_extensions_.FindKey(id);
}

void TestExternalCache::PutExternalExtension(
    const std::string& id,
    const base::FilePath& crx_file_path,
    const std::string& version,
    PutExternalExtensionCallback callback) {
  AddEntryToCrxCache(id, crx_file_path.value(), version);
  std::move(callback).Run(id, true);
}

bool TestExternalCache::SimulateExtensionDownloadFinished(
    const std::string& id,
    const std::string& crx_path,
    const std::string& version) {
  if (!pending_downloads_.count(id))
    return false;

  AddEntryToCrxCache(id, crx_path, version);
  delegate_->OnExtensionLoadedInCache(id);
  return true;
}

bool TestExternalCache::SimulateExtensionDownloadFailed(const std::string& id) {
  if (!pending_downloads_.count(id))
    return false;

  delegate_->OnExtensionDownloadFailed(id);
  return true;
}

void TestExternalCache::UpdateCachedExtensions() {
  for (const auto& entry : configured_extensions_->DictItems()) {
    DCHECK(entry.second.is_dict());
    if (GetExtensionUpdateUrl(entry.second, always_check_for_updates_)
            .is_valid()) {
      pending_downloads_.insert(entry.first);
    }

    if (crx_cache_.count(entry.first)) {
      cached_extensions_.SetKey(
          entry.first,
          GetExtensionValueToCache(entry.second, crx_cache_[entry.first].path,
                                   crx_cache_[entry.first].version));
    } else if (ShouldCacheImmediately(entry.second)) {
      cached_extensions_.SetKey(entry.first, entry.second.Clone());
    }
  }

  delegate_->OnExtensionListsUpdated(&cached_extensions_);
}

void TestExternalCache::AddEntryToCrxCache(const std::string& id,
                                           const std::string& crx_path,
                                           const std::string& version) {
  crx_cache_[id] = {crx_path, version};

  const base::Value* extension =
      configured_extensions_->FindKeyOfType(id, base::Value::Type::DICTIONARY);
  if (extension) {
    cached_extensions_.SetKey(
        id, GetExtensionValueToCache(*extension, crx_path, version));
    delegate_->OnExtensionListsUpdated(&cached_extensions_);
  }
}

}  // namespace chromeos
