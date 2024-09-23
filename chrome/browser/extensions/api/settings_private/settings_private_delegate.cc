// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/settings_private/settings_private_delegate.h"

#include <memory>
#include <utility>

#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/settings_private/prefs_util.h"
#include "chrome/browser/extensions/api/settings_private/prefs_util_enums.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "url/gurl.h"

namespace extensions {

SettingsPrivateDelegate::SettingsPrivateDelegate(Profile* profile)
    : profile_(profile) {
  prefs_util_ = std::make_unique<PrefsUtil>(profile);
}

SettingsPrivateDelegate::~SettingsPrivateDelegate() {
}

std::optional<base::Value::Dict> SettingsPrivateDelegate::GetPref(
    const std::string& name) {
  std::optional<api::settings_private::PrefObject> pref =
      prefs_util_->GetPref(name);
  if (!pref)
    return std::nullopt;
  return pref->ToValue();
}

base::Value::List SettingsPrivateDelegate::GetAllPrefs() {
  base::Value::List prefs;

  const TypedPrefMap& keys = prefs_util_->GetAllowlistedKeys();
  for (const auto& it : keys) {
    if (std::optional<base::Value::Dict> pref = GetPref(it.first); pref) {
      prefs.Append(std::move(*pref));
    }
  }

  return prefs;
}

settings_private::SetPrefResult SettingsPrivateDelegate::SetPref(
    const std::string& pref_name,
    const base::Value* value) {
  return prefs_util_->SetPref(pref_name, value);
}

base::Value SettingsPrivateDelegate::GetDefaultZoom() {
  // Zoom level prefs aren't available for off-the-record profiles (like guest
  // mode on Chrome OS). The setting isn't visible to users anyway, so return a
  // default value.
  if (profile_->IsOffTheRecord())
    return base::Value(0.0);
  double zoom = blink::ZoomLevelToZoomFactor(
      profile_->GetZoomLevelPrefs()->GetDefaultZoomLevelPref());
  return base::Value(zoom);
}

settings_private::SetPrefResult SettingsPrivateDelegate::SetDefaultZoom(
    double zoom) {
  // See comment in GetDefaultZoom().
  if (profile_->IsOffTheRecord())
    return settings_private::SetPrefResult::PREF_NOT_MODIFIABLE;
  double zoom_factor = blink::ZoomFactorToZoomLevel(zoom);
  profile_->GetZoomLevelPrefs()->SetDefaultZoomLevelPref(zoom_factor);
  return settings_private::SetPrefResult::SUCCESS;
}

}  // namespace extensions
