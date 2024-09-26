// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_garbage_collector.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/one_shot_event.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/extension_garbage_collector_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "chrome/browser/extensions/pending_extension_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/file_util.h"

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

  InstallTracker::Get(context_)->AddObserver(this);
}

ExtensionGarbageCollector::~ExtensionGarbageCollector() {}

// static
ExtensionGarbageCollector* ExtensionGarbageCollector::Get(
    content::BrowserContext* context) {
  return ExtensionGarbageCollectorFactory::GetForBrowserContext(context);
}

void ExtensionGarbageCollector::Shutdown() {
  InstallTracker::Get(context_)->RemoveObserver(this);
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
  ExtensionPrefs::ExtensionsInfo extensions_info =
      extension_prefs->GetInstalledExtensionsInfo();
  std::multimap<ExtensionId, base::FilePath> extension_paths;
  for (const auto& info : extensions_info) {
    extension_paths.insert(
        std::make_pair(info.extension_id, info.extension_path));
  }

  extensions_info = extension_prefs->GetAllDelayedInstallInfo();
  for (const auto& info : extensions_info) {
    extension_paths.insert(
        std::make_pair(info.extension_id, info.extension_path));
  }

  ExtensionService* service =
      ExtensionSystem::Get(context_)->extension_service();
  if (!GetExtensionFileTaskRunner()->PostTask(
          FROM_HERE, base::BindOnce(&GarbageCollectExtensionsOnFileThread,
                                    service->install_directory(),
                                    extension_paths, /*unpacked=*/false))) {
    NOTREACHED_IN_MIGRATION();
  }

  if (!GetExtensionFileTaskRunner()->PostTask(
          FROM_HERE, base::BindOnce(&GarbageCollectExtensionsOnFileThread,
                                    service->unpacked_install_directory(),
                                    extension_paths, /*unpacked=*/true))) {
    NOTREACHED_IN_MIGRATION();
  }
}

void ExtensionGarbageCollector::OnBeginCrxInstall(
    content::BrowserContext* context,
    const CrxInstaller& installer,
    const ExtensionId& extension_id) {
  crx_installs_in_progress_++;
}

void ExtensionGarbageCollector::OnFinishCrxInstall(
    content::BrowserContext* context,
    const CrxInstaller& installer,
    const ExtensionId& extension_id,
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
