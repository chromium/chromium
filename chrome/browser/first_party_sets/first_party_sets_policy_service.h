// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_POLICY_SERVICE_H_
#define CHROME_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_POLICY_SERVICE_H_

#include "base/sequence_checker.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/network/public/mojom/first_party_sets_access_delegate.mojom.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace first_party_sets {

// A profile keyed service for storing Remote FirstPartySetsAccessDelegates
// which must await the initialization of the browsers list of First-Party Sets.
//
// This service only exists for a BrowserContext if First-Party Sets is enabled
// globally by the base::Feature and for that BrowserContext by enterprise
// policy.
class FirstPartySetsPolicyService : public KeyedService {
 public:
  using PolicyCustomization =
      content::FirstPartySetsHandler::PolicyCustomization;

  FirstPartySetsPolicyService(content::BrowserContext* context,
                              const base::Value::Dict& policy);
  FirstPartySetsPolicyService(const FirstPartySetsPolicyService&) = delete;
  FirstPartySetsPolicyService& operator=(const FirstPartySetsPolicyService&) =
      delete;
  ~FirstPartySetsPolicyService() override;

  void AddRemoteAccessDelegate(
      mojo::Remote<network::mojom::FirstPartySetsAccessDelegate>
          access_delegate);

  // KeyedService:
  void Shutdown() override;

 private:
  // Triggers changes that occur once the customizations are ready for the
  // profile that created this service.
  void OnCustomizationsReady(PolicyCustomization customizations);

  // The remote delegates associated with the profile that created this
  // service.
  mojo::RemoteSet<network::mojom::FirstPartySetsAccessDelegate>
      access_delegates_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The FirstPartySetsOverrides enterprise policy value for the profile
  // that created this service.
  base::Value::Dict policy_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The customizations to the browser's list of First-Party Sets to respect
  // the changes specified by this FirstPartySetsOverrides policy for the
  // profile that created this service.
  absl::optional<PolicyCustomization> customizations_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<FirstPartySetsPolicyService> weak_factory_{this};
};

}  // namespace first_party_sets

#endif  // CHROME_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_POLICY_SERVICE_H_
