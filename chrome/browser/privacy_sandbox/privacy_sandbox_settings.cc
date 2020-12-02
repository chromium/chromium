// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings.h"

#include "base/feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_prefs.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/prefs/pref_service.h"

PrivacySandboxSettings::PrivacySandboxSettings(
    content_settings::CookieSettings* cookie_settings,
    PrefService* pref_service)
    : cookie_settings_(cookie_settings), pref_service_(pref_service) {
  DCHECK(pref_service_);
  DCHECK(cookie_settings_);
}

bool PrivacySandboxSettings::IsFlocAllowed(
    const GURL& url,
    const GURL& site_for_cookies,
    const base::Optional<url::Origin>& top_frame_origin) const {
  // Simply respect cookie settings.
  // TODO(crbug.com/1152336): Respect privacy sandbox settings.
  return cookie_settings_->IsCookieAccessAllowed(url, site_for_cookies,
                                                 top_frame_origin);
}

base::Time PrivacySandboxSettings::FlocDataAccessibleSince() const {
  // Simply indicate that all history is available.
  // TODO(crbug.com/1152336): Respect clear on exit & storage deletion events.
  return base::Time();
}

bool PrivacySandboxSettings::IsConversionMeasurementAllowed(
    const url::Origin& top_frame_origin,
    const url::Origin& reporting_origin) const {
  // Simply respect the 3P cookie setting.
  // TODO(crbug.com/1152336): Respect privacy sandbox settings.
  return !cookie_settings_->ShouldBlockThirdPartyCookies();
}

bool PrivacySandboxSettings::ShouldSendConversionReport(
    const url::Origin& impression_origin,
    const url::Origin& conversion_origin,
    const url::Origin& reporting_origin) const {
  // Simply respect the 3P cookie setting.
  // TODO(crbug.com/1152336): Respect privacy sandbox settings.
  return !cookie_settings_->ShouldBlockThirdPartyCookies();
}
