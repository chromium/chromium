// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/preference/network_prediction_transformer.h"

#include "base/values.h"
#include "chrome/browser/preloading/preloading_prefs.h"

namespace extensions {

NetworkPredictionTransformer::NetworkPredictionTransformer() = default;
NetworkPredictionTransformer::~NetworkPredictionTransformer() = default;

std::optional<base::Value> NetworkPredictionTransformer::ExtensionToBrowserPref(
    const base::Value& extension_pref,
    std::string& error,
    bool& bad_message) {
  if (!extension_pref.is_bool()) {
    DCHECK(false) << "Preference not found.";
  } else if (extension_pref.GetBool()) {
    return base::Value(
        static_cast<int>(prefetch::NetworkPredictionOptions::kDefault));
  }
  return base::Value(
      static_cast<int>(prefetch::NetworkPredictionOptions::kDisabled));
}

std::optional<base::Value> NetworkPredictionTransformer::BrowserToExtensionPref(
    const base::Value& browser_pref,
    bool is_incognito_profile) {
  prefetch::NetworkPredictionOptions value =
      prefetch::NetworkPredictionOptions::kDefault;
  if (browser_pref.is_int()) {
    value =
        static_cast<prefetch::NetworkPredictionOptions>(browser_pref.GetInt());
  }
  return base::Value(value != prefetch::NetworkPredictionOptions::kDisabled);
}

}  // namespace extensions
