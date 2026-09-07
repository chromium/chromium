// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/browser_delegate/keyed_service_provider/favicon_service_provider_impl.h"

#include "base/check.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/account_id/account_id.h"
#include "components/keyed_service/core/service_access_type.h"
#include "content/public/browser/browser_context.h"

namespace ash {

FaviconServiceProviderImpl::FaviconServiceProviderImpl() = default;

FaviconServiceProviderImpl::~FaviconServiceProviderImpl() = default;

favicon::FaviconService* FaviconServiceProviderImpl::Find(
    const AccountId& account_id) {
  content::BrowserContext* context =
      BrowserContextHelper::Get()->GetBrowserContextByAccountId(account_id);
  // Callers resolve a live profile for `account_id` before calling Find(), so a
  // browser context always exists here.
  CHECK(context);
  return FaviconServiceFactory::GetForProfile(
      Profile::FromBrowserContext(context), ServiceAccessType::EXPLICIT_ACCESS);
}

}  // namespace ash
