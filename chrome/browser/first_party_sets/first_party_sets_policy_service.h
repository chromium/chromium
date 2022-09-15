// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_POLICY_SERVICE_H_
#define CHROME_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_POLICY_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
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

  content::BrowserContext* browser_context() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return browser_context_;
  }

 private:
  // Triggers changes that occur once the customizations are ready for the
  // profile that created this service.
  void OnCustomizationsReady(net::FirstPartySetsContextConfig config);

  // Triggers changes that occur once the sets transition clearing is done for
  // the profile that created this service.
  void OnSiteDataCleared();

  // The remote delegates associated with the profile that created this
  // service.
  mojo::RemoteSet<network::mojom::FirstPartySetsAccessDelegate>
      access_delegates_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The BrowserContext with which this service is associated. Set to nullptr in
  // `Shutdown()`.
  raw_ptr<content::BrowserContext> browser_context_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The FirstPartySetsOverrides enterprise policy value for the profile
  // that created this service.
  base::Value::Dict policy_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The customizations to the browser's list of First-Party Sets to respect
  // the changes specified by this FirstPartySetsOverrides policy for the
  // profile that created this service.
  absl::optional<net::FirstPartySetsContextConfig> config_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<FirstPartySetsPolicyService> weak_factory_{this};
};

}  // namespace first_party_sets

#endif  // CHROME_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_POLICY_SERVICE_H_
