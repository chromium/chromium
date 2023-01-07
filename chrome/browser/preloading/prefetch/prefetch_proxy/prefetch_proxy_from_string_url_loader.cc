// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_from_string_url_loader.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/io_buffer.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

PrefetchProxyFromStringURLLoader::PrefetchProxyFromStringURLLoader(
    std::unique_ptr<PrefetchedMainframeResponseContainer> response,
    const network::ResourceRequest& tentative_resource_request)
    : head_(response->TakeHead()),
      body_buffer_(
          base::MakeRefCounted<net::StringIOBuffer>(response->TakeBody())),
      bytes_of_raw_data_to_transfer_(body_buffer_->size()) {}

PrefetchProxyFromStringURLLoader::~PrefetchProxyFromStringURLLoader() = default;

void PrefetchProxyFromStringURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const absl::optional<GURL>& new_url) {
  NOTREACHED();
}

void PrefetchProxyFromStringURLLoader::SetPriority(
    net::RequestPriority priority,
    int32_t intra_priority_value) {
  // Ignore: this class doesn't have a concept of priority.
}

void PrefetchProxyFromStringURLLoader::PauseReadingBodyFromNet() {
  // Ignore: this class doesn't read from network.
}

void PrefetchProxyFromStringURLLoader::ResumeReadingBodyFromNet() {
  // Ignore: this class doesn't read from network.
}

void PrefetchProxyFromStringURLLoader::TransferRawData() {
  while (true) {
    DCHECK_GE(bytes_of_raw_data_to_transfer_, write_position_);
    uint32_t write_size =
        static_cast<uint32_t>(bytes_of_raw_data_to_transfer_ - write_position_);
    if (write_size == 0) {
      Finish(net::OK);
      return;
    }

    MojoResult result =
        producer_handle_->WriteData(body_buffer_->data() + write_position_,
                                    &write_size, MOJO_WRITE_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      handle_watcher_->ArmOrNotify();
      return;
    }

    if (result != MOJO_RESULT_OK) {
      Finish(net::ERR_FAILED);
      return;
    }

    // |write_position_| should only be updated when the mojo pipe has
    // successfully been written to.
    write_position_ += write_size;
  }
}

PrefetchProxyFromStringURLLoader::RequestHandler
PrefetchProxyFromStringURLLoader::ServingResponseHandler() {
  return base::BindOnce(&PrefetchProxyFromStringURLLoader::BindAndStart,
                        weak_ptr_factory_.GetWeakPtr());
}

void PrefetchProxyFromStringURLLoader::BindAndStart(
    const network::ResourceRequest& request,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  DCHECK(!receiver_.is_bound());
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(
      base::BindOnce(&PrefetchProxyFromStringURLLoader::OnMojoDisconnect,
                     weak_ptr_factory_.GetWeakPtr()));
  client_.Bind(std::move(client));

  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  MojoResult rv =
      mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle);

  if (rv != MOJO_RESULT_OK) {
    Finish(net::ERR_FAILED);
    return;
  }

  client_->OnReceiveResponse(std::move(head_), std::move(consumer_handle),
                             absl::nullopt);

  producer_handle_ = std::move(producer_handle);

  handle_watcher_ = std::make_unique<mojo::SimpleWatcher>(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL,
      base::SequencedTaskRunner::GetCurrentDefault());
  handle_watcher_->Watch(
      producer_handle_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
      MOJO_WATCH_CONDITION_SATISFIED,
      base::BindRepeating(&PrefetchProxyFromStringURLLoader::OnHandleReady,
                          weak_ptr_factory_.GetWeakPtr()));

  TransferRawData();
}

void PrefetchProxyFromStringURLLoader::OnHandleReady(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  if (result != MOJO_RESULT_OK) {
    Finish(net::ERR_FAILED);
    return;
  }
  TransferRawData();
}

void PrefetchProxyFromStringURLLoader::Finish(int error) {
  client_->OnComplete(network::URLLoaderCompletionStatus(error));
  handle_watcher_.reset();
  producer_handle_.reset();
  client_.reset();
  receiver_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
  MaybeDeleteSelf();
}

void PrefetchProxyFromStringURLLoader::OnMojoDisconnect() {
  receiver_.reset();
  client_.reset();
  MaybeDeleteSelf();
}

void PrefetchProxyFromStringURLLoader::MaybeDeleteSelf() {
  if (!receiver_.is_bound() && !client_.is_bound()) {
    delete this;
  }
}
