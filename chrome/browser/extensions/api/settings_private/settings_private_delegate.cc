// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/settings_private/settings_private_delegate.h"

#include <utility>

#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
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
  prefs_util_.reset(new PrefsUtil(profile));
}

SettingsPrivateDelegate::~SettingsPrivateDelegate() {
}

std::unique_ptr<base::Value> SettingsPrivateDelegate::GetPref(
    const std::string& name) {
  std::unique_ptr<api::settings_private::PrefObject> pref =
      prefs_util_->GetPref(name);
  if (!pref)
    return std::make_unique<base::Value>();
  return pref->ToValue();
}

std::unique_ptr<base::Value> SettingsPrivateDelegate::GetAllPrefs() {
  std::unique_ptr<base::ListValue> prefs(new base::ListValue());

  const TypedPrefMap& keys = prefs_util_->GetWhitelistedKeys();
  for (const auto& it : keys) {
    std::unique_ptr<base::Value> pref = GetPref(it.first);
    if (!pref->is_none())
      prefs->Append(std::move(pref));
  }

  return std::move(prefs);
}

settings_private::SetPrefResult SettingsPrivateDelegate::SetPref(
    const std::string& pref_name,
    const base::Value* value) {
  return prefs_util_->SetPref(pref_name, value);
}

std::unique_ptr<base::Value> SettingsPrivateDelegate::GetDefaultZoom() {
  // Zoom level prefs aren't available for off-the-record profiles (like guest
  // mode on Chrome OS). The setting isn't visible to users anyway, so return a
  // default value.
  if (profile_->IsOffTheRecord())
    return std::make_unique<base::Value>(0.0);
  double zoom = blink::PageZoomLevelToZoomFactor(
      profile_->GetZoomLevelPrefs()->GetDefaultZoomLevelPref());
  return std::make_unique<base::Value>(zoom);
}

settings_private::SetPrefResult SettingsPrivateDelegate::SetDefaultZoom(
    double zoom) {
  // See comment in GetDefaultZoom().
  if (profile_->IsOffTheRecord())
    return settings_private::SetPrefResult::PREF_NOT_MODIFIABLE;
  double zoom_factor = blink::PageZoomFactorToZoomLevel(zoom);
  profile_->GetZoomLevelPrefs()->SetDefaultZoomLevelPref(zoom_factor);
  return settings_private::SetPrefResult::SUCCESS;
}

}  // namespace extensions
