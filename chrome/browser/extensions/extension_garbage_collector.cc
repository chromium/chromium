// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_garbage_collector.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/one_shot_event.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/syslog_logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/extension_garbage_collector_factory.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/install_tracker.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/file_util.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

// Wait this long before trying to garbage collect extensions again.
constexpr base::TimeDelta kGarbageCollectRetryDelay = base::Seconds(30);

// Wait this long after startup to see if there are any extensions which can be
// garbage collected.
constexpr base::TimeDelta kGarbageCollectStartupDelay = base::Seconds(30);

using ExtensionPathsMultimap = std::multimap<ExtensionId, base::FilePath>;

void CheckExtensionDirectory(const base::FilePath& path,
                             const ExtensionPathsMultimap& extension_paths) {
  base::FilePath basename = path.BaseName();
  // Clean up temporary files left if Chrome crashed or quit in the middle
  // of an extension install.
  if (basename.value() == file_util::kTempDirectoryName) {
    base::DeletePathRecursively(path);
    return;
  }

  // Parse directory name as a potential extension ID.
  ExtensionId extension_id;
  if (base::IsStringASCII(basename.value())) {
    extension_id = base::UTF16ToASCII(basename.LossyDisplayName());
    if (!crx_file::id_util::IdIsValid(extension_id))
      extension_id.clear();
  }

  // Delete directories that aren't valid IDs.
  if (extension_id.empty()) {
    base::DeletePathRecursively(path);
    return;
  }

  typedef ExtensionPathsMultimap::const_iterator Iter;
  std::pair<Iter, Iter> iter_pair = extension_paths.equal_range(extension_id);

  // If there is no entry in the prefs file, just delete the directory and
  // move on. This can legitimately happen when an uninstall does not
  // complete, for example, when a plugin is in use at uninstall time.
  if (iter_pair.first == iter_pair.second) {
    base::DeletePathRecursively(path);
    return;
  }

  // Clean up old version directories.
  base::FileEnumerator versions_enumerator(
      path, false /* Not recursive */, base::FileEnumerator::DIRECTORIES);
  for (base::FilePath version_dir = versions_enumerator.Next();
       !version_dir.empty();
       version_dir = versions_enumerator.Next()) {
    bool known_version = false;
    for (auto iter = iter_pair.first; iter != iter_pair.second; ++iter) {
      if (version_dir.BaseName() == iter->second.BaseName()) {
        known_version = true;
        break;
      }
    }
    if (!known_version)
      base::DeletePathRecursively(version_dir);
  }
}

// Deletes uninstalled extensions in the unpacked directory.
// Installed unpacked extensions are not saved in the same directory structure
// as packed extensions. For example they have no version subdirs and their root
// folders are not named with the extension's ID, so we can't use the same logic
// as packed extensions when deleting them. Note: This is meant to only handle
// unpacked .zip installs and should not be called for an `extension_directory`
// outside the profile directory because if `extension_directory` is not in
// `installed_extension_dirs` we'll delete it. Currently there's some certainty
// that `extension_directory` will not be outside the profile directory.
void CheckUnpackedExtensionDirectory(
    const base::FilePath& extension_directory,
    const ExtensionPathsMultimap& installed_extension_dirs) {
  // Check to see if the extension is installed and don't proceed if it is.
  for (auto const& [_, installed_extension_dir] : installed_extension_dirs) {
    if (extension_directory == installed_extension_dir) {
      return;
    }
  }

  base::DeletePathRecursively(extension_directory);
}

// Identifying info for an unpacked extension whose source directory existence
// is checked on a file thread.
struct UnpackedExtensionPathInfo {
  ExtensionId id;
  base::FilePath path;
  mojom::ManifestLocation location;
};

// Returns the (id, location) of each unpacked extension whose source directory
// no longer exists on disk. Runs on a file thread.
std::vector<std::pair<ExtensionId, mojom::ManifestLocation>>
GetDeletedUnpackedExtensionsOnFileThread(
    std::vector<UnpackedExtensionPathInfo> unpacked_extensions) {
  std::vector<std::pair<ExtensionId, mojom::ManifestLocation>> deleted;
  for (const auto& info : unpacked_extensions) {
    if (!base::PathExists(info.path)) {
      deleted.emplace_back(info.id, info.location);
    }
  }
  return deleted;
}

}  // namespace

ExtensionGarbageCollector::ExtensionGarbageCollector(
    content::BrowserContext* context)
    : context_(context), crx_installs_in_progress_(0) {
  ExtensionSystem* extension_system = ExtensionSystem::Get(context_);
  DCHECK(extension_system);

  extension_system->ready().PostDelayed(
      FROM_HERE,
      base::BindOnce(&ExtensionGarbageCollector::GarbageCollectExtensions,
                     weak_factory_.GetWeakPtr()),
      kGarbageCollectStartupDelay);

  InstallTrackerFactory::GetForBrowserContext(context_)->AddObserver(this);
}

ExtensionGarbageCollector::~ExtensionGarbageCollector() = default;

// static
ExtensionGarbageCollector* ExtensionGarbageCollector::Get(
    content::BrowserContext* context) {
  return ExtensionGarbageCollectorFactory::GetForBrowserContext(context);
}

void ExtensionGarbageCollector::Shutdown() {
  InstallTrackerFactory::GetForBrowserContext(context_)->RemoveObserver(this);
}

void ExtensionGarbageCollector::GarbageCollectExtensionsForTest() {
  GarbageCollectExtensions();
}

// static
void ExtensionGarbageCollector::GarbageCollectExtensionsOnFileThread(
    const base::FilePath& install_directory,
    const ExtensionPathsMultimap& extension_paths,
    bool unpacked) {
  // Nothing to clean up if it doesn't exist.
  if (!base::DirectoryExists(install_directory))
    return;

  base::FileEnumerator enumerator(install_directory,
                                  false,  // Not recursive.
                                  base::FileEnumerator::DIRECTORIES);

  for (base::FilePath extension_path = enumerator.Next();
       !extension_path.empty();
       extension_path = enumerator.Next()) {
    unpacked ? CheckUnpackedExtensionDirectory(extension_path, extension_paths)
             : CheckExtensionDirectory(extension_path, extension_paths);
  }
  // TODO(crbug.com/379867155) Remove this after chrome app kiosk crash recovery
  // is independent of extensions garbage collection.
  SYSLOG(INFO)
      << "Garbage collection for extensions on file thread is complete.";
}

void ExtensionGarbageCollector::GarbageCollectExtensions() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(context_);
  DCHECK(extension_prefs);

  if (extension_prefs->pref_service()->ReadOnly())
    return;

  if (crx_installs_in_progress_ > 0) {
    // Don't garbage collect while there are installations in progress,
    // which may be using the temporary installation directory. Try to garbage
    // collect again later.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ExtensionGarbageCollector::GarbageCollectExtensions,
                       weak_factory_.GetWeakPtr()),
        kGarbageCollectRetryDelay);
    return;
  }

  // TODO(crbug.com/40875193): Since the GC recursively deletes, insert a check
  // so that we can't attempt to delete outside the profile directory. The
  // problem is that in extension_garbage_collector_unittest.cc the directory
  // containing the extension installs is not a direct subdir of the profile
  // directory whereas this is true in production. So we can't do a simple check
  // like that to ensure we're inside the profile directory.
  ExtensionPrefs::InstallRecords extensions_info =
      extension_prefs->GetInstalledExtensionsInfo();
  std::multimap<ExtensionId, base::FilePath> extension_paths;
  std::vector<UnpackedExtensionPathInfo> unpacked_extensions;
  for (const auto& info : extensions_info) {
    extension_paths.insert(
        std::make_pair(info.extension_id, info.extension_path));
    // Collect unpacked extensions so we can later check whether their
    // source directory still exists on disk. Only kUnpacked is considered here,
    // not kCommandLine (--load-extension): command-line extensions are not
    // reloaded on restart (installed_loader.cc) and are deliberately excluded
    // when seeding the global static rule budget at startup
    // (global_rules_tracker.cc CalculateInitialAllocatedGlobalRuleCount), so a
    // stale command-line pref never leaks any allocation. Worse, clearing it
    // here would decrement the budget by an amount that was never counted,
    // underflowing allocated_global_rule_count_ (DCHECK/size_t wrap).
    if (info.extension_location == mojom::ManifestLocation::kUnpacked) {
      unpacked_extensions.push_back(
          {info.extension_id, info.extension_path, info.extension_location});
    }
  }

  extensions_info = extension_prefs->GetAllDelayedInstallInfo();
  for (const auto& info : extensions_info) {
    extension_paths.insert(
        std::make_pair(info.extension_id, info.extension_path));
  }

  ExtensionRegistrar* registrar = ExtensionRegistrar::Get(context_);
  if (!GetExtensionFileTaskRunner()->PostTask(
          FROM_HERE, base::BindOnce(&GarbageCollectExtensionsOnFileThread,
                                    registrar->install_directory(),
                                    extension_paths, /*unpacked=*/false))) {
    NOTREACHED();
  }

  if (!GetExtensionFileTaskRunner()->PostTask(
          FROM_HERE, base::BindOnce(&GarbageCollectExtensionsOnFileThread,
                                    registrar->unpacked_install_directory(),
                                    extension_paths, /*unpacked=*/true))) {
    NOTREACHED();
  }

  // Detect unpacked extensions whose source directory was deleted on disk
  // (e.g. the user removed the folder while Chrome was closed). These never
  // load, so no unload/uninstall event fires and their ExtensionPrefs are never
  // cleaned up (GetInstalledExtensionsInfo still contains them). Check path
  // existence on a file thread, then remove their stale prefs on the UI thread.
  if (!unpacked_extensions.empty()) {
    GetExtensionFileTaskRunner()->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&GetDeletedUnpackedExtensionsOnFileThread,
                       std::move(unpacked_extensions)),
        base::BindOnce(
            &ExtensionGarbageCollector::OnDeletedUnpackedExtensionsIdentified,
            weak_factory_.GetWeakPtr()));
  }
}

void ExtensionGarbageCollector::OnDeletedUnpackedExtensionsIdentified(
    std::vector<std::pair<ExtensionId, mojom::ManifestLocation>>
        deleted_extensions) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (deleted_extensions.empty()) {
    return;
  }

  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(context_);
  ExtensionRegistry* registry = ExtensionRegistry::Get(context_);
  for (const auto& [extension_id, location] : deleted_extensions) {
    // If the extension is somehow still installed (e.g. the directory
    // reappeared, or it was loaded from elsewhere before this task ran), leave
    // it alone.
    if (registry->GetInstalledExtension(extension_id)) {
      continue;
    }

    // Removing the prefs entry deletes core extension state and notifies
    // observers via ExtensionPrefsObserver::OnExtensionPrefsDeleted(), letting
    // API-specific systems (e.g. declarativeNetRequest) release the state they
    // associated with this extension.
    extension_prefs->OnExtensionUninstalled(extension_id, location,
                                            /*external_uninstall=*/false);
  }
}

void ExtensionGarbageCollector::OnBeginCrxInstall(
    content::BrowserContext* context,
    const ExtensionId& extension_id) {
  crx_installs_in_progress_++;
}

void ExtensionGarbageCollector::OnFinishCrxInstall(
    content::BrowserContext* context,
    const base::FilePath& source_file,
    const ExtensionId& extension_id,
    const Extension* extension,
    bool success) {
  crx_installs_in_progress_--;
  if (crx_installs_in_progress_ < 0) {
    // This can only happen if there is a mismatch in our begin/finish
    // accounting.
    DUMP_WILL_BE_NOTREACHED();

    // Don't let the count go negative to avoid garbage collecting when
    // an install is actually in progress.
    crx_installs_in_progress_ = 0;
  }
}

}  // namespace extensions
