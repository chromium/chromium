// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/test_blocklist_state_fetcher.h"

#include "base/containers/contains.h"
#include "components/safe_browsing/core/browser/db/v4_test_util.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace extensions {
namespace {

class DummySharedURLLoaderFactory : public network::SharedURLLoaderFactory {
 public:
  DummySharedURLLoaderFactory() {}

  // network::URLLoaderFactory implementation:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    // Ensure the client pipe doesn't get closed to avoid SimpleURLLoader seeing
    // a connection error.
    clients_.push_back(std::move(client));
  }

  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override {
    NOTREACHED_IN_MIGRATION();
  }

  // network::PendingSharedURLLoaderFactory implementation
  std::unique_ptr<network::PendingSharedURLLoaderFactory> Clone() override {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }

 private:
  friend class base::RefCounted<DummySharedURLLoaderFactory>;
  ~DummySharedURLLoaderFactory() override = default;

  std::vector<mojo::PendingRemote<network::mojom::URLLoaderClient>> clients_;
};

}  // namespace

TestBlocklistStateFetcher::TestBlocklistStateFetcher(
    BlocklistStateFetcher* fetcher)
    : fetcher_(fetcher) {
  fetcher_->SetSafeBrowsingConfig(safe_browsing::GetTestV4ProtocolConfig());

  url_loader_factory_ = base::MakeRefCounted<DummySharedURLLoaderFactory>();
  fetcher_->url_loader_factory_ = url_loader_factory_.get();
}

TestBlocklistStateFetcher::~TestBlocklistStateFetcher() {}

void TestBlocklistStateFetcher::SetBlocklistVerdict(
    const std::string& id,
    ClientCRXListInfoResponse_Verdict state) {
  verdicts_[id] = state;
}

bool TestBlocklistStateFetcher::HandleFetcher(const std::string& id) {
  network::SimpleURLLoader* url_loader = nullptr;
  for (auto& it : fetcher_->requests_) {
    if (it.second.second == id) {
      url_loader = it.second.first.get();
      break;
    }
  }

  if (!url_loader) {
    return false;
  }

  ClientCRXListInfoResponse response;
  if (base::Contains(verdicts_, id)) {
    response.set_verdict(verdicts_[id]);
  } else {
    response.set_verdict(ClientCRXListInfoResponse::NOT_IN_BLOCKLIST);
  }

  std::string response_str;
  response.SerializeToString(&response_str);

  fetcher_->OnURLLoaderCompleteInternal(url_loader, response_str, 200, net::OK);

  return true;
}

}  // namespace extensions
