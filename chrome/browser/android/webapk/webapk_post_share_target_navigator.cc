// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_post_share_target_navigator.h"

#include <jni.h>

#include <optional>
#include <sstream>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/timer/timer.h"
#include "chrome/browser/android/webapk/webapk_features.h"
#include "chrome/browser/web_share_target/target_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "net/base/mime_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"
#include "url/origin.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/WebApkPostShareTargetNavigator_jni.h"

using base::android::JavaRef;

namespace webapk {

namespace {

void NavigateShareTargetPostInternal(
    const scoped_refptr<network::ResourceRequestBody>& post_data,
    const std::string& header_list,
    const GURL& share_target_gurl,
    bool should_replace_current_entry,
    content::WebContents* web_contents) {
  content::OpenURLParams open_url_params =
      content::OpenURLParams::CreateBrowserInitiated(
          share_target_gurl, WindowOpenDisposition::kCurrentTab,
          ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL);
  // The share target action URL has already been verified to be within the web
  // app's scope, so the app itself is the initiator of this navigation.
  // Attributing the navigation to it makes the network stack compute SameSite
  // the same way it would for a form submission by the app. Without an
  // initiator the request looks like a user-typed navigation, which would
  // attach SameSite=Strict/Lax cookies even after a server-side redirect to a
  // cross-site URL.
  open_url_params.initiator_origin = url::Origin::Create(share_target_gurl);
  open_url_params.post_data = post_data;
  open_url_params.extra_headers = header_list;
  // When the POST follows a bootstrap load of the start URL, replace that
  // entry so that going back from the share target page exits the app rather
  // than returning to the bootstrap page.
  open_url_params.should_replace_current_entry = should_replace_current_entry;
  web_contents->OpenURL(open_url_params, /*navigation_handle_callback=*/{});
}

// Holds a POST share target navigation for a WebContents until a service
// worker controlling the share target action URL is available, so that the
// POST is handled by the service worker (which typically stashes the shared
// files and redirects to the app UI) rather than going to the network.
//
// If no service worker is registered, the web app's start URL is loaded in
// the WebContents to let the page register its service worker. A newly
// installed service worker activates immediately (there is no previous
// version to wait for), and the subsequent main-frame POST navigation in the
// same WebContents is intercepted by it, even though the bootstrap page itself
// is not controlled (no clients.claim() is needed).
//
// While waiting:
// - Navigations started by the page (e.g. a sign-in redirect chain, or the
//   user following a link) are part of the app's flow and do not affect the
//   pending POST.
// - A browser-initiated navigation (a new intent such as a relaunch, deep link
//   or GET share, Back, reload, or a direct NavigateShareTargetPost()) means
//   that the user has moved on, so the pending POST is dropped rather than
//   navigating away from where the user now is.
// - If the bootstrap load fails, the POST is sent straight away.
// - If no service worker activates within a timeout, the POST is sent anyway.
//   The timeout only counts while a page of the app's origin is committed (see
//   UpdateTimeoutTimer()), so that e.g. signing in on another origin is not
//   interrupted. While on another origin the POST waits until the user returns
//   to the app's origin or navigates elsewhere, up to a maximum wait, after
//   which it is dropped.
// - If the service worker activates while a page of another origin is
//   committed, the POST is sent once a page of the app's origin commits, rather
//   than navigating away from e.g. a sign-in page.
// The POST only replaces the current history entry if that entry is still the
// bootstrap entry.
//
// Owned by the WebContents; deletes itself once the POST has been sent or
// dropped.
class PendingShareTargetPost
    : public content::WebContentsUserData<PendingShareTargetPost>,
      public content::WebContentsObserver,
      public content::ServiceWorkerContextObserver {
 public:
  PendingShareTargetPost(const PendingShareTargetPost&) = delete;
  PendingShareTargetPost& operator=(const PendingShareTargetPost&) = delete;
  ~PendingShareTargetPost() override = default;

  static void Start(
      content::WebContents* web_contents,
      const scoped_refptr<network::ResourceRequestBody>& post_data,
      const std::string& header_list,
      const GURL& share_target_gurl,
      const GURL& bootstrap_gurl) {
    // A newer share replaces any share that is still waiting.
    Cancel(web_contents);
    CreateForWebContents(web_contents, post_data, header_list,
                         share_target_gurl, bootstrap_gurl);
    FromWebContents(web_contents)->CheckForServiceWorker();
  }

  // Drops any pending POST for |web_contents|.
  static void Cancel(content::WebContents* web_contents) {
    web_contents->RemoveUserData(UserDataKey());
  }

 private:
  friend class content::WebContentsUserData<PendingShareTargetPost>;

  PendingShareTargetPost(
      content::WebContents* web_contents,
      const scoped_refptr<network::ResourceRequestBody>& post_data,
      const std::string& header_list,
      const GURL& share_target_gurl,
      const GURL& bootstrap_gurl)
      : content::WebContentsUserData<PendingShareTargetPost>(*web_contents),
        content::WebContentsObserver(web_contents),
        post_data_(post_data),
        header_list_(header_list),
        share_target_gurl_(share_target_gurl),
        bootstrap_gurl_(bootstrap_gurl),
        service_worker_context_(web_contents->GetBrowserContext()
                                    ->GetDefaultStoragePartition()
                                    ->GetServiceWorkerContext()) {
    // Observe from the start, so that OnDestruct() clears
    // |service_worker_context_| even while CheckHasServiceWorker() is in
    // flight, and so that no activation is missed.
    service_worker_context_observation_.Observe(service_worker_context_);
  }

  void CheckForServiceWorker() {
    // CheckHasServiceWorker() waits for an installing worker to finish
    // installing. An installed worker is enough: the navigation request
    // handler waits for it to activate before dispatching the fetch event.
    service_worker_context_->CheckHasServiceWorker(
        share_target_gurl_, GetStorageKey(),
        base::BindOnce(&PendingShareTargetPost::OnCheckHasServiceWorker,
                       weak_factory_.GetWeakPtr()));
  }

  blink::StorageKey GetStorageKey() const {
    return blink::StorageKey::CreateFirstParty(
        url::Origin::Create(share_target_gurl_));
  }

  void OnCheckHasServiceWorker(content::ServiceWorkerCapability capability) {
    if (capability != content::ServiceWorkerCapability::NO_SERVICE_WORKER ||
        !service_worker_context_) {
      SendPostAndDestroy();
      return;
    }

    // Only bootstrap an app that has never registered a service worker (e.g.
    // a preinstalled app that has never been launched). If the origin already
    // has a registration, just not one whose scope includes the action (or
    // the app handles the POST on its server), loading the start URL will not
    // add one, so send the POST now rather than after the timeout. This runs
    // after CheckHasServiceWorker() has gone to storage, by which time the
    // registry knows the registered storage keys, so it does not return its
    // conservative "true" for the cold start case this is meant to handle.
    if (service_worker_context_->MaybeHasRegistrationForStorageKey(
            GetStorageKey())) {
      SendPostAndDestroy();
      return;
    }

    content::NavigationController::LoadURLParams params(bootstrap_gurl_);
    params.transition_type = ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL;
    // As for the POST, attribute the load to the app (the bootstrap URL is
    // same-origin with the share target action) rather than treating it as a
    // user-typed navigation, so that SameSite cookies are computed correctly
    // if it redirects cross-site. See crbug.com/40061291.
    params.initiator_origin = url::Origin::Create(bootstrap_gurl_);

    max_wait_timer_.Start(FROM_HERE,
                          kShareTargetWaitForServiceWorkerMaxWait.Get(),
                          base::BindOnce(&PendingShareTargetPost::OnMaxWait,
                                         weak_factory_.GetWeakPtr()));

    // DidStartNavigation() for the bootstrap navigation may run before
    // LoadURLWithParams() returns; it must not be mistaken for an unrelated
    // browser-initiated navigation.
    base::WeakPtr<PendingShareTargetPost> weak_this =
        weak_factory_.GetWeakPtr();
    starting_bootstrap_ = true;
    base::WeakPtr<content::NavigationHandle> handle =
        GetWebContents().GetController().LoadURLWithParams(params);
    if (!weak_this) {
      return;
    }
    starting_bootstrap_ = false;
    if (!handle) {
      // The bootstrap navigation did not start, or already finished.
      if (!bootstrap_navigation_id_) {
        SendPostAndDestroy();
      }
      return;
    }
    bootstrap_navigation_id_ = handle->GetNavigationId();
  }

  // content::WebContentsObserver:
  void DidStartNavigation(content::NavigationHandle* handle) override {
    if (!handle->IsInPrimaryMainFrame()) {
      return;
    }
    // The bootstrap navigation is same-document if the tab already shows the
    // bootstrap URL's document, e.g. on a warm start with a start URL that has
    // a fragment.
    if (starting_bootstrap_) {
      bootstrap_navigation_id_ = handle->GetNavigationId();
      return;
    }
    if (handle->IsSameDocument()) {
      return;
    }
    if (handle->GetNavigationId() == bootstrap_navigation_id_ ||
        handle->IsRendererInitiated()) {
      return;
    }
    Cancel(&GetWebContents());  // Deletes |this|.
  }

  void DidFinishNavigation(content::NavigationHandle* handle) override {
    // While the WebContents is being destroyed, in-flight navigations finish
    // without committing. Do not treat that as a failed bootstrap and send the
    // POST into a WebContents that is going away.
    if (GetWebContents().IsBeingDestroyed()) {
      return;
    }
    // Nothing to track until the bootstrap navigation has started.
    if (!bootstrap_navigation_id_ || !handle->IsInPrimaryMainFrame()) {
      return;
    }
    const bool is_bootstrap =
        handle->GetNavigationId() == *bootstrap_navigation_id_;
    // Other same-document navigations do not change the document, so do not
    // affect the timeout. A same-document bootstrap navigation must still be
    // tracked, or the timeout would never start.
    if (handle->IsSameDocument() && !is_bootstrap) {
      return;
    }
    if (is_bootstrap) {
      if (handle->HasCommitted()) {
        bootstrap_entry_id_ = GetWebContents()
                                  .GetController()
                                  .GetLastCommittedEntry()
                                  ->GetUniqueID();
      }
      if (!handle->HasCommitted() || handle->IsErrorPage()) {
        // The page will not register a service worker; do not wait for it.
        SendPostAndDestroy();
        return;
      }
    }
    if (handle->HasCommitted()) {
      if (service_worker_activated_ && IsOnAppOrigin()) {
        SendPostAndDestroy();
        return;
      }
      UpdateTimeoutTimer(handle->GetURL());
    }
  }

  bool IsOnAppOrigin() {
    return url::IsSameOriginWith(GetWebContents().GetLastCommittedURL(),
                                 share_target_gurl_);
  }

  // The timeout only runs while a page of the app's origin, which can register
  // the service worker, is committed. It does not start until the bootstrap
  // page commits, and is stopped while on another origin, e.g. a sign-in page
  // that the start URL redirected to, which can take the user much longer than
  // the timeout. It starts again (from the full timeout) once a page of the
  // app's origin commits again, e.g. after the sign-in completes.
  void UpdateTimeoutTimer(const GURL& committed_url) {
    if (!url::IsSameOriginWith(committed_url, share_target_gurl_)) {
      timeout_timer_.Stop();
      return;
    }
    if (!timeout_timer_.IsRunning()) {
      timeout_timer_.Start(FROM_HERE,
                           kShareTargetWaitForServiceWorkerTimeout.Get(),
                           base::BindOnce(&PendingShareTargetPost::OnTimeout,
                                          weak_factory_.GetWeakPtr()));
    }
  }

  // content::ServiceWorkerContextObserver:
  void OnVersionActivated(int64_t version_id, const GURL& scope) override {
    if (!content::ServiceWorkerContext::ScopeMatches(scope,
                                                     share_target_gurl_)) {
      return;
    }
    // Once the bootstrap has started, do not navigate away from a page of
    // another origin (e.g. a sign-in page) that the user may be using. Send the
    // POST once a page of the app's origin commits instead.
    if (bootstrap_navigation_id_ && !IsOnAppOrigin()) {
      service_worker_activated_ = true;
      return;
    }
    SendPostAndDestroy();
  }

  void OnDestruct(content::ServiceWorkerContext* context) override {
    // Stop using the context. If OnCheckHasServiceWorker() still runs, it
    // sends the POST without waiting, as before this feature.
    service_worker_context_observation_.Reset();
    service_worker_context_ = nullptr;
  }

  void OnTimeout() {
    // Preserve the previous behavior: send the POST even though there is no
    // service worker to handle it.
    SendPostAndDestroy();
  }

  void OnMaxWait() {
    // Send the POST if the user is on the app's origin, as on timeout. Do not
    // send it into a page of another origin that the user has stayed on for
    // this long (e.g. an abandoned sign-in); drop it instead.
    if (IsOnAppOrigin()) {
      SendPostAndDestroy();
      return;
    }
    Cancel(&GetWebContents());  // Deletes |this|.
  }

  // Whether the POST should replace the current history entry: only if that
  // entry is the committed bootstrap entry, not a page the user has navigated
  // to since (or the page that was there before the bootstrap committed).
  bool ShouldReplaceCurrentEntry() {
    if (!bootstrap_entry_id_) {
      return false;
    }
    content::NavigationEntry* entry =
        GetWebContents().GetController().GetLastCommittedEntry();
    return entry && entry->GetUniqueID() == *bootstrap_entry_id_;
  }

  // Sends the POST navigation. Deletes |this|.
  void SendPostAndDestroy() {
    content::WebContents* web_contents = &GetWebContents();
    const bool should_replace_current_entry = ShouldReplaceCurrentEntry();
    scoped_refptr<network::ResourceRequestBody> post_data =
        std::move(post_data_);
    std::string header_list = std::move(header_list_);
    GURL share_target_gurl = share_target_gurl_;

    Cancel(web_contents);  // Deletes |this|.

    NavigateShareTargetPostInternal(post_data, header_list, share_target_gurl,
                                    should_replace_current_entry, web_contents);
  }

  scoped_refptr<network::ResourceRequestBody> post_data_;
  std::string header_list_;
  const GURL share_target_gurl_;
  const GURL bootstrap_gurl_;
  raw_ptr<content::ServiceWorkerContext> service_worker_context_;
  base::ScopedObservation<content::ServiceWorkerContext,
                          content::ServiceWorkerContextObserver>
      service_worker_context_observation_{this};
  base::OneShotTimer timeout_timer_;
  // Limits the total wait, including while on another origin.
  base::OneShotTimer max_wait_timer_;
  // Whether a service worker whose scope includes the share target activated
  // while a page of another origin was committed.
  bool service_worker_activated_ = false;
  // True while the bootstrap navigation is being started.
  bool starting_bootstrap_ = false;
  // The bootstrap navigation, once started.
  std::optional<int64_t> bootstrap_navigation_id_;
  // The history entry committed by the bootstrap navigation.
  std::optional<int> bootstrap_entry_id_;
  base::WeakPtrFactory<PendingShareTargetPost> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(PendingShareTargetPost);

}  // namespace

void NavigateShareTargetPost(
    const scoped_refptr<network::ResourceRequestBody>& post_data,
    const std::string& header_list,
    const GURL& share_target_gurl,
    content::WebContents* web_contents) {
  // A direct POST supersedes any POST still waiting for a service worker.
  PendingShareTargetPost::Cancel(web_contents);
  NavigateShareTargetPostInternal(post_data, header_list, share_target_gurl,
                                  /*should_replace_current_entry=*/false,
                                  web_contents);
}

void NavigateShareTargetPostWhenServiceWorkerReady(
    const scoped_refptr<network::ResourceRequestBody>& post_data,
    const std::string& header_list,
    const GURL& share_target_gurl,
    const GURL& bootstrap_gurl,
    content::WebContents* web_contents) {
  if (!base::FeatureList::IsEnabled(kShareTargetWaitForServiceWorker) ||
      !bootstrap_gurl.is_valid() || !bootstrap_gurl.SchemeIsHTTPOrHTTPS() ||
      !url::IsSameOriginWith(bootstrap_gurl, share_target_gurl)) {
    NavigateShareTargetPost(post_data, header_list, share_target_gurl,
                            web_contents);
    return;
  }
  PendingShareTargetPost::Start(web_contents, post_data, header_list,
                                share_target_gurl, bootstrap_gurl);
}

}  // namespace webapk

static void JNI_WebApkPostShareTargetNavigator_NativeLoadViewForShareTargetPost(
    JNIEnv* env,
    const bool java_is_multipart_encoding,
    const std::vector<std::string>& names,
    const std::vector<std::string>& values,
    const JavaRef<jbooleanArray>& java_is_value_file_uris,
    const std::vector<std::string>& filenames,
    const std::vector<std::string>& types,
    const std::string& url,
    const GURL& bootstrap_url,
    const JavaRef<jobject>& java_web_contents) {
  std::vector<bool> is_value_file_uris;

  bool is_multipart_encoding = java_is_multipart_encoding;
  base::android::JavaBooleanArrayToBoolVector(env, java_is_value_file_uris,
                                              &is_value_file_uris);

  GURL share_target_gurl(url);

  scoped_refptr<network::ResourceRequestBody> post_data;
  std::string header_list;
  if (is_multipart_encoding) {
    std::string boundary = net::GenerateMimeMultipartBoundary();
    header_list = base::StringPrintf(
        "Content-Type: multipart/form-data; boundary=%s\r\n", boundary.c_str());
    post_data = web_share_target::ComputeMultipartBody(
        names, values, is_value_file_uris, filenames, types, boundary);
  } else {
    std::string body = web_share_target::ComputeUrlEncodedBody(names, values);
    header_list = "Content-Type: application/x-www-form-urlencoded\r\n";
    post_data = network::ResourceRequestBody::CreateFromCopyOfBytes(
        base::as_byte_span(body));
  }

  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  webapk::NavigateShareTargetPostWhenServiceWorkerReady(
      post_data, header_list, share_target_gurl, bootstrap_url, web_contents);
}

DEFINE_JNI(WebApkPostShareTargetNavigator)
