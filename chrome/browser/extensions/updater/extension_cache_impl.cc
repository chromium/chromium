// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/updater/extension_cache_impl.h"

#include <stddef.h>
#include <stdint.h>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/version.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/updater/chromeos_extension_cache_delegate.h"
#include "chrome/browser/extensions/updater/local_extension_cache.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/browser/install/sandboxed_unpacker_failure_reason.h"

namespace extensions {

ExtensionCacheImpl::ExtensionCacheImpl(
    std::unique_ptr<ChromeOSExtensionCacheDelegate> delegate)
    : cache_(new LocalExtensionCache(
          delegate->GetCacheDir(),
          delegate->GetMaximumCacheSize(),
          delegate->GetMaximumCacheAge(),
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
               base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}))) {
  cache_->Init(true, base::BindOnce(&ExtensionCacheImpl::OnCacheInitialized,
                                    weak_ptr_factory_.GetWeakPtr()));
}

ExtensionCacheImpl::~ExtensionCacheImpl() = default;

void ExtensionCacheImpl::Start(base::OnceClosure callback) {
  if (!cache_ || cache_->is_ready()) {
    DCHECK(init_callbacks_.empty());
    std::move(callback).Run();
  } else {
    init_callbacks_.push_back(std::move(callback));
  }
}

void ExtensionCacheImpl::Shutdown(base::OnceClosure callback) {
  if (cache_)
    cache_->Shutdown(std::move(callback));
  else
    std::move(callback).Run();
}

void ExtensionCacheImpl::AllowCaching(const std::string& id) {
  allowed_extensions_.insert(id);
}

bool ExtensionCacheImpl::GetExtension(const std::string& id,
                                      const std::string& expected_hash,
                                      base::FilePath* file_path,
                                      std::string* version) {
  if (cache_ && CachingAllowed(id))
    return cache_->GetExtension(id, expected_hash, file_path, version);
  else
    return false;
}

void ExtensionCacheImpl::PutExtension(const std::string& id,
                                      const std::string& expected_hash,
                                      const base::FilePath& file_path,
                                      const std::string& version,
                                      PutExtensionCallback callback) {
  if (cache_ && CachingAllowed(id)) {
    cache_->PutExtension(id, expected_hash, file_path, base::Version(version),
                         std::move(callback));
  } else {
    std::move(callback).Run(file_path, true);
  }
}

bool ExtensionCacheImpl::CachingAllowed(const std::string& id) {
  return base::Contains(allowed_extensions_, id);
}

void ExtensionCacheImpl::OnCacheInitialized() {
  for (auto& callback : init_callbacks_)
    std::move(callback).Run();
  init_callbacks_.clear();

  uint64_t cache_size = 0;
  size_t extensions_count = 0;
  if (cache_->GetStatistics(&cache_size, &extensions_count)) {
    UMA_HISTOGRAM_COUNTS_100("Extensions.ExtensionCacheCount",
                             extensions_count);
    UMA_HISTOGRAM_MEMORY_MB("Extensions.ExtensionCacheSize",
                            cache_size / (1024 * 1024));
  }
}

bool ExtensionCacheImpl::OnInstallFailed(const std::string& id,
                                         const std::string& hash,
                                         const CrxInstallError& error) {
  if (!cache_)
    return false;

  if (error.type() == extensions::CrxInstallErrorType::DECLINED) {
    DVLOG(2) << "Extension install was declined, file kept";
    return false;
  }
  // Remove and retry download if the crx present in the cache is corrupted or
  // not according to the expectations,
  if (error.IsCrxVerificationFailedError() ||
      error.IsCrxExpectationsFailedError()) {
    if (cache_->ShouldRetryDownload(id, hash)) {
      cache_->RemoveExtension(id, hash);
      return true;
    }
    return false;
  }

  cache_->RemoveExtension(id, hash);
  return true;
}

}  // namespace extensions
