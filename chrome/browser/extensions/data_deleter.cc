// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/data_deleter.h"

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/task_runner.h"
#include "chrome/browser/extensions/chrome_extension_cookies.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/api/storage/storage_frontend.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"

using base::WeakPtr;
using content::BrowserContext;
using content::BrowserThread;
using content::StoragePartition;

namespace extensions {

namespace {

// Helper function that deletes data of a given |storage_origin| in a given
// |partition| and synchronously invokes |done_callback| once the data is
// deleted.
void DeleteOrigin(Profile* profile,
                  StoragePartition* partition,
                  const GURL& origin,
                  base::OnceClosure done_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(profile);
  DCHECK(partition);

  if (origin.SchemeIs(kExtensionScheme)) {
    auto subtask_done_callback =
        base::BarrierClosure(2, std::move(done_callback));

    // TODO(ajwong): Cookies are not properly isolated for
    // chrome-extension:// scheme.  (http://crbug.com/158386).
    //
    // However, no isolated apps actually can write to kExtensionScheme
    // origins. Thus, it is benign to delete from the
    // RequestContextForExtensions because there's nothing stored there. We
    // preserve this code path without checking for isolation because it's
    // simpler than special casing.  This code should go away once we merge
    // the various URLRequestContexts (http://crbug.com/159193).
    partition->ClearDataForOrigin(
        ~StoragePartition::REMOVE_DATA_MASK_SHADER_CACHE,
        StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL, origin,
        subtask_done_callback);

    // Delete cookies separately from other data so that the request context
    // for extensions doesn't need to be passed into the StoragePartition.
    ChromeExtensionCookies::Get(profile)->ClearCookies(origin,
                                                       subtask_done_callback);
  } else {
    // We don't need to worry about the media request context because that
    // shares the same cookie store as the main request context.
    partition->ClearDataForOrigin(
        ~StoragePartition::REMOVE_DATA_MASK_SHADER_CACHE,
        StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL, origin,
        std::move(done_callback));
  }
}

void OnNeedsToGarbageCollectIsolatedStorage(WeakPtr<ExtensionService> es) {
  if (es) {
    es->profile()->GetPrefs()->SetBoolean(
        prefs::kShouldGarbageCollectStoragePartitions, true);
  }
}

}  // namespace

// static
void DataDeleter::StartDeleting(Profile* profile,
                                const Extension* extension,
                                base::OnceClosure done_callback) {
  DCHECK(profile);
  DCHECK(extension);

  // Storage deletion can take a couple different tasks, depending on the
  // extension. The number of tasks is precomputed and passed to a barrier
  // closure.
  bool has_isolated_storage = false;
  bool delete_extension_origin = false;
  bool delete_web_url_origin = false;
  bool delete_extension_storage = false;
  int num_tasks = 0;

  GURL launch_web_url_origin;
  StoragePartition* partition = nullptr;

  if (util::HasIsolatedStorage(*extension, profile)) {
    has_isolated_storage = true;
    ++num_tasks;
  } else {
    delete_extension_origin = true;
    ++num_tasks;

    launch_web_url_origin =
        AppLaunchInfo::GetLaunchWebURL(extension).DeprecatedGetOriginAsURL();
    partition =
        util::GetStoragePartitionForExtensionId(extension->id(), profile);

    ExtensionSpecialStoragePolicy* storage_policy =
        profile->GetExtensionSpecialStoragePolicy();
    if (storage_policy->NeedsProtection(extension) &&
        !storage_policy->IsStorageProtected(launch_web_url_origin)) {
      delete_web_url_origin = true;
      ++num_tasks;
    }
  }

  // StorageFrontend may not exist in unit tests.
  StorageFrontend* frontend = StorageFrontend::Get(profile);
  if (frontend) {
    delete_extension_storage = true;
    ++num_tasks;
  }

  auto subtask_done_callback =
      base::BarrierClosure(num_tasks, std::move(done_callback));
  if (has_isolated_storage) {
    profile->AsyncObliterateStoragePartition(
        util::GetPartitionDomainForExtension(extension),
        base::BindOnce(&OnNeedsToGarbageCollectIsolatedStorage,
                       ExtensionSystem::Get(profile)
                           ->extension_service()
                           ->AsExtensionServiceWeakPtr()),
        subtask_done_callback);
  }
  if (delete_extension_origin) {
    DCHECK(partition);
    DeleteOrigin(profile, partition, extension->url(), subtask_done_callback);
  }
  if (delete_web_url_origin) {
    DCHECK(partition);
    DCHECK(!launch_web_url_origin.is_empty());
    DeleteOrigin(profile, partition, launch_web_url_origin,
                 subtask_done_callback);
  }
  if (delete_extension_storage) {
    frontend->DeleteStorageSoon(extension->id(), subtask_done_callback);
  }
}

}  // namespace extensions
