// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_service.h"

#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/dips/dips_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/cookie_settings.h"

DIPSService::DIPSService(content::BrowserContext* context)
    : browser_context_(context),
      cookie_settings_(CookieSettingsFactory::GetForProfile(
          Profile::FromBrowserContext(context))) {}

DIPSService::~DIPSService() = default;

/* static */
DIPSService* DIPSService::Get(content::BrowserContext* context) {
  return DIPSServiceFactory::GetForBrowserContext(context);
}

void DIPSService::Shutdown() {
  cookie_settings_.reset();
}

bool DIPSService::ShouldBlockThirdPartyCookies() const {
  return cookie_settings_->ShouldBlockThirdPartyCookies();
}
