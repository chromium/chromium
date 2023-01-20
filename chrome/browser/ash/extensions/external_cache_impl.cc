// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/external_cache_impl.h"

#include <stddef.h>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/ash/extensions/external_cache_delegate.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/extensions/updater/chrome_extension_downloader_factory.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/updater/extension_downloader.h"
#include "extensions/browser/updater/extension_downloader_types.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest.h"
#include "extensions/common/verifier_formats.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromeos {

namespace {

void FlushFile(const base::FilePath& path) {
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_WRITE);
  file.Flush();
  file.Close();
}

}  // namespace

ExternalCacheImpl::ExternalCacheImpl(
    const base::FilePath& cache_dir,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const scoped_refptr<base::SequencedTaskRunner>& backend_task_runner,
    ExternalCacheDelegate* delegate,
    bool always_check_updates,
    bool wait_for_cache_initialization,
    bool allow_scheduled_updates)
    : local_cache_(cache_dir, 0, base::TimeDelta(), backend_task_runner),
      url_loader_factory_(std::move(url_loader_factory)),
      backend_task_runner_(backend_task_runner),
      delegate_(delegate),
      always_check_updates_(always_check_updates),
      wait_for_cache_initialization_(wait_for_cache_initialization),
      allow_scheduled_updates_(allow_scheduled_updates) {
  notification_registrar_.Add(
      this, extensions::NOTIFICATION_EXTENSION_INSTALL_ERROR,
      content::NotificationService::AllBrowserContextsAndSources());
  kiosk_crx_updates_from_policy_subscription_ =
      ash::CrosSettings::Get()->AddSettingsObserver(
          ash::kKioskCRXManifestUpdateURLIgnored,
          base::BindRepeating(&ExternalCacheImpl::MaybeScheduleNextCacheCheck,
                              weak_ptr_factory_.GetWeakPtr()));
}

ExternalCacheImpl::~ExternalCacheImpl() = default;

const base::Value::Dict& ExternalCacheImpl::GetCachedExtensions() {
  return cached_extensions_;
}

void ExternalCacheImpl::Shutdown(base::OnceClosure callback) {
  local_cache_.Shutdown(std::move(callback));
}

void ExternalCacheImpl::UpdateExtensionsList(base::Value::Dict prefs) {
  extensions_ = std::move(prefs);

  if (extensions_.empty()) {
    // If list of know extensions is empty, don't init cache on disk. It is
    // important shortcut for test to don't wait forever for cache dir
    // initialization that should happen outside of Chrome on real device.
    cached_extensions_.clear();
    UpdateExtensionLoader();
    return;
  }

  CheckCache();
}

void ExternalCacheImpl::OnDamagedFileDetected(const base::FilePath& path) {
  for (const auto [key, value] : cached_extensions_) {
    if (!value.is_dict()) {
      NOTREACHED() << "ExternalCacheImpl found bad entry with type "
                   << value.type();
      continue;
    }

    const std::string* external_crx = value.GetDict().FindString(
        extensions::ExternalProviderImpl::kExternalCrx);
    if (external_crx && *external_crx == path.value()) {
      extensions::ExtensionId id = key;
      LOG(ERROR) << "ExternalCacheImpl extension at " << path.value()
                 << " failed to install, deleting it.";
      RemoveCachedExtension(id);
      UpdateExtensionLoader();

      // Don't try to DownloadMissingExtensions() from here,
      // since it can cause a fail/retry loop.
      // TODO(crbug.com/1121546) trigger re-installation mechanism with
      // exponential back-off.
      return;
    }
  }
  DLOG(ERROR) << "ExternalCacheImpl cannot find external_crx " << path.value();
}

void ExternalCacheImpl::RemoveExtensions(
    const std::vector<extensions::ExtensionId>& ids) {
  if (ids.empty())
    return;

  for (const auto& id : ids) {
    extensions_.RemoveByDottedPath(id);
    RemoveCachedExtension(id);
  }
  UpdateExtensionLoader();
}

void ExternalCacheImpl::RemoveCachedExtension(
    const extensions::ExtensionId& id) {
  cached_extensions_.RemoveByDottedPath(id);
  local_cache_.RemoveExtension(id, std::string());

  if (delegate_)
    delegate_->OnCachedExtensionFileDeleted(id);
}

bool ExternalCacheImpl::GetExtension(const extensions::ExtensionId& id,
                                     base::FilePath* file_path,
                                     std::string* version) {
  return local_cache_.GetExtension(id, std::string(), file_path, version);
}

bool ExternalCacheImpl::ExtensionFetchPending(
    const extensions::ExtensionId& id) {
  return extensions_.Find(id) && !cached_extensions_.Find(id);
}

void ExternalCacheImpl::PutExternalExtension(
    const extensions::ExtensionId& id,
    const base::FilePath& crx_file_path,
    const std::string& version,
    PutExternalExtensionCallback callback) {
  local_cache_.PutExtension(
      id, std::string(), crx_file_path, version,
      base::BindOnce(&ExternalCacheImpl::OnPutExternalExtension,
                     weak_ptr_factory_.GetWeakPtr(), id, std::move(callback)));
}

void ExternalCacheImpl::SetBackoffPolicy(
    absl::optional<net::BackoffEntry::Policy> backoff_policy) {
  backoff_policy_ = backoff_policy;
  if (downloader_) {
    // If `backoff_policy` is `absl::nullopt`, it will reset to default backoff
    // policy.
    downloader_->SetBackoffPolicy(backoff_policy);
  }
}

void ExternalCacheImpl::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  DCHECK_EQ(extensions::NOTIFICATION_EXTENSION_INSTALL_ERROR, type);

  extensions::CrxInstaller* installer =
      content::Source<extensions::CrxInstaller>(source).ptr();
  OnDamagedFileDetected(installer->source_file());
}

void ExternalCacheImpl::OnExtensionDownloadFailed(
    const extensions::ExtensionId& id,
    Error error,
    const PingResult& ping_result,
    const std::set<int>& request_ids,
    const FailureData& data) {
  if (error == Error::NO_UPDATE_AVAILABLE) {
    if (!cached_extensions_.Find(id)) {
      LOG(ERROR) << "ExternalCacheImpl extension " << id
                 << " not found on update server";
      delegate_->OnExtensionDownloadFailed(id, error);
    } else {
      // No version update for an already cached extension.
      delegate_->OnExtensionLoadedInCache(id, /*is_updated=*/false);
    }
  } else {
    LOG(ERROR) << "ExternalCacheImpl failed to download extension " << id
               << ", error " << static_cast<int>(error);
    delegate_->OnExtensionDownloadFailed(id, error);
  }
}

void ExternalCacheImpl::OnExtensionDownloadFinished(
    const extensions::CRXFileInfo& file,
    bool file_ownership_passed,
    const GURL& download_url,
    const extensions::ExtensionDownloaderDelegate::PingResult& ping_result,
    const std::set<int>& request_ids,
    InstallCallback callback) {
  DCHECK(file_ownership_passed);
  DCHECK(file.expected_version.IsValid());
  local_cache_.PutExtension(
      file.extension_id, file.expected_hash, file.path,
      file.expected_version.GetString(),
      base::BindOnce(&ExternalCacheImpl::OnPutExtension,
                     weak_ptr_factory_.GetWeakPtr(), file.extension_id));
  if (!callback.is_null())
    std::move(callback).Run(true);
}

bool ExternalCacheImpl::IsExtensionPending(const extensions::ExtensionId& id) {
  return ExtensionFetchPending(id);
}

bool ExternalCacheImpl::GetExtensionExistingVersion(
    const extensions::ExtensionId& id,
    std::string* version) {
  const base::Value::Dict* extension_dictionary =
      cached_extensions_.FindDictByDottedPath(id);
  if (!extension_dictionary) {
    return false;
  }
  const std::string* val = extension_dictionary->FindString(
      extensions::ExternalProviderImpl::kExternalVersion);
  if (!val)
    return false;
  *version = *val;
  return true;
}

void ExternalCacheImpl::UpdateExtensionLoader() {
  VLOG(1) << "Notify ExternalCacheImpl delegate about cache update";
  if (delegate_)
    delegate_->OnExtensionListsUpdated(cached_extensions_);
}

void ExternalCacheImpl::CheckCache() {
  if (local_cache_.is_shutdown())
    return;

  if (local_cache_.is_uninitialized()) {
    local_cache_.Init(wait_for_cache_initialization_,
                      base::BindOnce(&ExternalCacheImpl::CheckCache,
                                     weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // If url_loader_factory_ is missing we can't download anything.
  if (url_loader_factory_) {
    downloader_ = ChromeExtensionDownloaderFactory::CreateForURLLoaderFactory(
        url_loader_factory_, this, extensions::GetExternalVerifierFormat());
    if (backoff_policy_) {
      downloader_->SetBackoffPolicy(backoff_policy_);
    }
  }

  cached_extensions_.clear();
  for (const auto [id, value] : extensions_) {
    if (!value.is_dict()) {
      LOG(ERROR) << "ExternalCacheImpl found bad entry with type "
                 << value.type();
      continue;
    }

    base::FilePath file_path;
    std::string version;
    std::string hash;
    bool is_cached = local_cache_.GetExtension(id, hash, &file_path, &version);
    if (!is_cached)
      version = "0.0.0.0";
    if (downloader_) {
      GURL update_url =
          GetExtensionUpdateUrl(value.GetDict(), always_check_updates_);

      if (update_url.is_valid()) {
        downloader_->AddPendingExtension(extensions::ExtensionDownloaderTask(
            id, update_url,
            extensions::mojom::ManifestLocation::kExternalPolicy, false, 0,
            extensions::DownloadFetchPriority::kBackground,
            base::Version(version), extensions::Manifest::TYPE_UNKNOWN,
            std::string()));
      }
    }
    if (is_cached) {
      cached_extensions_.Set(
          id, GetExtensionValueToCache(value.GetDict(), file_path.value(),
                                       version));
    } else if (ShouldCacheImmediately(value.GetDict())) {
      cached_extensions_.Set(id, value.Clone());
    }
  }

  if (downloader_)
    downloader_->StartAllPending(nullptr);

  VLOG(1) << "Updated ExternalCacheImpl, there are "
          << cached_extensions_.size() << " extensions cached";

  UpdateExtensionLoader();

  // Cancel already-scheduled check, if any. We want scheduled update to happen
  // when update interval passes after previous check, independent of what has
  // triggered this check.
  scheduler_weak_ptr_factory_.InvalidateWeakPtrs();

  MaybeScheduleNextCacheCheck();
}

void ExternalCacheImpl::MaybeScheduleNextCacheCheck() {
  if (!allow_scheduled_updates_) {
    return;
  }
  bool kiosk_crx_updates_from_policy = false;
  if (!(ash::CrosSettings::Get()->GetBoolean(
            ash::kKioskCRXManifestUpdateURLIgnored,
            &kiosk_crx_updates_from_policy) &&
        kiosk_crx_updates_from_policy)) {
    return;
  }

  // Jitter the frequency by +/- 20% like it's done in ExtensionUpdater.
  const double jitter_factor = base::RandDouble() * 0.4 + 0.8;
  base::TimeDelta delay =
      base::Seconds(extensions::kDefaultUpdateFrequencySeconds);
  delay *= jitter_factor;
  content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&ExternalCacheImpl::CheckCache,
                         scheduler_weak_ptr_factory_.GetWeakPtr()),
          delay);
}

void ExternalCacheImpl::OnPutExtension(const extensions::ExtensionId& id,
                                       const base::FilePath& file_path,
                                       bool file_ownership_passed) {
  if (local_cache_.is_shutdown() || file_ownership_passed) {
    backend_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&base::DeletePathRecursively),
                       file_path));
    return;
  }

  VLOG(1) << "ExternalCacheImpl installed a new extension in the cache " << id;

  const base::Value::Dict* original_entry = extensions_.FindDict(id);
  if (!original_entry) {
    LOG(ERROR) << "ExternalCacheImpl cannot find entry for extension " << id;
    return;
  }

  std::string version;
  std::string hash;
  if (!local_cache_.GetExtension(id, hash, nullptr, &version)) {
    // Copy entry to don't modify it inside extensions_.
    LOG(ERROR) << "Can't find installed extension in cache " << id;
    return;
  }

  if (flush_on_put_) {
    backend_task_runner_->PostTask(FROM_HERE,
                                   base::BindOnce(&FlushFile, file_path));
  }

  // Whether the extension is updated. Must be set before cached_extensions_.Set
  // or it always returns true.
  bool is_updated = cached_extensions_.contains(id);

  cached_extensions_.Set(id, GetExtensionValueToCache(
                                 *original_entry, file_path.value(), version));

  if (delegate_)
    delegate_->OnExtensionLoadedInCache(id, is_updated);

  UpdateExtensionLoader();
}

void ExternalCacheImpl::OnPutExternalExtension(
    const extensions::ExtensionId& id,
    PutExternalExtensionCallback callback,
    const base::FilePath& file_path,
    bool file_ownership_passed) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  OnPutExtension(id, file_path, file_ownership_passed);
  std::move(callback).Run(id, !file_ownership_passed);
}

}  // namespace chromeos
