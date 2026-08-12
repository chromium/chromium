// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_TEMPLATE_URL_SERVICE_PROVIDER_IMPL_H_
#define CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_TEMPLATE_URL_SERVICE_PROVIDER_IMPL_H_

#include "chromeos/ash/components/search_engines/template_url_service_provider.h"

class AccountId;
class TemplateURLService;

namespace ash {

// //chrome-side implementation of TemplateURLServiceProvider. Wraps
// //chrome/browser/search_engines' Profile-keyed TemplateURLServiceFactory so
// ChromeOS callers can reach the service through the chromeos-side interface.
class TemplateURLServiceProviderImpl : public TemplateURLServiceProvider {
 public:
  TemplateURLServiceProviderImpl();
  TemplateURLServiceProviderImpl(const TemplateURLServiceProviderImpl&) =
      delete;
  TemplateURLServiceProviderImpl& operator=(
      const TemplateURLServiceProviderImpl&) = delete;
  ~TemplateURLServiceProviderImpl() override;

  // TemplateURLServiceProvider:
  TemplateURLService* Find(const AccountId& account_id) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_TEMPLATE_URL_SERVICE_PROVIDER_IMPL_H_
