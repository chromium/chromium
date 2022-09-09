// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_service.h"

#include "base/task/thread_pool.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/dips/dips_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/cookie_settings.h"

DIPSService::DIPSService(content::BrowserContext* context)
    : browser_context_(context),
      cookie_settings_(CookieSettingsFactory::GetForProfile(
          Profile::FromBrowserContext(context))),
      storage_(base::SequenceBound<DIPSStorage>(CreateTaskRunner())) {}

DIPSService::~DIPSService() = default;

/* static */
DIPSService* DIPSService::Get(content::BrowserContext* context) {
  return DIPSServiceFactory::GetForBrowserContext(context);
}

void DIPSService::Shutdown() {
  cookie_settings_.reset();
}

scoped_refptr<base::SequencedTaskRunner> DIPSService::CreateTaskRunner() {
  return base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::ThreadPolicy::PREFER_BACKGROUND});
}

bool DIPSService::ShouldBlockThirdPartyCookies() const {
  return cookie_settings_->ShouldBlockThirdPartyCookies();
}
