// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GCM_GCM_PRODUCT_UTIL_H_
#define CHROME_BROWSER_GCM_GCM_PRODUCT_UTIL_H_

#include <string>

class PrefRegistrySimple;
class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace gcm {

// Returns a string like "com.chrome.macosx" that should be used as the GCM
// category when an app_id is sent as a subtype instead of as a category. This
// is generated once, then remains fixed forever (even if the product name
// changes), since it must match existing Instance ID tokens.
std::string GetProductCategoryForSubtypes(PrefService* prefs);

void RegisterPrefs(PrefRegistrySimple* registry);
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace gcm

#endif  // CHROME_BROWSER_GCM_GCM_PRODUCT_UTIL_H_
