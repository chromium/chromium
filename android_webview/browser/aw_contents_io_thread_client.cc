// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_contents_io_thread_client.h"

#include <map>
#include <memory>
#include <utility>

#include "android_webview/browser/aw_settings.h"
#include "android_webview/browser/network_service/aw_web_resource_intercept_response.h"
#include "android_webview/browser/network_service/aw_web_resource_request.h"
#include "android_webview/browser_jni_headers/AwContentsBackgroundThreadClient_jni.h"
#include "android_webview/browser_jni_headers/AwContentsIoThreadClient_jni.h"
#include "android_webview/common/devtools_instrumentation.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/lock.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/embedder_support/android/util/input_stream.h"
#include "components/embedder_support/android/util/web_resource_response.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/base/data_url.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using base::LazyInstance;
using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfStrings;
using content::BrowserThread;
using content::RenderFrameHost;
using content::WebContents;
using std::map;
using std::pair;
using std::string;

namespace android_webview {

namespace {

typedef map<content::GlobalRenderFrameHostId, JavaObjectWeakGlobalRef>
    RenderFrameHostToWeakGlobalRefType;

typedef pair<base::flat_set<RenderFrameHost*>, JavaObjectWeakGlobalRef>
    HostsAndWeakGlobalRefPair;

// When browser side navigation is enabled, RenderFrameIDs do not have
// valid render process host and render frame ids for frame navigations.
// We need to identify these by using FrameTreeNodeIds. Furthermore, we need
// to keep track of which RenderFrameHosts are associated with each
// FrameTreeNodeId, so we know when the last RenderFrameHost is deleted (and
// therefore the FrameTreeNodeId should be removed).
typedef map<int, HostsAndWeakGlobalRefPair> FrameTreeNodeToWeakGlobalRefType;

// RfhToIoThreadClientMap -----------------------------------------------------
class RfhToIoThreadClientMap {
 public:
  static RfhToIoThreadClientMap* GetInstance();
  void Set(content::GlobalRenderFrameHostId rfh_id,
           const JavaObjectWeakGlobalRef& client);
  absl::optional<JavaObjectWeakGlobalRef> Get(
      content::GlobalRenderFrameHostId rfh_id);

  absl::optional<JavaObjectWeakGlobalRef> Get(int frame_tree_node_id);

  // Prefer to call these when RenderFrameHost* is available, because they
  // update both maps at the same time.
  void Set(RenderFrameHost* rfh, const JavaObjectWeakGlobalRef& client);
  void Erase(RenderFrameHost* rfh);

 private:
  base::Lock map_lock_;
  // We maintain two maps simultaneously so that we can always get the correct
  // JavaObjectWeakGlobalRef, even when only HostIdPair or FrameTreeNodeId is
  // available.
  RenderFrameHostToWeakGlobalRefType rfh_to_weak_global_ref_;
  FrameTreeNodeToWeakGlobalRefType frame_tree_node_to_weak_global_ref_;
};

// static
LazyInstance<RfhToIoThreadClientMap>::DestructorAtExit g_instance_ =
    LAZY_INSTANCE_INITIALIZER;

// static
LazyInstance<JavaObjectWeakGlobalRef>::DestructorAtExit g_sw_instance_ =
    LAZY_INSTANCE_INITIALIZER;

// static
RfhToIoThreadClientMap* RfhToIoThreadClientMap::GetInstance() {
  return g_instance_.Pointer();
}

void RfhToIoThreadClientMap::Set(content::GlobalRenderFrameHostId rfh_id,
                                 const JavaObjectWeakGlobalRef& client) {
  base::AutoLock lock(map_lock_);
  rfh_to_weak_global_ref_[rfh_id] = client;
}

absl::optional<JavaObjectWeakGlobalRef> RfhToIoThreadClientMap::Get(
    content::GlobalRenderFrameHostId rfh_id) {
  base::AutoLock lock(map_lock_);
  RenderFrameHostToWeakGlobalRefType::iterator iterator =
      rfh_to_weak_global_ref_.find(rfh_id);
  if (iterator == rfh_to_weak_global_ref_.end()) {
    return absl::nullopt;
  } else {
    return iterator->second;
  }
}

absl::optional<JavaObjectWeakGlobalRef> RfhToIoThreadClientMap::Get(
    int frame_tree_node_id) {
  base::AutoLock lock(map_lock_);
  FrameTreeNodeToWeakGlobalRefType::iterator iterator =
      frame_tree_node_to_weak_global_ref_.find(frame_tree_node_id);
  if (iterator == frame_tree_node_to_weak_global_ref_.end()) {
    return absl::nullopt;
  } else {
    return iterator->second.second;
  }
}

void RfhToIoThreadClientMap::Set(RenderFrameHost* rfh,
                                 const JavaObjectWeakGlobalRef& client) {
  int frame_tree_node_id = rfh->GetFrameTreeNodeId();
  content::GlobalRenderFrameHostId rfh_id = rfh->GetGlobalId();
  base::AutoLock lock(map_lock_);

  // If this FrameTreeNodeId already has an associated JavaObjectWeakGlobalRef,
  // add this RenderFrameHost to the hosts set (it's harmless to overwrite the
  // JavaObjectWeakGlobalRef). Otherwise, operator[] creates a new map entry and
  // we add this RenderFrameHost to the hosts set and insert |client| in the
  // pair.
  HostsAndWeakGlobalRefPair& current_entry =
      frame_tree_node_to_weak_global_ref_[frame_tree_node_id];
  current_entry.second = client;
  current_entry.first.insert(rfh);

  // Always add the entry to the HostIdPair map, since entries are 1:1 with
  // RenderFrameHosts.
  rfh_to_weak_global_ref_[rfh_id] = client;
}

void RfhToIoThreadClientMap::Erase(RenderFrameHost* rfh) {
  int frame_tree_node_id = rfh->GetFrameTreeNodeId();
  content::GlobalRenderFrameHostId rfh_id = rfh->GetGlobalId();
  base::AutoLock lock(map_lock_);
  HostsAndWeakGlobalRefPair& current_entry =
      frame_tree_node_to_weak_global_ref_[frame_tree_node_id];
  size_t num_erased = current_entry.first.erase(rfh);
  DCHECK(num_erased == 1);
  // Only remove this entry from the FrameTreeNodeId map if there are no more
  // live RenderFrameHosts.
  if (current_entry.first.empty()) {
    frame_tree_node_to_weak_global_ref_.erase(frame_tree_node_id);
  }

  // Always safe to remove the entry from the HostIdPair map, since entries are
  // 1:1 with RenderFrameHosts.
  rfh_to_weak_global_ref_.erase(rfh_id);
}

// ClientMapEntryUpdater ------------------------------------------------------

class ClientMapEntryUpdater : public content::WebContentsObserver {
 public:
  ClientMapEntryUpdater(JNIEnv* env,
                        WebContents* web_contents,
                        jobject jdelegate);

  void RenderFrameCreated(RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;
  void WebContentsDestroyed() override;

 private:
  JavaObjectWeakGlobalRef jdelegate_;
};

ClientMapEntryUpdater::ClientMapEntryUpdater(JNIEnv* env,
                                             WebContents* web_contents,
                                             jobject jdelegate)
    : content::WebContentsObserver(web_contents), jdelegate_(env, jdelegate) {
  DCHECK(web_contents);
  DCHECK(jdelegate);

  if (web_contents->GetPrimaryMainFrame())
    RenderFrameCreated(web_contents->GetPrimaryMainFrame());
}

void ClientMapEntryUpdater::RenderFrameCreated(RenderFrameHost* rfh) {
  RfhToIoThreadClientMap::GetInstance()->Set(rfh, jdelegate_);
}

void ClientMapEntryUpdater::RenderFrameDeleted(RenderFrameHost* rfh) {
  RfhToIoThreadClientMap::GetInstance()->Erase(rfh);
}

void ClientMapEntryUpdater::WebContentsDestroyed() {
  delete this;
}

}  // namespace

// AwContentsIoThreadClient -----------------------------------------------

// static
// Wrap an optional |JavaObjectWeakGlobalRef| to a Java
// |AwContentsIoThreadClient| in a native |AwContentsIoThreadClient| by getting
// a scoped local reference. This will return |nullptr| if either the optional
// is empty or the weak reference has already expired.
std::unique_ptr<AwContentsIoThreadClient> WrapOptionalWeakRef(
    absl::optional<JavaObjectWeakGlobalRef> opt_delegate_weak_ref) {
  if (opt_delegate_weak_ref) {
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobject> java_delegate = opt_delegate_weak_ref->get(env);
    if (java_delegate) {
      return std::make_unique<AwContentsIoThreadClient>(java_delegate);
    }
  }
  return nullptr;
}

// static
std::unique_ptr<AwContentsIoThreadClient> AwContentsIoThreadClient::FromID(
    content::GlobalRenderFrameHostId render_frame_host_id) {
  return WrapOptionalWeakRef(
      RfhToIoThreadClientMap::GetInstance()->Get(render_frame_host_id));
}

std::unique_ptr<AwContentsIoThreadClient> AwContentsIoThreadClient::FromID(
    int frame_tree_node_id) {
  return WrapOptionalWeakRef(
      RfhToIoThreadClientMap::GetInstance()->Get(frame_tree_node_id));
}

// static
void AwContentsIoThreadClient::SubFrameCreated(int render_process_id,
                                               int parent_render_frame_id,
                                               int child_render_frame_id) {
  content::GlobalRenderFrameHostId parent_rfh_id(render_process_id,
                                                 parent_render_frame_id);
  content::GlobalRenderFrameHostId child_rfh_id(render_process_id,
                                                child_render_frame_id);
  RfhToIoThreadClientMap* map = RfhToIoThreadClientMap::GetInstance();
  absl::optional<JavaObjectWeakGlobalRef> opt_delegate_weak_ref =
      map->Get(parent_rfh_id);
  if (opt_delegate_weak_ref) {
    map->Set(child_rfh_id, opt_delegate_weak_ref.value());
  } else {
    // It is possible to not find a mapping for the parent rfh_id if the WebView
    // is in the process of being destroyed, and the mapping has already been
    // erased.
    LOG(WARNING) << "No IoThreadClient associated with parent RenderFrameHost.";
  }
}

// static
void AwContentsIoThreadClient::Associate(WebContents* web_contents,
                                         const JavaRef<jobject>& jclient) {
  JNIEnv* env = AttachCurrentThread();
  // The ClientMapEntryUpdater lifespan is tied to the WebContents.
  new ClientMapEntryUpdater(env, web_contents, jclient.obj());
}

// static
void AwContentsIoThreadClient::SetServiceWorkerIoThreadClient(
    const base::android::JavaRef<jobject>& jclient,
    const base::android::JavaRef<jobject>& browser_context) {
  // TODO: currently there is only one browser context so it is ok to
  // store in a global variable, in the future use browser_context to
  // obtain the correct instance.
  JavaObjectWeakGlobalRef temp(AttachCurrentThread(), jclient.obj());
  g_sw_instance_.Get() = temp;
}

// static
std::unique_ptr<AwContentsIoThreadClient>
AwContentsIoThreadClient::GetServiceWorkerIoThreadClient() {
  return WrapOptionalWeakRef(absl::make_optional(g_sw_instance_.Get()));
}

AwContentsIoThreadClient::AwContentsIoThreadClient(const JavaRef<jobject>& obj)
    : java_object_(obj) {
  DCHECK(java_object_);
}

AwContentsIoThreadClient::~AwContentsIoThreadClient() = default;

AwContentsIoThreadClient::CacheMode AwContentsIoThreadClient::GetCacheMode()
    const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  JNIEnv* env = AttachCurrentThread();
  return static_cast<AwContentsIoThreadClient::CacheMode>(
      Java_AwContentsIoThreadClient_getCacheMode(env, java_object_));
}

namespace {

std::unique_ptr<AwWebResourceInterceptResponse> NoInterceptRequest() {
  return nullptr;
}

std::unique_ptr<AwWebResourceInterceptResponse> RunShouldInterceptRequest(
    AwWebResourceRequest request,
    JavaObjectWeakGlobalRef ref) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  JNIEnv* env = AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj = ref.get(env);
  if (!obj) {
    return NoInterceptRequest();
  }

  AwWebResourceRequest::AwJavaWebResourceRequest java_web_resource_request;
  AwWebResourceRequest::ConvertToJava(env, request, &java_web_resource_request);

  devtools_instrumentation::ScopedEmbedderCallbackTask embedder_callback(
      "shouldInterceptRequest");
  ScopedJavaLocalRef<jobject> java_ref =
      Java_AwContentsBackgroundThreadClient_shouldInterceptRequestFromNative(
          env, obj, java_web_resource_request.jurl,
          request.is_outermost_main_frame, request.has_user_gesture,
          java_web_resource_request.jmethod,
          java_web_resource_request.jheader_names,
          java_web_resource_request.jheader_values);

  DCHECK(java_ref)
      << "shouldInterceptRequestFromNative() should return non-null value";
  auto web_resource_intercept_response =
      std::make_unique<AwWebResourceInterceptResponse>(java_ref);

  bool has_response = web_resource_intercept_response->HasResponse(env);
  UMA_HISTOGRAM_BOOLEAN(
      "Android.WebView.ShouldInterceptRequest.IsRequestIntercepted",
      has_response);
  return web_resource_intercept_response;
}

}  // namespace

void AwContentsIoThreadClient::ShouldInterceptRequestAsync(
    AwWebResourceRequest request,
    ShouldInterceptRequestResponseCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::OnceCallback<std::unique_ptr<AwWebResourceInterceptResponse>()>
      get_response = base::BindOnce(&NoInterceptRequest);
  JNIEnv* env = AttachCurrentThread();
  if (!bg_thread_client_object_) {
    bg_thread_client_object_.Reset(
        Java_AwContentsIoThreadClient_getBackgroundThreadClient(env,
                                                                java_object_));
  }
  if (bg_thread_client_object_) {
    get_response = base::BindOnce(
        &RunShouldInterceptRequest, std::move(request),
        JavaObjectWeakGlobalRef(env, bg_thread_client_object_.obj()));
  }
  sequenced_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, std::move(get_response), std::move(callback));
}

bool AwContentsIoThreadClient::ShouldBlockContentUrls() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  JNIEnv* env = AttachCurrentThread();
  return Java_AwContentsIoThreadClient_shouldBlockContentUrls(env,
                                                              java_object_);
}

bool AwContentsIoThreadClient::ShouldBlockFileUrls() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  JNIEnv* env = AttachCurrentThread();
  return Java_AwContentsIoThreadClient_shouldBlockFileUrls(env, java_object_);
}

bool AwContentsIoThreadClient::ShouldBlockSpecialFileUrls() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  JNIEnv* env = AttachCurrentThread();
  return Java_AwContentsIoThreadClient_shouldBlockSpecialFileUrls(env,
                                                                  java_object_);
}

bool AwContentsIoThreadClient::ShouldAcceptThirdPartyCookies() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  JNIEnv* env = AttachCurrentThread();
  return Java_AwContentsIoThreadClient_shouldAcceptThirdPartyCookies(
      env, java_object_);
}

bool AwContentsIoThreadClient::GetSafeBrowsingEnabled() const {
  DCHECK_CURRENTLY_ON(
      base::FeatureList::IsEnabled(safe_browsing::kSafeBrowsingOnUIThread)
          ? content::BrowserThread::UI
          : content::BrowserThread::IO);

  JNIEnv* env = AttachCurrentThread();
  return Java_AwContentsIoThreadClient_getSafeBrowsingEnabled(env,
                                                              java_object_);
}

bool AwContentsIoThreadClient::ShouldBlockNetworkLoads() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  JNIEnv* env = AttachCurrentThread();
  return Java_AwContentsIoThreadClient_shouldBlockNetworkLoads(env,
                                                               java_object_);
}

}  // namespace android_webview
