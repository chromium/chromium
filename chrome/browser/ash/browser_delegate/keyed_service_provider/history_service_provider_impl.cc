// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/browser_delegate/keyed_service_provider/history_service_provider_impl.h"

#include "base/check.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/account_id/account_id.h"
#include "components/keyed_service/core/service_access_type.h"
#include "content/public/browser/browser_context.h"

namespace ash {

HistoryServiceProviderImpl::HistoryServiceProviderImpl() = default;

HistoryServiceProviderImpl::~HistoryServiceProviderImpl() = default;

history::HistoryService* HistoryServiceProviderImpl::Find(
    const AccountId& account_id) {
  content::BrowserContext* context =
      BrowserContextHelper::Get()->GetBrowserContextByAccountId(account_id);
  // Callers resolve a live profile for `account_id` before calling Find(), so a
  // browser context always exists here.
  CHECK(context);
  return HistoryServiceFactory::GetForProfile(
      Profile::FromBrowserContext(context), ServiceAccessType::EXPLICIT_ACCESS);
}

}  // namespace ash
