// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/shared_storage/shared_storage_private_api.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/shared_storage_private.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

// TODO(b/231890240): Once Terminal SWA runs in lacros rather than ash, we can
// migrate gnubbyd back to using chrome.storage.local and remove this private
// API.

namespace shared_api = extensions::api::shared_storage_private;

namespace extensions {
namespace shared_storage {
void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kSharedStorage);
}
}  // namespace shared_storage

SharedStoragePrivateGetFunction::SharedStoragePrivateGetFunction() = default;
SharedStoragePrivateGetFunction::~SharedStoragePrivateGetFunction() = default;

ExtensionFunction::ResponseAction SharedStoragePrivateGetFunction::Run() {
  PrefService* prefs =
      Profile::FromBrowserContext(browser_context())->GetPrefs();
  return RespondNow(
      WithArguments(prefs->GetValue(prefs::kSharedStorage).Clone()));
}

SharedStoragePrivateSetFunction::SharedStoragePrivateSetFunction() = default;
SharedStoragePrivateSetFunction::~SharedStoragePrivateSetFunction() = default;

ExtensionFunction::ResponseAction SharedStoragePrivateSetFunction::Run() {
  std::optional<shared_api::Set::Params> params =
      shared_api::Set::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  PrefService* prefs =
      Profile::FromBrowserContext(browser_context())->GetPrefs();
  ScopedDictPrefUpdate update(prefs, prefs::kSharedStorage);
  update->Merge(std::move(params->items.additional_properties));
  return RespondNow(NoArguments());
}

SharedStoragePrivateRemoveFunction::SharedStoragePrivateRemoveFunction() =
    default;
SharedStoragePrivateRemoveFunction::~SharedStoragePrivateRemoveFunction() =
    default;

ExtensionFunction::ResponseAction SharedStoragePrivateRemoveFunction::Run() {
  std::optional<shared_api::Remove::Params> params =
      shared_api::Remove::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  PrefService* prefs =
      Profile::FromBrowserContext(browser_context())->GetPrefs();
  ScopedDictPrefUpdate update(prefs, prefs::kSharedStorage);
  base::Value::Dict& items = update.Get();
  for (const auto& key : params->keys) {
    items.Remove(key);
  }
  return RespondNow(NoArguments());
}

}  // namespace extensions
