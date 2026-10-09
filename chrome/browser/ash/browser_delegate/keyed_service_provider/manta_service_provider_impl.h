// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_MANTA_SERVICE_PROVIDER_IMPL_H_
#define CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_MANTA_SERVICE_PROVIDER_IMPL_H_

#include "chromeos/ash/components/manta/manta_service_provider.h"

namespace ash {

class MantaServiceProviderImpl : public MantaServiceProvider {
 public:
  MantaServiceProviderImpl();
  MantaServiceProviderImpl(const MantaServiceProviderImpl&) = delete;
  MantaServiceProviderImpl& operator=(const MantaServiceProviderImpl&) = delete;
  ~MantaServiceProviderImpl() override;

  // MantaServiceProvider:
  manta::MantaService* Find(const AccountId& account_id) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_MANTA_SERVICE_PROVIDER_IMPL_H_
