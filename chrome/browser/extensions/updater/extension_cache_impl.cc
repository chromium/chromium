// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/updater/extension_cache_impl.h"

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/updater/chromeos_extension_cache_delegate.h"
#include "chrome/browser/extensions/updater/local_extension_cache.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/browser/install/sandboxed_unpacker_failure_reason.h"

namespace extensions {

ExtensionCacheImpl::ExtensionCacheImpl(
    std::unique_ptr<ChromeOSExtensionCacheDelegate> delegate,
    base::TaskPriority task_priority)
    : cache_(new LocalExtensionCache(
          delegate->GetCacheDir(),
          delegate->GetMaximumCacheSize(),
          delegate->GetMaximumCacheAge(),
          base::CreateSequencedTaskRunner(
              {base::ThreadPool(), base::MayBlock(), task_priority,
               base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}))) {
  notification_registrar_.Add(
      this, extensions::NOTIFICATION_EXTENSION_INSTALL_ERROR,
      content::NotificationService::AllBrowserContextsAndSources());
  cache_->Init(true, base::Bind(&ExtensionCacheImpl::OnCacheInitialized,
                                weak_ptr_factory_.GetWeakPtr()));
}

ExtensionCacheImpl::~ExtensionCacheImpl() {
}

void ExtensionCacheImpl::Start(const base::Closure& callback) {
  if (!cache_ || cache_->is_ready()) {
    DCHECK(init_callbacks_.empty());
    callback.Run();
  } else {
    init_callbacks_.push_back(callback);
  }
}

void ExtensionCacheImpl::Shutdown(const base::Closure& callback) {
  if (cache_)
    cache_->Shutdown(callback);
  else
    callback.Run();
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
                                      const PutExtensionCallback& callback) {
  if (cache_ && CachingAllowed(id))
    cache_->PutExtension(id, expected_hash, file_path, version, callback);
  else
    callback.Run(file_path, true);
}

bool ExtensionCacheImpl::CachingAllowed(const std::string& id) {
  return base::Contains(allowed_extensions_, id);
}

void ExtensionCacheImpl::OnCacheInitialized() {
  for (std::vector<base::Closure>::iterator it = init_callbacks_.begin();
       it != init_callbacks_.end(); ++it) {
    it->Run();
  }
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

void ExtensionCacheImpl::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  DCHECK_EQ(extensions::NOTIFICATION_EXTENSION_INSTALL_ERROR, type);

  if (!cache_)
    return;

  extensions::CrxInstaller* installer =
      content::Source<extensions::CrxInstaller>(source).ptr();
  const std::string& id = installer->expected_id();
  const std::string& hash = installer->expected_hash();
  const extensions::CrxInstallError* error =
      content::Details<const extensions::CrxInstallError>(details).ptr();
  const auto error_type = error->type();

  if (error_type == extensions::CrxInstallErrorType::DECLINED) {
    DVLOG(2) << "Extension install was declined, file kept";
    return;
  }

  if (error_type ==
          extensions::CrxInstallErrorType::SANDBOXED_UNPACKER_FAILURE &&
      error->sandbox_failure_detail() ==
          extensions::SandboxedUnpackerFailureReason::
              CRX_HASH_VERIFICATION_FAILED) {
    if (cache_->ShouldRetryDownload(id, hash)) {
      cache_->RemoveExtension(id, hash);
      installer->set_hash_check_failed(true);
    }
    // We deliberately keep the file with incorrect hash sum, so that it
    // will not be re-downloaded each time.
    return;
  }

  cache_->RemoveExtension(id, hash);
}

}  // namespace extensions
