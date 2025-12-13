// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/initial_external_extension_loader.h"

#include <string>

#include "base/strings/strcat.h"
#include "base/types/to_address.h"
#include "base/values.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension_urls.h"

namespace extensions {

namespace {

std::string MakePrefName(const std::string& extension_id,
                         const std::string& pref_name) {
  return base::StrCat({extension_id, ".", pref_name});
}

// Generates a dictionary of preferences for the given extension IDs.
// For each valid extension ID in `extensions_ids`, its creates
// preferences to set the update URL to the Chrome Web Store and to mark the
// extension as trusted (i.e., not untrusted).
base::Value::Dict GenerateExtensionPrefs(
    const base::Value::List& extensions_ids) {
  base::Value::Dict prefs;
  const std::string web_store_update_url =
      extension_urls::GetWebstoreUpdateUrl().spec();
  for (const auto& extension_id : extensions_ids) {
    const std::string* id = extension_id.GetIfString();
    if (!id || !crx_file::id_util::IdIsValid(*id)) {
      continue;
    }

    prefs.SetByDottedPath(
        MakePrefName(*id, ExternalProviderImpl::kExternalUpdateUrl),
        web_store_update_url);
    prefs.SetByDottedPath(
        MakePrefName(*id, ExternalProviderImpl::kMayBeUntrusted), false);
  }
  return prefs;
}

}  // namespace

InitialExternalExtensionLoader::InitialExternalExtensionLoader(
    PrefService& prefs)
    : prefs_(prefs) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  pref_registrar_.Init(base::to_address(prefs_));

  // base::Unretained() is safe because `pref_registrar_` is owned by `this`.
  //
  // `pref_names::kInitialInstallList` may not be populated at this time.
  // Therefore, future changes should be monitored so that the extension ids
  // will be taken into consideration.
  pref_registrar_.Add(
      pref_names::kInitialInstallList,
      base::BindRepeating(
          &InitialExternalExtensionLoader::OnExtensionsPrefChanged,
          base::Unretained(this)));
}

InitialExternalExtensionLoader::~InitialExternalExtensionLoader() = default;

void InitialExternalExtensionLoader::StartLoading() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::Value::List& ids =
      prefs_->GetList(pref_names::kInitialInstallList);
  LoadFinished(GenerateExtensionPrefs(ids));
}

void InitialExternalExtensionLoader::OnExtensionsPrefChanged() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::Value::List& ids =
      prefs_->GetList(pref_names::kInitialInstallList);
  OnUpdated(GenerateExtensionPrefs(ids));
}

}  // namespace extensions
