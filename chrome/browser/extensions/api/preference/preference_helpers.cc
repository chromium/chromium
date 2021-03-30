// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/preference/preference_helpers.h"

#include <memory>
#include <utility>

#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/preference/preference_api.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions {
namespace preference_helpers {

namespace {

const char kIncognitoPersistent[] = "incognito_persistent";
const char kIncognitoSessionOnly[] = "incognito_session_only";
const char kRegular[] = "regular";
const char kRegularOnly[] = "regular_only";

const char kLevelOfControlKey[] = "levelOfControl";

const char kNotControllable[] = "not_controllable";
const char kControlledByOtherExtensions[] = "controlled_by_other_extensions";
const char kControllableByThisExtension[] = "controllable_by_this_extension";
const char kControlledByThisExtension[] = "controlled_by_this_extension";

}  // namespace

bool StringToScope(const std::string& s,
                   ExtensionPrefsScope* scope) {
  if (s == kRegular)
    *scope = kExtensionPrefsScopeRegular;
  else if (s == kRegularOnly)
    *scope = kExtensionPrefsScopeRegularOnly;
  else if (s == kIncognitoPersistent)
    *scope = kExtensionPrefsScopeIncognitoPersistent;
  else if (s == kIncognitoSessionOnly)
    *scope = kExtensionPrefsScopeIncognitoSessionOnly;
  else
    return false;
  return true;
}

const char* GetLevelOfControl(
    Profile* profile,
    const std::string& extension_id,
    const std::string& browser_pref,
    bool incognito) {
  PrefService* prefs = incognito ? profile->GetOffTheRecordPrefs()
                                 : profile->GetPrefs();
  bool from_incognito = false;
  bool* from_incognito_ptr = incognito ? &from_incognito : nullptr;
  const PrefService::Preference* pref = prefs->FindPreference(browser_pref);
  if (!pref->IsExtensionModifiable())
    return kNotControllable;

  if (PreferenceAPI::Get(profile)->DoesExtensionControlPref(
          extension_id,
          browser_pref,
          from_incognito_ptr)) {
    return kControlledByThisExtension;
  }

  if (PreferenceAPI::Get(profile)->CanExtensionControlPref(extension_id,
                                                           browser_pref,
                                                           incognito)) {
    return kControllableByThisExtension;
  }

  return kControlledByOtherExtensions;
}

void DispatchEventToExtensions(Profile* profile,
                               events::HistogramValue histogram_value,
                               const std::string& event_name,
                               base::ListValue* args,
                               mojom::APIPermissionID permission,
                               bool incognito,
                               const std::string& browser_pref) {
  EventRouter* router = EventRouter::Get(profile);
  if (!router || !router->HasEventListener(event_name))
    return;

  for (const scoped_refptr<const extensions::Extension>& extension :
       ExtensionRegistry::Get(profile)->enabled_extensions()) {
    // TODO(bauerb): Only iterate over registered event listeners.
    if (router->ExtensionHasEventListener(extension->id(), event_name) &&
        extension->permissions_data()->HasAPIPermission(permission) &&
        (!incognito || util::IsIncognitoEnabled(extension->id(), profile))) {
      // Inject level of control key-value.
      base::DictionaryValue* dict;
      bool rv = args->GetDictionary(0, &dict);
      DCHECK(rv);
      std::string level_of_control =
          GetLevelOfControl(profile, extension->id(), browser_pref, incognito);
      dict->SetString(kLevelOfControlKey, level_of_control);

      // If the extension is in incognito split mode,
      // a) incognito pref changes are visible only to the incognito tabs
      // b) regular pref changes are visible only to the incognito tabs if the
      //    incognito pref has not already been set
      Profile* restrict_to_profile = nullptr;
      if (IncognitoInfo::IsSplitMode(extension.get())) {
        if (incognito) {  // Handle case a).
          // If off the record profile does not exist, there should be no
          // extensions running in incognito at this time, and consequentially
          // no need to dispatch an event restricted to an incognito extension.
          // Furthermore, avoid calling GetPrimaryOTRProfile() in this case -
          // this method creates off the record profile if one does not exist.
          // Unnecessarily creating off the record profile is undesirable, and
          // can lead to a crash if incognito is disallowed for the current
          // profile (see https://crbug.com/796814).
          if (!profile->HasPrimaryOTRProfile())
            continue;
          restrict_to_profile = profile->GetPrimaryOTRProfile();
        } else {  // Handle case b).
          bool controlled_from_incognito = false;
          bool controlled_by_extension =
              PreferenceAPI::Get(profile)->DoesExtensionControlPref(
                  extension->id(), browser_pref, &controlled_from_incognito);
          if (controlled_by_extension && controlled_from_incognito)
            restrict_to_profile = profile;
        }
      }

      std::unique_ptr<base::ListValue> args_copy(args->DeepCopy());
      auto event =
          std::make_unique<Event>(histogram_value, event_name,
                                  std::move(args_copy), restrict_to_profile);
      router->DispatchEventToExtension(extension->id(), std::move(event));
    }
  }
}

}  // namespace preference_helpers
}  // namespace extensions
