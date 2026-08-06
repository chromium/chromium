// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/browser_delegate/keyed_service_provider/identity_manager_provider_impl.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"

namespace ash {

IdentityManagerProviderImpl::IdentityManagerProviderImpl() = default;
IdentityManagerProviderImpl::~IdentityManagerProviderImpl() = default;

signin::IdentityManager* IdentityManagerProviderImpl::Find(
    const AccountId& account_id) {
  return IdentityManagerFactory::GetForProfile(Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
          account_id)));
}

}  // namespace ash
