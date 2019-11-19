// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_COOKIE_MANAGER_H_
#define ANDROID_WEBVIEW_BROWSER_COOKIE_MANAGER_H_

#include <memory>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/containers/circular_deque.h"
#include "base/no_destructor.h"
#include "base/thread_annotations.h"
#include "base/threading/thread.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/cookie_manager.mojom-forward.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

class GURL;

namespace base {
class SingleThreadTaskRunner;
}

namespace net {
class CookieStore;
class CanonicalCookie;
}

namespace android_webview {

// CookieManager creates and owns Webview's CookieStore, in addition to handling
// calls into the CookieStore from Java.
//
// Since Java calls can be made on the IO Thread, and must synchronously return
// a result, and the CookieStore API allows it to asynchronously return results,
// the CookieStore must be run on its own thread, to prevent deadlock.
//
// Initialization:
//
// There are two possible scenarios: 1) The CookieManager is used before the
// Network Service is initialized. 2) The CookieManager is not used until after
// the Network Service is initialized (during content initialization).
//
// Case 2) is straightforward: Once the NetworkContext and the
// network::mojom::CookieManager are created, the AwContentBrowserClient calls
// PassMojoCookieManagerToAwCookieManager, which ends up calling
// CookieManager::SwapMojoCookieManagerAsync, setting the |mojo_cookie_manager_|
// member of CookieManager (the AW one; it's an unfortunately overloaded term).
//
// In case 1), the CookieManager creates a provisional CookieStore
// |cookie_store_|, which it uses for all operations (because the
// network::mojom::CookieManager doesn't exist yet): For every cookie task
// it receives, the CookieManager first checks for the presence of a
// |mojo_cookie_manager_|, and if it doesn't exist, the CookieManager checks for
// the presence of a provisionally-created CookieStore, creating one if it
// doesn't exist (in GetCookieStore). Then whichever one it found will handle
// the cookie task.
//
// When it comes time to create the NetworkContext, which comes with a
// network::mojom::CookieManager, the provisionally-created CookieStore needs to
// transfer its contents (with the results of the pre-content-initialization
// cookie tasks) to the newly created network::mojom::CookieManager. It does
// this by flushing its contents to disk and then calling the same method,
// CookieManager::SwapMojoCookieManagerAsync, which binds the newly created
// network::mojom::CookieManager to |mojo_cookie_manager_|. Thereafter, any
// cookie tasks will be handled by |mojo_cookie_manager_| because it now exists.
//
// This works because the newly created network::mojom::CookieManager reads from
// the same on-disk backing store that the provisionally-created CookieStore
// just flushed its contents to.
//
// Why is this not a race condition? This was addressed in crbug.com/933461.
// If the CookieManager receives cookie tasks while the flush is in progress,
// those tasks are added to a task queue, which is not executed until after the
// new |mojo_cookie_manager_| has finished being set. The new
// |mojo_cookie_manager_| only loads from disk upon receiving a task (*not* upon
// creation, importantly; see CookieMonster::FetchAllCookiesIfNecessary, which
// is only called if cookie tasks are received), so it will not try to load from
// disk until the flush is complete.
class CookieManager {
 public:
  static CookieManager* GetInstance();

  // Passes a |cookie_manager_remote|, which this will use for CookieManager
  // APIs going forward. Only called in the Network Service path, with the
  // intention this is called once during content initialization (when we create
  // the only NetworkContext). Note: no other cookie tasks will be processed
  // while this operation is running.
  void SetMojoCookieManager(
      mojo::PendingRemote<network::mojom::CookieManager> cookie_manager_remote);

  void SetShouldAcceptCookies(JNIEnv* env,
                              const base::android::JavaParamRef<jobject>& obj,
                              jboolean accept);
  jboolean GetShouldAcceptCookies(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void SetCookie(JNIEnv* env,
                 const base::android::JavaParamRef<jobject>& obj,
                 const base::android::JavaParamRef<jstring>& url,
                 const base::android::JavaParamRef<jstring>& value,
                 const base::android::JavaParamRef<jobject>& java_callback);
  void SetCookieSync(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj,
                     const base::android::JavaParamRef<jstring>& url,
                     const base::android::JavaParamRef<jstring>& value);

  base::android::ScopedJavaLocalRef<jstring> GetCookie(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& url);

  void RemoveAllCookies(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& java_callback);
  void RemoveSessionCookies(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& java_callback);
  void RemoveAllCookiesSync(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj);
  void RemoveSessionCookiesSync(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void RemoveExpiredCookies(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj);
  void FlushCookieStore(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj);
  jboolean HasCookies(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj);
  bool GetAllowFileSchemeCookies();
  jboolean GetAllowFileSchemeCookies(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  // Configures whether CookieManager and WebView instances will honor requests
  // to set cookies for file:// scheme URLs. This method must be called (and
  // must finish execution) before calling any other WebView APIs which modify
  // the cookie store (otherwise, this is not guaranteed to succeed).
  //
  // This blocks the calling thread until its work is done to achieve this
  // guarantee (otherwise other mojo::Remote<network::mojom::CookieManager>
  // instances might be able to modify the underlying net::CookieStore before
  // this call finishes.
  void SetAllowFileSchemeCookies(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jboolean allow);

  base::FilePath GetCookieStorePath();

 private:
  friend class base::NoDestructor<CookieManager>;

  CookieManager();
  ~CookieManager();

  // Returns the CookieStore, creating it if necessary. This must only be called
  // on the CookieStore TaskRunner.
  net::CookieStore* GetCookieStore();

  // Gets the Network Service CookieManager if it's been passed via
  // |SetMojoCookieManager|. Otherwise (if Network Service is disabled or
  // content layer has not yet initialized the NetworkContext), this returns
  // nullptr (and |GetCookieStore| should be used installed). This must only be
  // called on the CookieStore TaskRunner.
  network::mojom::CookieManager* GetMojoCookieManager();

  void ExecCookieTaskSync(
      base::OnceCallback<void(base::OnceCallback<void(bool)>)> task);
  void ExecCookieTaskSync(
      base::OnceCallback<void(base::OnceCallback<void(int)>)> task);
  void ExecCookieTaskSync(base::OnceCallback<void(base::OnceClosure)> task);
  void ExecCookieTask(base::OnceClosure task);
  // Runs all queued-up cookie tasks in |tasks_|.
  void RunPendingCookieTasks();

  void SetCookieHelper(const GURL& host,
                       const std::string& value,
                       base::OnceCallback<void(bool)> callback);

  void GotCookies(const std::vector<net::CanonicalCookie>& cookies);
  void GetCookieListAsyncHelper(const GURL& host,
                                net::CookieList* result,
                                base::OnceClosure complete);
  void GetCookieListCompleted(base::OnceClosure complete,
                              net::CookieList* result,
                              const net::CookieStatusList& value,
                              const net::CookieStatusList& excluded_cookies);

  void RemoveSessionCookiesHelper(base::OnceCallback<void(bool)> callback);
  void RemoveAllCookiesHelper(base::OnceCallback<void(bool)> callback);
  void RemoveCookiesCompleted(base::OnceCallback<void(bool)> callback,
                              uint32_t num_deleted);

  void FlushCookieStoreAsyncHelper(base::OnceClosure complete);

  void SetMojoCookieManagerAsync(
      mojo::PendingRemote<network::mojom::CookieManager> cookie_manager_remote,
      base::OnceClosure complete);
  void SwapMojoCookieManagerAsync(
      mojo::PendingRemote<network::mojom::CookieManager> cookie_manager_remote,
      base::OnceClosure complete);

  void HasCookiesAsyncHelper(bool* result, base::OnceClosure complete);
  void HasCookiesCompleted(base::OnceClosure complete,
                           bool* result,
                           const net::CookieList& cookies);

  void SetAllowFileSchemeCookiesAsyncHelper(bool allow,
                                            base::OnceClosure complete);
  // |can_change_schemes| indicates whether or not this call was successful,
  // indicating whether we may update |allow_file_scheme_cookies_|.
  void SetAllowFileSchemeCookiesCompleted(base::OnceClosure complete,
                                          bool allow,
                                          bool can_change_schemes);
  void MigrateCookieStorePath();

  base::FilePath cookie_store_path_;

  // This protects the following bool, as it's used on multiple threads.
  base::Lock allow_file_scheme_cookies_lock_;
  // True if cookies should be allowed for file URLs. Can only be changed prior
  // to creating the CookieStore.
  bool allow_file_scheme_cookies_ GUARDED_BY(allow_file_scheme_cookies_lock_);
  // True once the cookie store has been created. Just used to track when
  // |allow_file_scheme_cookies_| can no longer be modified. Only accessed on
  // |cookie_store_task_runner_|.
  bool cookie_store_created_;

  base::Thread cookie_store_client_thread_;
  base::Thread cookie_store_backend_thread_;

  scoped_refptr<base::SingleThreadTaskRunner> cookie_store_task_runner_;
  std::unique_ptr<net::CookieStore> cookie_store_;

  // Tracks if we're in the middle of a call to SetMojoCookieManager(). See the
  // note in SetMojoCookieManager(). Must only be accessed on
  // |cookie_store_task_runner_|.
  bool setting_new_mojo_cookie_manager_;

  // |tasks_| is a queue we manage, to allow us to delay tasks until after
  // SetMojoCookieManager()'s work is done. This is modified on different
  // threads, so accesses must be guarded by |task_queue_lock_|.
  base::Lock task_queue_lock_;
  base::circular_deque<base::OnceClosure> tasks_ GUARDED_BY(task_queue_lock_);

  // The CookieManager shared with the NetworkContext.
  mojo::Remote<network::mojom::CookieManager> mojo_cookie_manager_;

  DISALLOW_COPY_AND_ASSIGN(CookieManager);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_COOKIE_MANAGER_H_
