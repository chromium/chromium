// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/test_blacklist_state_fetcher.h"

#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/safe_browsing/db/v4_test_util.h"
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
      int32_t routing_id,
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
    NOTREACHED();
  }

  // network::SharedURLLoaderFactoryInfo implementation
  std::unique_ptr<network::SharedURLLoaderFactoryInfo> Clone() override {
    NOTREACHED();
    return nullptr;
  }

 private:
  friend class base::RefCounted<DummySharedURLLoaderFactory>;
  ~DummySharedURLLoaderFactory() override = default;

  std::vector<mojo::PendingRemote<network::mojom::URLLoaderClient>> clients_;
};

}  // namespace

TestBlacklistStateFetcher::TestBlacklistStateFetcher(
    BlacklistStateFetcher* fetcher) : fetcher_(fetcher) {
  fetcher_->SetSafeBrowsingConfig(safe_browsing::GetTestV4ProtocolConfig());

  url_loader_factory_ = base::MakeRefCounted<DummySharedURLLoaderFactory>();
  fetcher_->url_loader_factory_ = url_loader_factory_.get();
}

TestBlacklistStateFetcher::~TestBlacklistStateFetcher() {
}

void TestBlacklistStateFetcher::SetBlacklistVerdict(
    const std::string& id, ClientCRXListInfoResponse_Verdict state) {
  verdicts_[id] = state;
}

bool TestBlacklistStateFetcher::HandleFetcher(const std::string& id) {
  network::SimpleURLLoader* url_loader = nullptr;
  for (auto& it : fetcher_->requests_) {
    if (it.second.second == id) {
      url_loader = it.second.first.get();
      break;
    }
  }

  if (!url_loader)
    return false;

  ClientCRXListInfoResponse response;
  if (base::Contains(verdicts_, id))
    response.set_verdict(verdicts_[id]);
  else
    response.set_verdict(ClientCRXListInfoResponse::NOT_IN_BLACKLIST);

  std::string response_str;
  response.SerializeToString(&response_str);

  fetcher_->OnURLLoaderCompleteInternal(url_loader, response_str, 200, net::OK);

  return true;
}

}  // namespace extensions
