// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_PREFERENCE_PREFERENCE_HELPERS_H_
#define CHROME_BROWSER_EXTENSIONS_PREFERENCE_PREFERENCE_HELPERS_H_

#include <string>

#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/api_permission_id.mojom-shared.h"
#include "extensions/common/permissions/permission_set.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/prefs.mojom-shared.h"
#include "chromeos/crosapi/mojom/prefs.mojom.h"
#endif

class PrefService;
class Profile;

namespace extensions {
namespace preference_helpers {

// Returns a string constant (defined in the API) indicating the level of
// control this extension has over the specified preference.
const char* GetLevelOfControl(Profile* profile,
                              const ExtensionId& extension_id,
                              const std::string& browser_pref,
                              bool incognito);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// As GetLevelOfControl, but additionally considers the ash state of the pref.
// This is relevant when the underlying preference lives in ash. Note that when
// the ash state is kDefaultUnknown this is equivalent to GetLevelOfControl.
const char* GetLevelOfControlWithAshControlState(
    crosapi::mojom::PrefControlState control_state,
    Profile* profile,
    const ExtensionId& extension_id,
    const std::string& browser_pref,
    bool incognito);

// As DispatchEventToExtensions, but additionally considers the ash state of the
// pref.
void DispatchEventToExtensionsWithAshControlState(
    Profile* profile,
    events::HistogramValue histogram_value,
    const std::string& event_name,
    base::Value::List args,
    mojom::APIPermissionID permission,
    bool incognito,
    const std::string& browser_pref,
    crosapi::mojom::PrefControlState control_state);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

// Dispatches |event_name| to extensions listening to the event and holding
// |permission|. |args| is passed as arguments to the event listener.  A
// key-value for the level of control the extension has over |browser_pref| is
// injected into the first item of |args|, which must be of type
// dictionary.
void DispatchEventToExtensions(Profile* profile,
                               events::HistogramValue histogram_value,
                               const std::string& event_name,
                               base::Value::List args,
                               mojom::APIPermissionID permission,
                               bool incognito,
                               const std::string& browser_pref);

// Returns preferences service of the given profile. If |incognito| is true and
// |profile| has an Incognito profile, the preferenecs of the Incognito profile
// is returned and otherwise a read-only copy of |profile|'s preferences is
// given.
PrefService* GetProfilePrefService(Profile* profile, bool incognito);

}  // namespace preference_helpers
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_PREFERENCE_PREFERENCE_HELPERS_H_
