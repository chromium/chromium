// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_CONSENT_AUDITOR_PROVIDER_IMPL_H_
#define CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_CONSENT_AUDITOR_PROVIDER_IMPL_H_

#include "chromeos/ash/components/consent_auditor/consent_auditor_provider.h"

namespace ash {

class ConsentAuditorProviderImpl : public ConsentAuditorProvider {
 public:
  ConsentAuditorProviderImpl();
  ConsentAuditorProviderImpl(const ConsentAuditorProviderImpl&) = delete;
  ConsentAuditorProviderImpl& operator=(const ConsentAuditorProviderImpl&) =
      delete;
  ~ConsentAuditorProviderImpl() override;

  // ConsentAuditorProvider:
  consent_auditor::ConsentAuditor* Find(const AccountId& account_id) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_CONSENT_AUDITOR_PROVIDER_IMPL_H_
