// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_pwc_permission_delegate.h"

#include <string_view>

#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

namespace glic {

namespace {

using blink::mojom::PermissionStatus;
using content::PermissionResult;

PermissionResult PermissionFromPref(PrefService* prefs,
                                    std::string_view pref_name) {
  if (!prefs) {
    return PermissionResult(PermissionStatus::DENIED);
  }
  return PermissionResult(prefs->GetBoolean(pref_name)
                              ? PermissionStatus::GRANTED
                              : PermissionStatus::DENIED);
}

}  // namespace

GlicPwcPermissionDelegate::GlicPwcPermissionDelegate(Profile* profile)
    : profile_(profile) {}

GlicPwcPermissionDelegate::~GlicPwcPermissionDelegate() = default;

std::optional<content::PermissionResult>
GlicPwcPermissionDelegate::GetPermissionStatus(
    content::RenderFrameHost* /*render_frame_host*/,
    ContentSettingsType type) {
  PrefService* prefs = profile_ ? profile_->GetPrefs() : nullptr;

  switch (type) {
    case ContentSettingsType::MEDIASTREAM_MIC:
      return PermissionFromPref(prefs, prefs::kGlicMicrophoneEnabled);

    case ContentSettingsType::GEOLOCATION:
    case ContentSettingsType::GEOLOCATION_WITH_OPTIONS:
      return PermissionFromPref(prefs, prefs::kGlicGeolocationEnabled);

    case ContentSettingsType::CLIPBOARD_READ_WRITE:
    case ContentSettingsType::CLIPBOARD_SANITIZED_WRITE:
      return PermissionResult(PermissionStatus::GRANTED);

    default:
      return std::nullopt;
  }
}

}  // namespace glic
