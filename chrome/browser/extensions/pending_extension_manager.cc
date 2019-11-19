// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/pending_extension_manager.h"

#include <algorithm>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/version.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace {

// Install predicate used by AddFromExternalUpdateUrl().
bool AlwaysInstall(const extensions::Extension* extension) {
  return true;
}

std::string GetVersionString(const base::Version& version) {
  return version.IsValid() ? version.GetString() : "invalid";
}

}  // namespace

namespace extensions {

PendingExtensionManager::PendingExtensionManager(
    content::BrowserContext* context)
    : context_(context) {}

PendingExtensionManager::~PendingExtensionManager() {}

const PendingExtensionInfo* PendingExtensionManager::GetById(
    const std::string& id) const {
  PendingExtensionList::const_iterator iter;
  for (iter = pending_extension_list_.begin();
       iter != pending_extension_list_.end();
       ++iter) {
    if (id == iter->id())
      return &(*iter);
  }

  return NULL;
}

bool PendingExtensionManager::Remove(const std::string& id) {
  if (base::Contains(expected_policy_reinstalls_, id)) {
    base::TimeDelta latency =
        base::TimeTicks::Now() - expected_policy_reinstalls_[id];
    UMA_HISTOGRAM_LONG_TIMES("Extensions.CorruptPolicyExtensionResolved",
                             latency);
    expected_policy_reinstalls_.erase(id);
  }
  PendingExtensionList::iterator iter;
  for (iter = pending_extension_list_.begin();
       iter != pending_extension_list_.end();
       ++iter) {
    if (id == iter->id()) {
      pending_extension_list_.erase(iter);
      return true;
    }
  }

  return false;
}

bool PendingExtensionManager::IsIdPending(const std::string& id) const {
  return GetById(id) != NULL;
}

bool PendingExtensionManager::HasPendingExtensions() const {
  return !pending_extension_list_.empty();
}

bool PendingExtensionManager::HasPendingExtensionFromSync() const {
  PendingExtensionList::const_iterator iter;
  for (iter = pending_extension_list_.begin();
       iter != pending_extension_list_.end();
       ++iter) {
    if (iter->is_from_sync())
      return true;
  }

  return false;
}

bool PendingExtensionManager::HasHighPriorityPendingExtension() const {
  return std::find_if(
             pending_extension_list_.begin(), pending_extension_list_.end(),
             [](const PendingExtensionInfo& info) {
               return info.install_source() ==
                          Manifest::EXTERNAL_POLICY_DOWNLOAD ||
                      info.install_source() == Manifest::EXTERNAL_COMPONENT;
             }) != pending_extension_list_.end();
}

void PendingExtensionManager::ExpectPolicyReinstallForCorruption(
    const ExtensionId& id) {
  if (base::Contains(expected_policy_reinstalls_, id))
    return;
  expected_policy_reinstalls_[id] = base::TimeTicks::Now();
  UMA_HISTOGRAM_BOOLEAN("Extensions.CorruptPolicyExtensionDetected2", true);
}

bool PendingExtensionManager::IsPolicyReinstallForCorruptionExpected(
    const ExtensionId& id) const {
  return base::Contains(expected_policy_reinstalls_, id);
}

bool PendingExtensionManager::HasAnyPolicyReinstallForCorruption() const {
  return !expected_policy_reinstalls_.empty();
}

bool PendingExtensionManager::AddFromSync(
    const std::string& id,
    const GURL& update_url,
    const base::Version& version,
    PendingExtensionInfo::ShouldAllowInstallPredicate should_allow_install,
    bool remote_install) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (ExtensionRegistry::Get(context_)->GetExtensionById(
          id, ExtensionRegistry::EVERYTHING)) {
    LOG(ERROR) << "Trying to add pending extension " << id
               << " which already exists";
    return false;
  }

  // Make sure we don't ever try to install the CWS app, because even though
  // it is listed as a syncable app (because its values need to be synced) it
  // should already be installed on every instance.
  if (id == extensions::kWebStoreAppId) {
    NOTREACHED();
    return false;
  }

  static const bool kIsFromSync = true;
  static const Manifest::Location kSyncLocation = Manifest::INTERNAL;
  static const bool kMarkAcknowledged = false;

  return AddExtensionImpl(id,
                          std::string(),
                          update_url,
                          version,
                          should_allow_install,
                          kIsFromSync,
                          kSyncLocation,
                          Extension::NO_FLAGS,
                          kMarkAcknowledged,
                          remote_install);
}

bool PendingExtensionManager::AddFromExtensionImport(
    const std::string& id,
    const GURL& update_url,
    PendingExtensionInfo::ShouldAllowInstallPredicate should_allow_install) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (ExtensionRegistry::Get(context_)->GetExtensionById(
          id, ExtensionRegistry::EVERYTHING)) {
    LOG(ERROR) << "Trying to add pending extension " << id
               << " which already exists";
    return false;
  }

  static const bool kIsFromSync = false;
  static const Manifest::Location kManifestLocation = Manifest::INTERNAL;
  static const bool kMarkAcknowledged = false;
  static const bool kRemoteInstall = false;

  return AddExtensionImpl(id,
                          std::string(),
                          update_url,
                          base::Version(),
                          should_allow_install,
                          kIsFromSync,
                          kManifestLocation,
                          Extension::NO_FLAGS,
                          kMarkAcknowledged,
                          kRemoteInstall);
}

bool PendingExtensionManager::AddFromExternalUpdateUrl(
    const std::string& id,
    const std::string& install_parameter,
    const GURL& update_url,
    Manifest::Location location,
    int creation_flags,
    bool mark_acknowledged) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  static const bool kIsFromSync = false;
  static const bool kRemoteInstall = false;

  const Extension* extension = ExtensionRegistry::Get(context_)
      ->GetExtensionById(id, ExtensionRegistry::EVERYTHING);
  if (extension && location == Manifest::GetHigherPriorityLocation(
                                   location, extension->location())) {
    // If the new location has higher priority than the location of an existing
    // extension, let the update process overwrite the existing extension.
  } else {
    // Skip the installation if the extension was removed by the user and it's
    // not specified to be force-installed through the policy.
    if (!Manifest::IsPolicyLocation(location) &&
        ExtensionPrefs::Get(context_)->IsExternalExtensionUninstalled(id)) {
      return false;
    }

    if (extension) {
      LOG(DFATAL) << "Trying to add extension " << id
                  << " by external update, but it is already installed.";
      return false;
    }
  }

  return AddExtensionImpl(id,
                          install_parameter,
                          update_url,
                          base::Version(),
                          &AlwaysInstall,
                          kIsFromSync,
                          location,
                          creation_flags,
                          mark_acknowledged,
                          kRemoteInstall);
}


bool PendingExtensionManager::AddFromExternalFile(
    const std::string& id,
    Manifest::Location install_source,
    const base::Version& version,
    int creation_flags,
    bool mark_acknowledged) {
  // TODO(skerner): AddFromSync() checks to see if the extension is
  // installed, but this method assumes that the caller already
  // made sure it is not installed.  Make all AddFrom*() methods
  // consistent.
  const GURL& kUpdateUrl = GURL::EmptyGURL();
  static const bool kIsFromSync = false;
  static const bool kRemoteInstall = false;

  return AddExtensionImpl(id,
                          std::string(),
                          kUpdateUrl,
                          version,
                          &AlwaysInstall,
                          kIsFromSync,
                          install_source,
                          creation_flags,
                          mark_acknowledged,
                          kRemoteInstall);
}

void PendingExtensionManager::GetPendingIdsForUpdateCheck(
    std::list<std::string>* out_ids_for_update_check) const {
  PendingExtensionList::const_iterator iter;
  for (iter = pending_extension_list_.begin();
       iter != pending_extension_list_.end();
       ++iter) {
    Manifest::Location install_source = iter->install_source();

    // Some install sources read a CRX from the filesystem.  They can
    // not be fetched from an update URL, so don't include them in the
    // set of ids.
    if (install_source == Manifest::EXTERNAL_PREF ||
        install_source == Manifest::EXTERNAL_REGISTRY ||
        install_source == Manifest::EXTERNAL_POLICY) {
      continue;
    }

    out_ids_for_update_check->push_back(iter->id());
  }
}

bool PendingExtensionManager::AddExtensionImpl(
    const std::string& id,
    const std::string& install_parameter,
    const GURL& update_url,
    const base::Version& version,
    PendingExtensionInfo::ShouldAllowInstallPredicate should_allow_install,
    bool is_from_sync,
    Manifest::Location install_source,
    int creation_flags,
    bool mark_acknowledged,
    bool remote_install) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  PendingExtensionInfo info(id,
                            install_parameter,
                            update_url,
                            version,
                            should_allow_install,
                            is_from_sync,
                            install_source,
                            creation_flags,
                            mark_acknowledged,
                            remote_install);

  if (const PendingExtensionInfo* pending = GetById(id)) {
    // Bugs in this code will manifest as sporadic incorrect extension
    // locations in situations where multiple install sources run at the
    // same time. For example, on first login to a chrome os machine, an
    // extension may be requested by sync and the default extension set.
    // The following logging will help diagnose such issues.
    VLOG(1) << "Extension id " << id
            << " was entered for update more than once."
            << "  old location: " << pending->install_source()
            << "  new location: " << install_source
            << "  old version: " << GetVersionString(pending->version())
            << "  new version: " << GetVersionString(version);

    // Never override an existing extension with an older version. Only
    // extensions from local CRX files have a known version; extensions from an
    // update URL will get the latest version.

    // If |pending| has the same or higher precedence than |info| then don't
    // install |info| over |pending|.
    if (pending->CompareTo(info) >= 0)
      return false;

    VLOG(1) << "Overwrite existing record.";

    std::replace(pending_extension_list_.begin(),
                 pending_extension_list_.end(),
                 *pending,
                 info);
  } else {
    pending_extension_list_.push_back(info);
  }

  return true;
}

void PendingExtensionManager::AddForTesting(
    const PendingExtensionInfo& pending_extension_info) {
  pending_extension_list_.push_back(pending_extension_info);
}

}  // namespace extensions
