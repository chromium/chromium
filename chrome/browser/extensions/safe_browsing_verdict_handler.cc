// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/safe_browsing_verdict_handler.h"

#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/extensions/extension_service.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/blocklist_state.h"
#include "extensions/browser/extension_registry.h"

namespace extensions {

namespace {

// Partitions `before`, `after` and `unchanged` into `no_longer` and
// `not_yet`.
// `no_longer` = `before` - `after` - `unchanged`.
// `not_yet` = `after` - `before`.
void Partition(const ExtensionIdSet& before,
               const ExtensionIdSet& after,
               const ExtensionIdSet& unchanged,
               ExtensionIdSet* no_longer,
               ExtensionIdSet* not_yet) {
  *not_yet = base::STLSetDifference<ExtensionIdSet>(after, before);
  *no_longer = base::STLSetDifference<ExtensionIdSet>(before, after);
  *no_longer = base::STLSetDifference<ExtensionIdSet>(*no_longer, unchanged);
}

}  // namespace

SafeBrowsingVerdictHandler::SafeBrowsingVerdictHandler(
    ExtensionPrefs* extension_prefs,
    ExtensionRegistry* registry,
    ExtensionService* extension_service)
    : extension_prefs_(extension_prefs),
      registry_(registry),
      extension_service_(extension_service) {
  extension_registry_observation_.Observe(registry_.get());
}

SafeBrowsingVerdictHandler::~SafeBrowsingVerdictHandler() = default;

void SafeBrowsingVerdictHandler::Init() {
  TRACE_EVENT0("browser,startup", "SafeBrowsingVerdictHandler::Init");

  const ExtensionSet all_extensions =
      registry_->GenerateInstalledExtensionsSet();

  for (const auto& extension : all_extensions) {
    const BitMapBlocklistState state =
        blocklist_prefs::GetSafeBrowsingExtensionBlocklistState(
            extension->id(), extension_prefs_);
    if (state == BitMapBlocklistState::BLOCKLISTED_SECURITY_VULNERABILITY ||
        state == BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED ||
        state == BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION) {
      // If the extension was disabled in an older Chrome version, it is
      // possible that the acknowledged state is not set. Backfill the
      // acknowledged state if that's the case.
      blocklist_prefs::AddAcknowledgedBlocklistState(extension->id(), state,
                                                     extension_prefs_);
      greylist_.Insert(extension);
    } else if (state == BitMapBlocklistState::BLOCKLISTED_MALWARE) {
      blocklist_.Insert(extension);
    }
  }
}

void SafeBrowsingVerdictHandler::ManageBlocklist(
    const Blocklist::BlocklistStateMap& state_map) {
  ExtensionIdSet blocklist;
  ExtensionIdSet greylist;
  ExtensionIdSet unchanged;

  ExtensionIdSet installed_ids =
      registry_->GenerateInstalledExtensionsSet().GetIDs();
  for (const auto& it : state_map) {
    // It is possible that an extension is uninstalled when the blocklist is
    // fetching asynchronously. In this case, we should ignore this extension.
    if (!base::Contains(installed_ids, it.first)) {
      continue;
    }
    switch (it.second) {
      case NOT_BLOCKLISTED:
        break;
      case BLOCKLISTED_MALWARE:
        blocklist.insert(it.first);
        break;
      case BLOCKLISTED_SECURITY_VULNERABILITY:
      case BLOCKLISTED_CWS_POLICY_VIOLATION:
      case BLOCKLISTED_POTENTIALLY_UNWANTED:
        greylist.insert(it.first);
        break;
      case BLOCKLISTED_UNKNOWN:
        unchanged.insert(it.first);
        break;
    }
  }

  UpdateBlocklistedExtensions(blocklist, unchanged);
  UpdateGreylistedExtensions(greylist, unchanged, state_map);
}

void SafeBrowsingVerdictHandler::UpdateBlocklistedExtensions(
    const ExtensionIdSet& blocklist,
    const ExtensionIdSet& unchanged) {
  ExtensionIdSet not_yet_blocked, no_longer_blocked;
  Partition(blocklist_.GetIDs(), blocklist, unchanged, &no_longer_blocked,
            &not_yet_blocked);

  for (const auto& id : no_longer_blocked) {
    scoped_refptr<const Extension> extension = blocklist_.GetByID(id);
    DCHECK(extension.get())
        << "Extension " << id << " must be in the blocklist.";

    blocklist_.Remove(id);
    blocklist_prefs::SetSafeBrowsingExtensionBlocklistState(
        id, BitMapBlocklistState::NOT_BLOCKLISTED, extension_prefs_);
    extension_service_->OnBlocklistStateRemoved(id);
    UMA_HISTOGRAM_ENUMERATION("ExtensionBlacklist.UnblacklistInstalled",
                              extension->location());
  }

  for (const auto& id : not_yet_blocked) {
    scoped_refptr<const Extension> extension =
        registry_->GetInstalledExtension(id);
    DCHECK(extension.get()) << "Extension " << id << " needs to be "
                            << "blocklisted, but it's not installed.";

    blocklist_.Insert(extension);
    blocklist_prefs::SetSafeBrowsingExtensionBlocklistState(
        id, BitMapBlocklistState::BLOCKLISTED_MALWARE, extension_prefs_);
    extension_service_->OnBlocklistStateAdded(id);
    UMA_HISTOGRAM_ENUMERATION("ExtensionBlacklist.BlacklistInstalled",
                              extension->location());
  }
}

void SafeBrowsingVerdictHandler::UpdateGreylistedExtensions(
    const ExtensionIdSet& greylist,
    const ExtensionIdSet& unchanged,
    const Blocklist::BlocklistStateMap& state_map) {
  ExtensionIdSet not_yet_greylisted;
  ExtensionIdSet no_longer_greylisted;
  Partition(greylist_.GetIDs(), greylist, unchanged, &no_longer_greylisted,
            &not_yet_greylisted);

  for (const auto& id : no_longer_greylisted) {
    scoped_refptr<const Extension> extension = greylist_.GetByID(id);
    DCHECK(extension.get()) << "Extension " << id << " no longer greylisted, "
                            << "but it was not marked as greylisted.";

    greylist_.Remove(id);
    blocklist_prefs::SetSafeBrowsingExtensionBlocklistState(
        extension->id(), BitMapBlocklistState::NOT_BLOCKLISTED,
        extension_prefs_);
    extension_service_->OnGreylistStateRemoved(extension->id());
    UMA_HISTOGRAM_ENUMERATION("Extensions.Greylist.Enabled",
                              extension->location());
  }

  // Iterate over `greylist` instead of `not_yet_greylisted`, because the
  // extension needs to be disabled again if it is switched to another greylist
  // state.
  for (const auto& id : greylist) {
    scoped_refptr<const Extension> extension =
        registry_->GetInstalledExtension(id);
    DCHECK(extension.get()) << "Extension " << id << " needs to be "
                            << "disabled, but it's not installed.";

    greylist_.Insert(extension);
    BlocklistState greylist_state = state_map.find(id)->second;
    BitMapBlocklistState bitmap_greylist_state =
        blocklist_prefs::BlocklistStateToBitMapBlocklistState(greylist_state);
    blocklist_prefs::SetSafeBrowsingExtensionBlocklistState(
        extension->id(), bitmap_greylist_state, extension_prefs_);
    extension_service_->OnGreylistStateAdded(id, bitmap_greylist_state);
    UMA_HISTOGRAM_ENUMERATION("Extensions.Greylist.Disabled",
                              extension->location());
  }
}

void SafeBrowsingVerdictHandler::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UninstallReason reason) {
  blocklist_.Remove(extension->id());
  greylist_.Remove(extension->id());
}

}  // namespace extensions
