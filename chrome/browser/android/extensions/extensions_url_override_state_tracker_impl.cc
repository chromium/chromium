// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/extensions/extensions_url_override_state_tracker_impl.h"

#include <memory>

#include "base/notreached.h"
#include "chrome/browser/android/extensions/extensions_url_override_state_tracker.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/incognito_info.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

using IncognitoStatusToOverrideCount =
    ExtensionUrlOverrideStateTrackerImpl::IncognitoStatusToOverrideCount;

// RegistrarSynchronizer Implementation.
ExtensionUrlOverrideStateTrackerImpl::RegistrarSynchronizer::
    RegistrarSynchronizer(Profile* profile,
                          ExtensionUrlOverrideStateTrackerImpl* state_tracker)
    : state_tracker_(state_tracker) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile);
  for (auto extension : registry->enabled_extensions()) {
    OnExtensionOverrideAdded(*extension);
  }
}

void ExtensionUrlOverrideStateTrackerImpl::RegistrarSynchronizer::
    OnExtensionOverrideAdded(const Extension& extension) {
  const URLOverrides::URLOverrideMap& overrides =
      URLOverrides::GetChromeURLOverrides(&extension);
  state_tracker_->OnUrlOverrideRegistered(extension, overrides);
}

void ExtensionUrlOverrideStateTrackerImpl::RegistrarSynchronizer::
    OnExtensionOverrideRemoved(const Extension& extension) {
  const URLOverrides::URLOverrideMap& overrides =
      URLOverrides::GetChromeURLOverrides(&extension);
  state_tracker_->OnUrlOverrideDeactivated(extension, overrides);
}

// ExtensionUrlOverrideStateTrackerImpl Implementation.
ExtensionUrlOverrideStateTrackerImpl::ExtensionUrlOverrideStateTrackerImpl(
    Profile* profile,
    StateListener* listener)
    : listener_(listener), profile_(profile) {
  registrar_ =
      ExtensionWebUIOverrideRegistrar::GetFactoryInstance()->Get(profile_);
  synchronizer_ = std::make_unique<RegistrarSynchronizer>(profile, this);
  registrar_->AddObserver(synchronizer_.get());
}

ExtensionUrlOverrideStateTrackerImpl::~ExtensionUrlOverrideStateTrackerImpl() {
  registrar_->RemoveObserver(synchronizer_.get());
}

void ExtensionUrlOverrideStateTrackerImpl::OnUrlOverrideRegistered(
    const Extension& extension,
    const URLOverrides::URLOverrideMap& overrides) {
  GetAndCacheIncognitoStatus(extension);
  UpdateOverrides(extension, overrides, 1);
}
void ExtensionUrlOverrideStateTrackerImpl::OnUrlOverrideDeactivated(
    const Extension& extension,
    const URLOverrides::URLOverrideMap& overrides) {
  UpdateOverrides(extension, overrides, -1);
}

void ExtensionUrlOverrideStateTrackerImpl::UpdateOverrides(
    const Extension& extension,
    const URLOverrides::URLOverrideMap& overrides,
    int overrides_delta) {
  EnsureOverridesInitialized(overrides);
  DCHECK(extension_id_to_incognito_status_.count(extension.id()));
  bool incognito_override_allowed =
      extension_id_to_incognito_status_[extension.id()];
  for (const auto& page_override_pair : overrides) {
    const std::string& chrome_url_path = page_override_pair.first;
    auto override_it = override_map_.find(chrome_url_path);
    if (override_it == override_map_.end()) {
      NOTREACHED() << "Override map missing entry for initialized override";
    }

    IncognitoStatusToOverrideCount& counts = override_it->second;
    int prev_regular_overrides_count = counts[false];
    int prev_incognito_overrides_count = counts[true];
    counts[false] += overrides_delta;

    if (incognito_override_allowed) {
      counts[true] += overrides_delta;
    }

    int new_regular_overrides_count = counts[false];
    int new_incognito_overrides_count = counts[true];
    DCHECK_GE(new_regular_overrides_count, 0);
    DCHECK_GE(new_incognito_overrides_count, 0);

    bool was_enabled = prev_regular_overrides_count > 0;
    bool is_enabled = new_regular_overrides_count > 0;
    if (!was_enabled && is_enabled) {
      listener_->OnUrlOverrideEnabled(chrome_url_path,
                                      new_incognito_overrides_count > 0);
    } else if (was_enabled && !is_enabled) {
      DCHECK_EQ(new_incognito_overrides_count, 0);
      listener_->OnUrlOverrideDisabled(chrome_url_path);
    } else if (is_enabled) {
      bool was_incognito_enabled = prev_incognito_overrides_count > 0;
      bool is_incognito_enabled = new_incognito_overrides_count > 0;
      if (was_incognito_enabled != is_incognito_enabled) {
        listener_->OnUrlOverrideEnabled(chrome_url_path, is_incognito_enabled);
      }
    }
  }
}

void ExtensionUrlOverrideStateTrackerImpl::EnsureOverridesInitialized(
    const URLOverrides::URLOverrideMap& overrides) {
  for (const auto& page_override_pair : overrides) {
    const std::string& chrome_url_path = page_override_pair.first;
    auto it = override_map_.try_emplace(chrome_url_path).first;
    IncognitoStatusToOverrideCount& counts = it->second;
    counts.try_emplace(true, 0);
    counts.try_emplace(false, 0);
  }
}

bool ExtensionUrlOverrideStateTrackerImpl::GetAndCacheIncognitoStatus(
    const Extension& extension) {
  bool incognito_override_allowed =
      IncognitoInfo::IsSplitMode(&extension) &&
      util::IsIncognitoEnabled(extension.id(), profile_);
  extension_id_to_incognito_status_[extension.id()] =
      incognito_override_allowed;
  return incognito_override_allowed;
}

}  // namespace extensions
