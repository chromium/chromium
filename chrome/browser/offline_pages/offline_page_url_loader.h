// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_URL_LOADER_H_
#define CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_URL_LOADER_H_

#include <cstdint>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/offline_pages/offline_page_request_handler.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/network/public/mojom/url_loader.mojom-forward.h"

namespace content {
class NavigationUIData;
}

namespace mojo {
class SimpleWatcher;
}

namespace net {
class IOBuffer;
}

namespace offline_pages {

// A url loader that serves offline contents with network service enabled.
class OfflinePageURLLoader : public network::mojom::URLLoader,
                             public OfflinePageRequestHandler::Delegate {
 public:
  // Creates and returns a loader to serve the offline page. Nullptr is returned
  // if offline page cannot or should not be served.
  //
  // See NavigationLoaderInterceptor::MaybeCreateLoader documentation for the
  // meaning of |tentative_resource_request|.
  static std::unique_ptr<OfflinePageURLLoader> Create(
      content::NavigationUIData* navigation_ui_data,
      content::FrameTreeNodeId frame_tree_node_id,
      const network::ResourceRequest& tentative_resource_request,
      content::URLLoaderRequestInterceptor::LoaderCallback callback);

  OfflinePageURLLoader(const OfflinePageURLLoader&) = delete;
  OfflinePageURLLoader& operator=(const OfflinePageURLLoader&) = delete;

  ~OfflinePageURLLoader() override;

  void SetTabIdGetterForTesting(
      OfflinePageRequestHandler::Delegate::TabIdGetter tab_id_getter);

 private:
  OfflinePageURLLoader(
      content::NavigationUIData* navigation_ui_data,
      content::FrameTreeNodeId frame_tree_node_id,
      const network::ResourceRequest& tentative_resource_request,
      content::URLLoaderRequestInterceptor::LoaderCallback callback);

  // network::mojom::URLLoader:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const std::optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  // OfflinePageRequestHandler::Delegate overrides:
  void FallbackToDefault() override;
  void NotifyStartError(int error) override;
  void NotifyHeadersComplete(int64_t file_size) override;
  void NotifyReadRawDataComplete(int bytes_read) override;
  void SetOfflinePageNavigationUIData(bool is_offline_page) override;
  int GetPageTransition() const override;
  OfflinePageRequestHandler::Delegate::WebContentsGetter GetWebContentsGetter()
      const override;
  OfflinePageRequestHandler::Delegate::TabIdGetter GetTabIdGetter()
      const override;

  void ReadRawData();
  void OnReceiveResponse(
      int64_t file_size,
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client);
  void OnReceiveError(
      int error,
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client);
  void OnHandleReady(MojoResult result, const mojo::HandleSignalsState& state);
  void Finish(int error);
  void TransferRawData();
  void OnMojoDisconnect();
  void MaybeDeleteSelf();

  // Not owned. The owner of this should outlive this class instance.
  raw_ptr<content::NavigationUIData> navigation_ui_data_;

  content::FrameTreeNodeId frame_tree_node_id_;
  int transition_type_;
  content::URLLoaderRequestInterceptor::LoaderCallback loader_callback_;

  std::unique_ptr<OfflinePageRequestHandler> request_handler_;
  scoped_refptr<net::IOBuffer> buffer_;

  mojo::Receiver<network::mojom::URLLoader> receiver_{this};
  mojo::Remote<network::mojom::URLLoaderClient> client_;
  mojo::ScopedDataPipeProducerHandle producer_handle_;
  size_t bytes_of_raw_data_to_transfer_ = 0;
  size_t write_position_ = 0;
  std::unique_ptr<mojo::SimpleWatcher> handle_watcher_;

  OfflinePageRequestHandler::Delegate::TabIdGetter tab_id_getter_;

  base::WeakPtrFactory<OfflinePageURLLoader> weak_ptr_factory_{this};
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_URL_LOADER_H_
