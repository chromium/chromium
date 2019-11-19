// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_ANDROID_STREAM_READER_URL_LOADER_H_
#define ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_ANDROID_STREAM_READER_URL_LOADER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/http/http_byte_range.h"
#include "services/network/public/cpp/net_adapters.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace android_webview {

class InputStream;
class InputStreamReaderWrapper;

// Custom URLLoader implementation for loading responses from Android
// InputStreams. Although this works generally for implementers of the
// ResponseDelegate interface, this specifically aims to support:
//
//  - shouldInterceptRequest callback
//  - content:// URLs, which load content from Android ContentProviders (which
//    could be in-app or come from other apps)
//  - file:///android_asset/ & file:///android_res/ URLs, which load in-app
//    content from the app's asset/ and res/ folders
class AndroidStreamReaderURLLoader : public network::mojom::URLLoader {
 public:
  // Delegate abstraction for obtaining input streams.
  class ResponseDelegate {
   public:
    virtual ~ResponseDelegate() {}

    // This method is called from a worker thread, not from the IO thread.
    virtual std::unique_ptr<android_webview::InputStream> OpenInputStream(
        JNIEnv* env) = 0;

    // This method is called on the URLLoader thread (IO thread) if the
    // result of calling OpenInputStream was null.
    // Returns true if the request was restarted with a new loader or
    // was completed, false otherwise.
    virtual bool OnInputStreamOpenFailed() = 0;

    virtual bool GetMimeType(JNIEnv* env,
                             const GURL& url,
                             android_webview::InputStream* stream,
                             std::string* mime_type) = 0;

    virtual bool GetCharset(JNIEnv* env,
                            const GURL& url,
                            android_webview::InputStream* stream,
                            std::string* charset) = 0;

    virtual void AppendResponseHeaders(JNIEnv* env,
                                       net::HttpResponseHeaders* headers) = 0;
  };

  AndroidStreamReaderURLLoader(
      const network::ResourceRequest& resource_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      std::unique_ptr<ResponseDelegate> response_delegate);
  ~AndroidStreamReaderURLLoader() override;

  void Start();

  // network::mojom::URLLoader overrides:
  void FollowRedirect(const std::vector<std::string>& removed_headers,
                      const net::HttpRequestHeaders& modified_headers,
                      const base::Optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

 private:
  bool ParseRange(const net::HttpRequestHeaders& headers);
  void OnInputStreamOpened(
      std::unique_ptr<AndroidStreamReaderURLLoader::ResponseDelegate>
          returned_delegate,
      std::unique_ptr<android_webview::InputStream> input_stream);
  void OnReaderSeekCompleted(int result);
  void HeadersComplete(int status_code, const std::string& status_text);
  void RequestComplete(int status_code);
  void SendBody();

  void OnDataPipeWritable(MojoResult result);
  void CleanUp();

  // Called after trying to read some bytes from the stream. |result| can be a
  // positive number (the number of bytes read), zero (no bytes were read
  // because the stream is finished), or negative (error condition).
  void DidRead(int result);
  // Reads some bytes from the stream. Calls |DidRead| after each read (also, in
  // the case where it fails to read due to an error).
  void ReadMore();
  // Send response headers and the data pipe consumer handle (for the body) to
  // the URLLoaderClient. Requires |consumer_handle_| to be valid, and will make
  // |consumer_handle_| invalid after running.
  void SendResponseToClient();

  // Expected content size
  int64_t expected_content_size_ = -1;
  mojo::ScopedDataPipeConsumerHandle consumer_handle_;

  net::HttpByteRange byte_range_;
  network::ResourceRequest resource_request_;
  network::mojom::URLResponseHeadPtr response_head_;
  mojo::Remote<network::mojom::URLLoaderClient> client_;
  const net::MutableNetworkTrafficAnnotationTag traffic_annotation_;
  std::unique_ptr<ResponseDelegate> response_delegate_;
  scoped_refptr<InputStreamReaderWrapper> input_stream_reader_wrapper_;

  mojo::ScopedDataPipeProducerHandle producer_handle_;
  scoped_refptr<network::NetToMojoPendingBuffer> pending_buffer_;
  mojo::SimpleWatcher writable_handle_watcher_;
  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<AndroidStreamReaderURLLoader> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AndroidStreamReaderURLLoader);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_ANDROID_STREAM_READER_URL_LOADER_H_
