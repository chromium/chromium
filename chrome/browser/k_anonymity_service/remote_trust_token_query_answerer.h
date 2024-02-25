// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_K_ANONYMITY_SERVICE_REMOTE_TRUST_TOKEN_QUERY_ANSWERER_H_
#define CHROME_BROWSER_K_ANONYMITY_SERVICE_REMOTE_TRUST_TOKEN_QUERY_ANSWERER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"

// This class acts as a proxy to the TrustTokenQueryAnswerer in the network
// service. When something happens to the mojom connection to the
// TrustTokenQueryAnswerer it sets up a new connection and uses that for further
// queries. When a disconnection occurs the callbacks for any outstanding
// requests will be called with a `nullptr` argument.
//
// Callers should ensure that TrustTokenQueryAnswerer calls only occur one at a
// time. This is, after calling HasTrustTokens the caller should wait until the
// corresponding callback has fired before invoking HasTrustTokens again, and
// similarly for HasRedemptionRecord.
//
// The k-anonymity service code is the only user of this utility class, so it
// can share the directory with them. That may change if there are other users.
class RemoteTrustTokenQueryAnswerer
    : public network::mojom::TrustTokenQueryAnswerer {
 public:
  RemoteTrustTokenQueryAnswerer(url::Origin top_frame_origin, Profile* profile);

  ~RemoteTrustTokenQueryAnswerer() override;

  // Implementation of network::mojom::TrustTokenQueryAnswerer
  void HasTrustTokens(const url::Origin& issuer,
                      HasTrustTokensCallback callback) override;
  void HasRedemptionRecord(const url::Origin& issuer,
                           HasRedemptionRecordCallback callback) override;

  void FlushForTesting() { cached_answerer_.FlushForTesting(); }

 private:
  // Callback called by OnHasTrustTokens
  void OnHasTrustTokensCompleted(
      network::mojom::HasTrustTokensResultPtr result);
  void OnHasRedemptionRecordCompleted(
      network::mojom::HasRedemptionRecordResultPtr result);

  // Create a new connection to the TrustTokenQueryAnswerer and update the
  // cache to use it instead.
  void UpdateCachedAnswerer();

  // Handle a mojo disconnection. Retries current requests.
  void OnDisconnect();

  std::optional<HasTrustTokensCallback> pending_has_trust_tokens_request_;
  std::optional<HasRedemptionRecordCallback>
      pending_has_redemption_record_request_;

  const url::Origin top_frame_origin_;
  const raw_ptr<Profile> profile_;
  mojo::Remote<network::mojom::TrustTokenQueryAnswerer> cached_answerer_;
  base::WeakPtrFactory<RemoteTrustTokenQueryAnswerer> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_K_ANONYMITY_SERVICE_REMOTE_TRUST_TOKEN_QUERY_ANSWERER_H_
