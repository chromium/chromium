// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SITE_TOKEN_PROVIDER_FAKE_TRUSTED_URL_LOADER_HEADER_CLIENT_H_
#define CHROME_BROWSER_SITE_TOKEN_PROVIDER_FAKE_TRUSTED_URL_LOADER_HEADER_CLIENT_H_

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace network {
struct ResourceRequest;
}

namespace site_token_provider {

// Header that the fake sets on every request it observes, so that a test can
// verify that the request actually reached it.
inline constexpr char kFakeHeaderClientHeaderName[] = "X-Fake-Header-Client";
inline constexpr char kFakeHeaderClientHeaderValue[] = "observed-by-fake";

// A behavioral fake TrustedURLLoaderHeaderClient for browser tests. Every
// request it observes is counted and has kFakeHeaderClientHeaderName set to
// kFakeHeaderClientHeaderValue, so that a test can observe on the wire whether
// the request reached this client.
class FakeTrustedURLLoaderHeaderClient
    : public network::mojom::TrustedURLLoaderHeaderClient {
 public:
  FakeTrustedURLLoaderHeaderClient();
  ~FakeTrustedURLLoaderHeaderClient() override;

  FakeTrustedURLLoaderHeaderClient(const FakeTrustedURLLoaderHeaderClient&) =
      delete;
  FakeTrustedURLLoaderHeaderClient& operator=(
      const FakeTrustedURLLoaderHeaderClient&) = delete;

  mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
  AddReceiver();

  void OnLoaderCreated(
      int32_t request_id,
      mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver)
      override;

  void OnLoaderForCorsPreflightCreated(
      const network::ResourceRequest& request,
      mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver)
      override;

  int observed_request_count() const;

 private:
  void RecordObservedRequest();

  SEQUENCE_CHECKER(sequence_checker_);
  int observed_request_count_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  mojo::ReceiverSet<network::mojom::TrustedURLLoaderHeaderClient> receivers_;

  // The per-request clients handed out by OnLoaderCreated are owned by their
  // mojo pipe, not by this object, so they can outlive it while a request is
  // still in flight. They call back through a weak pointer so that a late
  // request is dropped rather than touching freed memory.
  base::WeakPtrFactory<FakeTrustedURLLoaderHeaderClient> weak_factory_{this};
};

}  // namespace site_token_provider

#endif  // CHROME_BROWSER_SITE_TOKEN_PROVIDER_FAKE_TRUSTED_URL_LOADER_HEADER_CLIENT_H_
