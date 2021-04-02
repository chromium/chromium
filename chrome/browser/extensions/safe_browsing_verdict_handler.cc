// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/safe_browsing_verdict_handler.h"

#include "chrome/browser/extensions/extension_service.h"
#include "extensions/browser/blocklist_state.h"
#include "extensions/browser/extension_registry.h"

namespace extensions {

SafeBrowsingVerdictHandler::SafeBrowsingVerdictHandler(
    ExtensionPrefs* extension_prefs,
    ExtensionRegistry* registry,
    ExtensionService* extension_service)
    : extension_prefs_(extension_prefs),
      registry_(registry),
      extension_service_(extension_service) {}

void SafeBrowsingVerdictHandler::Init() {
  TRACE_EVENT0("browser,startup", "SafeBrowsingVerdictHandler::Init");

  std::unique_ptr<ExtensionSet> all_extensions =
      registry_->GenerateInstalledExtensionsSet();

  for (const auto& extension : *all_extensions) {
    const BlocklistState state =
        extension_prefs_->GetExtensionBlocklistState(extension->id());
    if (state == BLOCKLISTED_SECURITY_VULNERABILITY ||
        state == BLOCKLISTED_POTENTIALLY_UNWANTED ||
        state == BLOCKLISTED_CWS_POLICY_VIOLATION)
      greylist_.Insert(extension);
  }
}

void SafeBrowsingVerdictHandler::ManageBlocklist(
    const Blocklist::BlocklistStateMap& state_map) {
  ExtensionIdSet greylist;
  ExtensionIdSet unchanged;

  for (const auto& it : state_map) {
    switch (it.second) {
      case NOT_BLOCKLISTED:
      case BLOCKLISTED_MALWARE:
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

  UpdateGreylistedExtensions(greylist, unchanged, state_map);
}

// static
void SafeBrowsingVerdictHandler::Partition(const ExtensionIdSet& before,
                                           const ExtensionIdSet& after,
                                           const ExtensionIdSet& unchanged,
                                           ExtensionIdSet* no_longer,
                                           ExtensionIdSet* not_yet) {
  *not_yet = base::STLSetDifference<ExtensionIdSet>(after, before);
  *no_longer = base::STLSetDifference<ExtensionIdSet>(before, after);
  *no_longer = base::STLSetDifference<ExtensionIdSet>(*no_longer, unchanged);
}

// TODO(oleg): UMA logging
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
    if (!extension.get()) {
      NOTREACHED() << "Extension " << id << " no longer greylisted, "
                   << "but it was not marked as greylisted.";
      continue;
    }

    greylist_.Remove(id);
    extension_prefs_->SetExtensionBlocklistState(extension->id(),
                                                 NOT_BLOCKLISTED);
    extension_service_->RemoveDisableReasonAndMaybeEnable(
        extension->id(), disable_reason::DISABLE_GREYLIST);
  }

  for (const auto& id : not_yet_greylisted) {
    scoped_refptr<const Extension> extension =
        registry_->GetInstalledExtension(id);
    if (!extension.get()) {
      NOTREACHED() << "Extension " << id << " needs to be "
                   << "disabled, but it's not installed.";
      continue;
    }

    greylist_.Insert(extension);
    extension_prefs_->SetExtensionBlocklistState(extension->id(),
                                                 state_map.find(id)->second);
    extension_service_->DisableExtension(id, disable_reason::DISABLE_GREYLIST);
  }
}

}  // namespace extensions
