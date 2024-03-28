// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gcm/gcm_product_util.h"

#include <string_view>

#include "base/strings/string_util.h"
#include "chrome/common/chrome_version.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"

namespace gcm {

namespace {

std::string ToLowerAlphaNum(std::string_view in) {
  std::string out;
  out.reserve(in.size());
  for (char ch : in) {
    if (base::IsAsciiAlpha(ch) || base::IsAsciiDigit(ch))
      out.push_back(base::ToLowerASCII(ch));
  }
  return out;
}

}  // namespace

std::string GetProductCategoryForSubtypes(PrefService* prefs) {
  std::string product_category_for_subtypes =
      prefs->GetString(prefs::kGCMProductCategoryForSubtypes);
  if (!product_category_for_subtypes.empty())
    return product_category_for_subtypes;

  std::string product = ToLowerAlphaNum(PRODUCT_SHORTNAME_STRING);
  std::string ns = product == "chromium" ? "org" : "com";
  std::string platform = ToLowerAlphaNum(version_info::GetOSType());
  product_category_for_subtypes = ns + '.' + product + '.' + platform;

  prefs->SetString(prefs::kGCMProductCategoryForSubtypes,
                   product_category_for_subtypes);
  return product_category_for_subtypes;
}

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kGCMProductCategoryForSubtypes,
                               std::string() /* default_value */);
}

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  RegisterPrefs(registry);
}

}  // namespace gcm
