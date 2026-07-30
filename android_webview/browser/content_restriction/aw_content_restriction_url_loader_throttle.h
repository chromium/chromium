// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_CONTENT_RESTRICTION_AW_CONTENT_RESTRICTION_URL_LOADER_THROTTLE_H_
#define ANDROID_WEBVIEW_BROWSER_CONTENT_RESTRICTION_AW_CONTENT_RESTRICTION_URL_LOADER_THROTTLE_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace android_webview {

class AwContentRestrictionManagerClient;
class AwContentRestrictionBlockedNavigationTracker;

// URLLoaderThrottle implementation for enforcing content restriction in
// WebViews.
class AwContentRestrictionURLLoaderThrottle : public blink::URLLoaderThrottle {
 public:
  explicit AwContentRestrictionURLLoaderThrottle(
      AwContentRestrictionManagerClient* client,
      AwContentRestrictionBlockedNavigationTracker* tracker,
      std::optional<int64_t> navigation_id);
  AwContentRestrictionURLLoaderThrottle(
      const AwContentRestrictionURLLoaderThrottle&) = delete;
  AwContentRestrictionURLLoaderThrottle& operator=(
      const AwContentRestrictionURLLoaderThrottle&) = delete;
  ~AwContentRestrictionURLLoaderThrottle() override;

  // blink::URLLoaderThrottle:
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& response_head,
      bool* defer,
      network::HttpRequestHeadersUpdateParams* headers_update_params) override;

 private:
  // Asynchronous bridge used to stream chunked and non-chunked data from
  // Chromium's mojo IPC data system into the (parcel) file descriptor.
  class DataPipeStreamerBase;
  class NonChunkedDataPipeStreamer;
  class ChunkedDataPipeStreamer;

  // Proxy data pipe getter used to replace the original chunked data pipe
  // getter. This is needed because the original chunked data pipe getter cannot
  // be cloned unlike the non-chunked version.
  class ProxyChunkedDataPipeGetter;

  // Streams request body contents into the specified file descriptor.
  void WriteRequestBodyToPipe(
      int write_fd,
      scoped_refptr<network::ResourceRequestBody> request_body);

  // Helper method responsible for setting up and streaming a chunked data pipe.
  void WriteChunkedDataPipeToPipe(
      scoped_refptr<base::RefCountedData<base::ScopedFD>> write_fd_wrapper,
      network::DataElement& element);

  // Internal callback helper used to handle the content classification result.
  void OnClassificationResult(bool is_allowed);

  raw_ptr<AwContentRestrictionManagerClient>
      content_restriction_manager_client_;
  raw_ptr<AwContentRestrictionBlockedNavigationTracker> tracker_;
  const std::optional<int64_t> navigation_id_;

  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;

  // List of streamers and proxy chunked data pipe getters used as memory
  // anchors to keep mojo IPC channels open until classification completion.
  std::vector<std::unique_ptr<DataPipeStreamerBase>> streamers_;
  std::vector<std::unique_ptr<ProxyChunkedDataPipeGetter>>
      proxy_chunked_data_pipe_getters_;

  base::WeakPtrFactory<AwContentRestrictionURLLoaderThrottle> weak_ptr_factory_{
      this};
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_CONTENT_RESTRICTION_AW_CONTENT_RESTRICTION_URL_LOADER_THROTTLE_H_
