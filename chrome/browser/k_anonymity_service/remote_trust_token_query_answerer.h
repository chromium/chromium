// Copyright 2022 The Chromium Authors. All rights reserved.
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
// queries.
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

 private:
  // Create a new connection to the TrustTokenQueryAnswerer and update the
  // cache to use it instead.
  void UpdateCachedAnswerer();

  const url::Origin top_frame_origin_;
  const base::raw_ptr<Profile> profile_;
  mojo::Remote<network::mojom::TrustTokenQueryAnswerer> cached_answerer_;
};

#endif  // CHROME_BROWSER_K_ANONYMITY_SERVICE_REMOTE_TRUST_TOKEN_QUERY_ANSWERER_H_
