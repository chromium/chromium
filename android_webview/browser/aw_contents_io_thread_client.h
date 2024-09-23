// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_CONTENTS_IO_THREAD_CLIENT_H_
#define ANDROID_WEBVIEW_BROWSER_AW_CONTENTS_IO_THREAD_CLIENT_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "android_webview/browser/aw_settings.h"
#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "base/functional/callback_forward.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/global_routing_id.h"

namespace content {
class WebContents;
}

namespace embedder_support {
class InputStream;
}

namespace android_webview {

class AwWebResourceInterceptResponse;
struct AwWebResourceRequest;

// This class provides a means of calling Java methods on an instance that has
// a 1:1 relationship with a WebContents instance directly from the IO thread.
//
// Specifically this is used to associate URLRequests with the WebContents that
// the URLRequest is made for.
//
// The native class is intended to be a short-lived handle that pins the
// Java-side instance. It is preferable to use the static getter methods to
// obtain a new instance of the class rather than holding on to one for
// prolonged periods of time (see note for more details).
//
// Note: The native AwContentsIoThreadClient instance has a Global ref to
// the Java object. By keeping the native AwContentsIoThreadClient
// instance alive you're also prolonging the lifetime of the Java instance, so
// don't keep a AwContentsIoThreadClient if you don't need to.
class AwContentsIoThreadClient {
 public:
  // Corresponds to WebSettings cache mode constants.
  enum CacheMode {
    LOAD_DEFAULT = -1,
    LOAD_NORMAL = 0,
    LOAD_CACHE_ELSE_NETWORK = 1,
    LOAD_NO_CACHE = 2,
    LOAD_CACHE_ONLY = 3,
  };

  // Associates the |jclient| instance (which must implement the
  // AwContentsIoThreadClient Java interface) with the |web_contents|.
  // This should be called at most once per |web_contents|.
  static void Associate(content::WebContents* web_contents,
                        const base::android::JavaRef<jobject>& jclient);

  // |jclient| must hold a non-null Java object.
  explicit AwContentsIoThreadClient(
      const base::android::JavaRef<jobject>& jclient);

  AwContentsIoThreadClient(const AwContentsIoThreadClient&) = delete;
  AwContentsIoThreadClient& operator=(const AwContentsIoThreadClient&) = delete;

  ~AwContentsIoThreadClient();

  // Implementation of AwContentsIoThreadClient.

  // Retrieve CacheMode setting value of this AwContents.
  // This method is called on the IO thread only.
  CacheMode GetCacheMode() const;

  // This will attempt to fetch the AwContentsIoThreadClient for the given
  // blink::LocalFrameToken.
  // This method can be called from any thread.
  // A null std::unique_ptr is a valid return value.
  static std::unique_ptr<AwContentsIoThreadClient> FromToken(
      const content::GlobalRenderFrameHostToken& global_frame_token);

  // This map is useful when browser side navigations are enabled as
  // render_frame_ids will not be valid anymore for some of the navigations.
  static std::unique_ptr<AwContentsIoThreadClient> FromID(
      content::FrameTreeNodeId frame_tree_node_id);

  // Called on the IO thread when a subframe is created.
  static void SubFrameCreated(int child_id,
                              const blink::LocalFrameToken& parent_frame_token,
                              const blink::LocalFrameToken& child_frame_token);

  // This method is called on the IO thread only.
  struct InterceptResponseData {
    InterceptResponseData();
    ~InterceptResponseData();

    // Move only.
    InterceptResponseData(InterceptResponseData&& other);
    InterceptResponseData& operator=(InterceptResponseData&& other);

    std::unique_ptr<AwWebResourceInterceptResponse> response;
    std::unique_ptr<embedder_support::InputStream> input_stream;
  };
  using ShouldInterceptRequestResponseCallback =
      base::OnceCallback<void(InterceptResponseData)>;
  void ShouldInterceptRequestAsync(
      AwWebResourceRequest request,
      ShouldInterceptRequestResponseCallback callback);

  // Check if the request should be blocked based on web content ownership.
  bool ShouldBlockRequest(AwWebResourceRequest request);

  // Retrieve the AllowContentAccess setting value of this AwContents.
  // This method is called on the IO thread only.
  bool ShouldBlockContentUrls() const;

  // Retrieve the AllowFileAccess setting value of this AwContents.
  // This method is called on the IO thread only.
  bool ShouldBlockFileUrls() const;

  // Retrieves if special android file urls (android_{asset/res}) should be
  // allowed.
  bool ShouldBlockSpecialFileUrls() const;

  // Retrieve the BlockNetworkLoads setting value of this AwContents.
  // This method is called on the IO thread only.
  bool ShouldBlockNetworkLoads() const;

  // Retrieve the AcceptCookies setting value of this AwContents.
  bool ShouldAcceptCookies() const;

  // Retrieve the AcceptThirdPartyCookies setting value of this AwContents.
  bool ShouldAcceptThirdPartyCookies() const;

  // Retrieve the SafeBrowsingEnabled setting value of this AwContents.
  bool GetSafeBrowsingEnabled() const;

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
  base::android::ScopedJavaGlobalRef<jobject> bg_thread_client_object_;
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_CONTENTS_IO_THREAD_CLIENT_H_
