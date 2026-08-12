// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/browser_delegate/keyed_service_provider/template_url_service_provider_impl.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/account_id/account_id.h"
#include "content/public/browser/browser_context.h"

namespace ash {

TemplateURLServiceProviderImpl::TemplateURLServiceProviderImpl() = default;

TemplateURLServiceProviderImpl::~TemplateURLServiceProviderImpl() = default;

TemplateURLService* TemplateURLServiceProviderImpl::Find(
    const AccountId& account_id) {
  content::BrowserContext* context =
      BrowserContextHelper::Get()->GetBrowserContextByAccountId(account_id);
  return TemplateURLServiceFactory::GetForProfile(
      Profile::FromBrowserContext(context));
}

}  // namespace ash
