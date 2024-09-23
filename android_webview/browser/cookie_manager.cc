// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/cookie_manager.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_browser_context_store.h"
#include "android_webview/browser/aw_client_hints_controller_delegate.h"
#include "android_webview/browser/aw_cookie_access_policy.h"
#include "android_webview/common/aw_switches.h"
#include "base/android/build_info.h"
#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/android/path_utils.h"
#include "base/android/scoped_java_ref.h"
#include "base/command_line.h"
#include "base/containers/circular_deque.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cookie_store_factory.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/parsed_cookie.h"
#include "net/url_request/url_request_context.h"
#include "services/network/cookie_access_delegate_impl.h"
#include "services/network/network_service.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/url_constants.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwCookieManager_jni.h"

using base::WaitableEvent;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using content::BrowserThread;
using net::CookieList;

// In the future, we may instead want to inject an explicit CookieStore
// dependency into this object during process initialization to avoid
// depending on the URLRequestContext.
// See issue http://crbug.com/157683

// On the CookieManager methods without a callback and methods with a callback
// when that callback is null can be called from any thread, including threads
// without a message loop. Methods with a non-null callback must be called on
// a thread with a running message loop.

namespace android_webview {

namespace {

void MaybeRunCookieCallback(base::OnceCallback<void(bool)> callback,
                            const bool& result) {
  if (callback)
    std::move(callback).Run(result);
}

const char kSecureCookieHistogramName[] = "Android.WebView.SecureCookieAction";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SecureCookieAction {
  kInvalidUrl = 0,
  kAlreadySecureScheme = 1,
  kInvalidCookie = 2,
  kNotASecureCookie = 3,
  kFixedUp = 4,
  kDisallowedAndroidR = 5,
  kMaxValue = kDisallowedAndroidR,
};

GURL MaybeFixUpSchemeForSecureCookie(const GURL& host,
                                     const std::string& value,
                                     bool workaround_http_secure_cookies,
                                     bool* should_allow_cookie) {
  net::ParsedCookie parsed_cookie(value);

  *should_allow_cookie = true;

  // Log message for catching strict secure cookies related bugs.
  // TODO(ntfschr): try to remove this, based on UMA stats
  // (https://crbug.com/933981)
  if (!host.is_valid()) {
    base::UmaHistogramEnumeration(kSecureCookieHistogramName,
                                  SecureCookieAction::kInvalidUrl);
    return host;
  }
  if (host.has_scheme() && !host.SchemeIs(url::kHttpScheme)) {
    base::UmaHistogramEnumeration(kSecureCookieHistogramName,
                                  SecureCookieAction::kAlreadySecureScheme);
    return host;
  }
  if (!parsed_cookie.IsValid()) {
    base::UmaHistogramEnumeration(kSecureCookieHistogramName,
                                  SecureCookieAction::kInvalidCookie);
    return host;
  }
  if (!parsed_cookie.IsSecure()) {
    base::UmaHistogramEnumeration(kSecureCookieHistogramName,
                                  SecureCookieAction::kNotASecureCookie);
    return host;
  }

  LOG(ERROR) << "Strict Secure Cookie policy does not allow setting a "
                "secure cookie for "
             << host.spec()
             << " for apps targeting >= R. Please either use the 'https:' "
                "scheme for this URL or omit the 'Secure' directive in the "
                "cookie value.";
  if (!workaround_http_secure_cookies) {
    // Don't allow setting this cookie if we target >= R.
    *should_allow_cookie = false;
    base::UmaHistogramEnumeration(kSecureCookieHistogramName,
                                  SecureCookieAction::kDisallowedAndroidR);
    return host;
  }

  base::UmaHistogramEnumeration(kSecureCookieHistogramName,
                                SecureCookieAction::kFixedUp);
  GURL::Replacements replace_host;
  replace_host.SetSchemeStr(url::kHttpsScheme);
  return host.ReplaceComponents(replace_host);
}

// Construct a closure which signals a waitable event if and when the closure
// is called the waitable event must still exist.
static base::OnceClosure SignalEventClosure(WaitableEvent* completion) {
  return base::BindOnce(&WaitableEvent::Signal, base::Unretained(completion));
}

static void DiscardBool(base::OnceClosure f, bool b) {
  std::move(f).Run();
}

static base::OnceCallback<void(bool)> BoolCallbackAdapter(base::OnceClosure f) {
  return base::BindOnce(&DiscardBool, std::move(f));
}

static void DiscardInt(base::OnceClosure f, int i) {
  std::move(f).Run();
}

static base::OnceCallback<void(int)> IntCallbackAdapter(base::OnceClosure f) {
  return base::BindOnce(&DiscardInt, std::move(f));
}

// Are cookies allowed for file:// URLs by default?
const bool kDefaultFileSchemeAllowed = false;

}  // namespace

// static
CookieManager* CookieManager::GetDefaultInstance() {
  static base::NoDestructor<CookieManager> instance(nullptr);
  return instance.get();
}

namespace {
base::FilePath GetPathInAppDirectory(std::string path) {
  base::FilePath result;
  if (!base::PathService::Get(base::DIR_ANDROID_APP_DATA, &result)) {
    NOTREACHED() << "Failed to get app data directory for Android WebView";
  }
  result = result.Append(FILE_PATH_LITERAL(path));
  return result;
}
}  // namespace

CookieManager::CookieManager(AwBrowserContext* const parent_context)
    : parent_context_(parent_context),
      allow_file_scheme_cookies_(kDefaultFileSchemeAllowed),
      cookie_store_created_(false),
      workaround_http_secure_cookies_(
          base::android::BuildInfo::GetInstance()->target_sdk_version() <
          base::android::SDK_VERSION_R),
      cookie_store_client_thread_("CookieMonsterClient"),
      cookie_store_backend_thread_("CookieMonsterBackend"),
      setting_new_mojo_cookie_manager_(false) {
  cookie_store_client_thread_.Start();
  cookie_store_backend_thread_.Start();
  cookie_store_task_runner_ = cookie_store_client_thread_.task_runner();
  cookie_store_path_ = GetContextPath().Append(FILE_PATH_LITERAL("Cookies"));
  if (!parent_context_) {
    // Default profile
    MigrateCookieStorePath();
  }
}

CookieManager::~CookieManager() = default;

void CookieManager::MigrateCookieStorePath() {
  base::FilePath old_cookie_store_path = GetPathInAppDirectory("Cookies");
  base::FilePath old_cookie_journal_path =
      GetPathInAppDirectory("Cookies-journal");
  base::FilePath new_cookie_journal_path =
      GetPathInAppDirectory("Default/Cookies-journal");

  if (base::PathExists(old_cookie_store_path)) {
    base::CreateDirectory(cookie_store_path_.DirName());
    base::Move(old_cookie_store_path, cookie_store_path_);
    base::Move(old_cookie_journal_path, new_cookie_journal_path);
  }
}

// Executes the |task| on |cookie_store_task_runner_| and waits for it to
// complete before returning.
//
// To execute a CookieTask synchronously you must arrange for Signal to be
// called on the waitable event at some point. You can call the bool or int
// versions of ExecCookieTaskSync, these will supply the caller with a
// placeholder callback which takes an int/bool, throws it away and calls
// Signal. Alternatively you can call the version which supplies a Closure in
// which case you must call Run on it when you want the unblock the calling
// code.
//
// Ignore a bool callback.
void CookieManager::ExecCookieTaskSync(
    base::OnceCallback<void(base::OnceCallback<void(bool)>)> task) {
  WaitableEvent completion(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  ExecCookieTask(base::BindOnce(
      std::move(task), BoolCallbackAdapter(SignalEventClosure(&completion))));

  // Waiting is necessary when implementing synchronous APIs for the WebView
  // embedder.
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope wait;
  completion.Wait();
}

// Ignore an int callback.
void CookieManager::ExecCookieTaskSync(
    base::OnceCallback<void(base::OnceCallback<void(int)>)> task) {
  WaitableEvent completion(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  ExecCookieTask(base::BindOnce(
      std::move(task), IntCallbackAdapter(SignalEventClosure(&completion))));
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope wait;
  completion.Wait();
}

// Call the supplied closure when you want to signal that the blocked code can
// continue.
void CookieManager::ExecCookieTaskSync(
    base::OnceCallback<void(base::OnceClosure)> task) {
  WaitableEvent completion(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  ExecCookieTask(
      base::BindOnce(std::move(task), SignalEventClosure(&completion)));
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope wait;
  completion.Wait();
}

// Executes the |task| using |cookie_store_task_runner_|.
void CookieManager::ExecCookieTask(base::OnceClosure task) {
  base::AutoLock lock(task_queue_lock_);
  tasks_.push_back(std::move(task));
  // Unretained is safe, since android_webview::CookieManager is a singleton we
  // never destroy (we don't need PostTask to do any memory management).
  cookie_store_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&CookieManager::RunPendingCookieTasks,
                                base::Unretained(this)));
}

void CookieManager::RunPendingCookieTasks() {
  DCHECK(cookie_store_task_runner_->RunsTasksInCurrentSequence());
  // Don't do any cookie tasks if in the middle of setting a mojo CookieManager,
  // we'll call this method when that operation is finished.
  if (setting_new_mojo_cookie_manager_)
    return;

  // Copy tasks into temp_queue to minimize the amount of time in the critical
  // section, and to mitigate live-lock issues if these tasks append to the task
  // queue themselves.
  base::circular_deque<base::OnceClosure> temp_queue;
  {
    base::AutoLock lock(task_queue_lock_);
    temp_queue.swap(tasks_);
  }
  while (!temp_queue.empty()) {
    std::move(temp_queue.front()).Run();
    temp_queue.pop_front();
  }
}

base::FilePath CookieManager::GetCookieStorePath() {
  return cookie_store_path_;
}

net::CookieStore* CookieManager::GetCookieStore() {
  // This should only be called for the default context.
  CHECK(!parent_context_);
  DCHECK(cookie_store_task_runner_->RunsTasksInCurrentSequence());

  if (!cookie_store_) {
    content::CookieStoreConfig cookie_config(
        cookie_store_path_, /* restore_old_session_cookies= */ true,
        /* persist_session_cookies= */ true);
    cookie_config.client_task_runner = cookie_store_task_runner_;
    cookie_config.background_task_runner =
        cookie_store_backend_thread_.task_runner();

    {
      base::AutoLock lock(allow_file_scheme_cookies_lock_);

      // There are some unknowns about how to correctly handle file:// cookies,
      // and our implementation for this is not robust.  http://crbug.com/582985
      //
      // TODO(mmenke): This call should be removed once we can deprecate and
      // remove the Android WebView 'CookieManager::SetAllowFileSchemeCookies'
      // method. Until then, note that this is just not a great idea.
      cookie_config.cookieable_schemes.insert(
          cookie_config.cookieable_schemes.begin(),
          net::CookieMonster::kDefaultCookieableSchemes,
          net::CookieMonster::kDefaultCookieableSchemes +
              net::CookieMonster::kDefaultCookieableSchemesCount);
      if (allow_file_scheme_cookies_)
        cookie_config.cookieable_schemes.push_back(url::kFileScheme);
      cookie_store_created_ = true;
    }

    cookie_store_ =
        content::CreateCookieStore(std::move(cookie_config), nullptr);
    auto cookie_access_delegate_type =
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kWebViewEnableModernCookieSameSite)
            ? network::mojom::CookieAccessDelegateType::ALWAYS_NONLEGACY
            : network::mojom::CookieAccessDelegateType::ALWAYS_LEGACY;
    cookie_store_->SetCookieAccessDelegate(
        std::make_unique<network::CookieAccessDelegateImpl>(
            cookie_access_delegate_type, nullptr /* first_party_sets */));
  }

  return cookie_store_.get();
}

network::mojom::CookieManager* CookieManager::GetMojoCookieManager() {
  DCHECK(cookie_store_task_runner_->RunsTasksInCurrentSequence());
  if (!mojo_cookie_manager_.is_bound())
    return nullptr;
  return mojo_cookie_manager_.get();
}

void CookieManager::SetMojoCookieManager(
    mojo::PendingRemote<network::mojom::CookieManager> cookie_manager_remote) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ExecCookieTaskSync(base::BindOnce(&CookieManager::SetMojoCookieManagerAsync,
                                    base::Unretained(this),
                                    std::move(cookie_manager_remote)));
}

void CookieManager::SetMojoCookieManagerAsync(
    mojo::PendingRemote<network::mojom::CookieManager> cookie_manager_remote,
    base::OnceClosure complete) {
  DCHECK(cookie_store_task_runner_->RunsTasksInCurrentSequence());
  setting_new_mojo_cookie_manager_ = true;
  // For simplicity, only permit this method to be called once (otherwise, we
  // must sometimes flush the mojo_cookie_manager_ instead of cookie_store_).
  DCHECK(!mojo_cookie_manager_.is_bound());
  if (!cookie_store_created_) {
    SwapMojoCookieManagerAsync(std::move(cookie_manager_remote),
                               std::move(complete));
    return;
  }

  GetCookieStore()->FlushStore(base::BindOnce(
      &CookieManager::SwapMojoCookieManagerAsync, base::Unretained(this),
      std::move(cookie_manager_remote), std::move(complete)));
}

void CookieManager::SwapMojoCookieManagerAsync(
    mojo::PendingRemote<network::mojom::CookieManager> cookie_manager_remote,
    base::OnceClosure complete) {
  DCHECK(cookie_store_task_runner_->RunsTasksInCurrentSequence());
  mojo_cookie_manager_.Bind(std::move(cookie_manager_remote));
  setting_new_mojo_cookie_manager_ = false;
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&CookieManager::ClearClientHintsCachedPerOriginMapIfNeeded,
                     base::Unretained(this)));
  std::move(complete).Run();  // unblock content initialization
  RunPendingCookieTasks();
}

base::android::ScopedJavaLocalRef<jobject>
CookieManager::GetJavaCookieManager() {
  if (!java_obj_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    java_obj_ =
        Java_AwCookieManager_create(env, reinterpret_cast<intptr_t>(this));
  }
  return base::android::ScopedJavaLocalRef<jobject>(java_obj_);
}

void CookieManager::SetWorkaroundHttpSecureCookiesForTesting(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean allow) {
  ExecCookieTaskSync(
      base::BindOnce(&CookieManager::SetWorkaroundHttpSecureCookiesAsyncHelper,
                     base::Unretained(this), allow));
}

void CookieManager::SetWorkaroundHttpSecureCookiesAsyncHelper(
    bool allow,
    base::OnceClosure complete) {
  DCHECK(cookie_store_task_runner_->RunsTasksInCurrentSequence());
  workaround_http_secure_cookies_ = allow;
  std::move(complete).Run();
}

void CookieManager::SetShouldAcceptCookies(JNIEnv* env,
                                           const JavaParamRef<jobject>& obj,
                                           jboolean accept) {
  cookie_access_policy_.SetShouldAcceptCookies(accept);
}

jboolean CookieManager::GetShouldAcceptCookies(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return cookie_access_policy_.GetShouldAcceptCookies();
}

void CookieManager::SetCookie(JNIEnv* env,
                              const JavaParamRef<jobject>& obj,
                              const JavaParamRef<jstring>& url,
                              const JavaParamRef<jstring>& value,
                              const JavaParamRef<jobject>& java_callback) {
  DCHECK(java_callback) << "Unexpected null Java callback";
  GURL host(ConvertJavaStringToUTF16(env, url));
  std::string cookie_value(ConvertJavaStringToUTF8(env, value));
  base::OnceCallback<void(bool)> callback =
      base::BindOnce(&base::android::RunBooleanCallbackAndroid,
                     ScopedJavaGlobalRef<jobject>(java_callback));

  ExecCookieTask(base::BindOnce(&CookieManager::SetCookieHelper,
                                base::Unretained(this), host, cookie_value,
                                std::move(callback)));
}

void CookieManager::SetCookieSync(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj,
                                  const JavaParamRef<jstring>& url,
                                  const JavaParamRef<jstring>& value) {
  GURL host(ConvertJavaStringToUTF16(env, url));
  std::string cookie_value(ConvertJavaStringToUTF8(env, value));

  ExecCookieTaskSync(base::BindOnce(&CookieManager::SetCookieHelper,
                                    base::Unretained(this), host,
                                    cookie_value));
}

void CookieManager::SetCookieHelper(const GURL& host,
                                    const std::string& value,
                                    base::OnceCallback<void(bool)> callback) {
  DCHECK(cookie_store_task_runner_->RunsTasksInCurrentSequence());

  bool should_allow_cookie = true;
  const GURL& new_host = MaybeFixUpSchemeForSecureCookie(
      host, value, workaround_http_secure_cookies_, &should_allow_cookie);
  std::optional<net::CookiePartitionKey> cookie_partition_key =
      net::cookie_util::PartitionedCookiesDisabledByCommandLine()
          ? std::nullopt
          : std::make_optional(net::CookiePartitionKey::FromWire(
                net::SchemefulSite(new_host),
                net::CookiePartitionKey::AncestorChainBit::kSameSite));

  std::unique_ptr<net::CanonicalCookie> cc(net::CanonicalCookie::Create(
      new_host, value, base::Time::Now(), std::nullopt /* server_time */,
      cookie_partition_key, net::CookieSourceType::kOther,
      /*status=*/nullptr));

  if (!cc || !should_allow_cookie) {
    MaybeRunCookieCallback(std::move(callback), false);
    return;
  }

  // Note: CookieStore and network::CookieManager both accept a
  // CookieAccessResult callback. WebView only cares about boolean success,
  // which is why we use |AdaptCookieAccessResultToBool|. This is temporary
  // technical debt until we fully launch the Network Service code path.
  if (GetMojoCookieManager()) {
    // *cc.get() is safe, because network::CookieManager::SetCanonicalCookie
    // will make a copy before our smart pointer goes out of scope.
    GetMojoCookieManager()->SetCanonicalCookie(
        *cc.get(), new_host, net::CookieOptions::MakeAllInclusive(),
        base::BindOnce(net::cookie_util::IsCookieAccessResultInclude)
            .Then(std::move(callback)));
  } else {
    GetCookieStore()->SetCanonicalCookieAsync(
        std::move(cc), new_host, net::CookieOptions::MakeAllInclusive(),
        base::BindOnce(net::cookie_util::IsCookieAccessResultInclude)
            .Then(std::move(callback)));
  }
}

ScopedJavaLocalRef<jstring> CookieManager::GetCookie(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& url) {
  GURL host(ConvertJavaStringToUTF16(env, url));

  net::CookieList cookie_list;
  ExecCookieTaskSync(base::BindOnce(&CookieManager::GetCookieListAsyncHelper,
                                    base::Unretained(this), host,
                                    &cookie_list));

  return base::android::ConvertUTF8ToJavaString(
      env, net::CanonicalCookie::BuildCookieLine(cookie_list));
}

ScopedJavaLocalRef<jobjectArray> CookieManager::GetCookieInfo(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& url) {
  GURL host(ConvertJavaStringToUTF16(env, url));

  net::CookieList cookie_list;
  ExecCookieTaskSync(base::BindOnce(&CookieManager::GetCookieListAsyncHelper,
                                    base::Unretained(this), host,
                                    &cookie_list));
  std::vector<std::string> cookie_attributes;
  for (net::CanonicalCookie cookie : cookie_list) {
    cookie_attributes.push_back(
        net::CanonicalCookie::BuildCookieAttributesLine(cookie));
  }
  return base::android::ToJavaArrayOfStrings(
      env, base::span<const std::string>(cookie_attributes));
}

void CookieManager::GetCookieListAsyncHelper(const GURL& host,
                                             net::CookieList* result,
                                             base::OnceClosure complete) {
  net::CookieOptions options = net::CookieOptions::MakeAllInclusive();

  // TODO(crbug.com/40188414): Complete partitioned cookies implementation for
  // WebView. The current implementation is a temporary fix for
  // crbug.com/1442333 to let the app access its 1p partitioned cookie.
  if (GetMojoCookieManager()) {
    GetMojoCookieManager()->GetCookieList(
        host, options,
        net::CookiePartitionKeyCollection(net::CookiePartitionKey::FromWire(
            net::SchemefulSite(host),
            net::CookiePartitionKey::AncestorChainBit::kSameSite)),
        base::BindOnce(&CookieManager::GetCookieListCompleted,
                       base::Unretained(this), std::move(complete), result));
  } else {
    GetCookieStore()->GetCookieListWithOptionsAsync(
        host, options,
        net::CookiePartitionKeyCollection(net::CookiePartitionKey::FromWire(
            net::SchemefulSite(host),
            net::CookiePartitionKey::AncestorChainBit::kSameSite)),
        base::BindOnce(&CookieManager::GetCookieListCompleted,
                       base::Unretained(this), std::move(complete), result));
  }
}

void CookieManager::GetCookieListCompleted(
    base::OnceClosure complete,
    net::CookieList* result,
    const net::CookieAccessResultList& value,
    const net::CookieAccessResultList& excluded_cookies) {
  *result = net::cookie_util::StripAccessResults(value);
  std::move(complete).Run();
}

void CookieManager::RemoveSessionCookies(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& java_callback) {
  DCHECK(java_callback) << "Unexpected null Java callback";
  base::OnceCallback<void(bool)> callback =
      base::BindOnce(&base::android::RunBooleanCallbackAndroid,
                     ScopedJavaGlobalRef<jobject>(java_callback));

  ExecCookieTask(base::BindOnce(&CookieManager::RemoveSessionCookiesHelper,
                                base::Unretained(this), std::move(callback)));
}

void CookieManager::RemoveSessionCookiesSync(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj) {
  ExecCookieTaskSync(base::BindOnce(&CookieManager::RemoveSessionCookiesHelper,
                                    base::Unretained(this)));
}

void CookieManager::RemoveSessionCookiesHelper(
    base::OnceCallback<void(bool)> callback) {
  if (GetMojoCookieManager()) {
    auto match_session_cookies = network::mojom::CookieDeletionFilter::New();
    match_session_cookies->session_control =
        network::mojom::CookieDeletionSessionControl::SESSION_COOKIES;
    GetMojoCookieManager()->DeleteCookies(
        std::move(match_session_cookies),
        base::BindOnce(&CookieManager::RemoveCookiesCompleted,
                       base::Unretained(this), std::move(callback)));
  } else {
    GetCookieStore()->DeleteSessionCookiesAsync(
        base::BindOnce(&CookieManager::RemoveCookiesCompleted,
                       base::Unretained(this), std::move(callback)));
  }
}

void CookieManager::RemoveCookiesCompleted(
    base::OnceCallback<void(bool)> callback,
    uint32_t num_deleted) {
  std::move(callback).Run(num_deleted > 0u);
}

void CookieManager::RemoveAllCookies(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& java_callback) {
  DCHECK(java_callback) << "Unexpected null Java callback";

  base::OnceCallback<void(bool)> callback =
      base::BindOnce(&base::android::RunBooleanCallbackAndroid,
                     ScopedJavaGlobalRef<jobject>(java_callback));

  ExecCookieTask(base::BindOnce(&CookieManager::RemoveAllCookiesHelper,
                                base::Unretained(this), std::move(callback)));
}

void CookieManager::RemoveAllCookiesSync(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj) {
  ExecCookieTaskSync(base::BindOnce(&CookieManager::RemoveAllCookiesHelper,
                                    base::Unretained(this)));
}

void CookieManager::RemoveAllCookiesHelper(
    base::OnceCallback<void(bool)> callback) {
  // Clear client hints preferences when all cookies are cleared.
  should_clear_client_hints_cached_per_origin_map_ = true;
  if (GetMojoCookieManager()) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &CookieManager::ClearClientHintsCachedPerOriginMapIfNeeded,
            base::Unretained(this)));
    // An empty filter matches all cookies.
    auto match_all_cookies = network::mojom::CookieDeletionFilter::New();
    GetMojoCookieManager()->DeleteCookies(
        std::move(match_all_cookies),
        base::BindOnce(&CookieManager::RemoveCookiesCompleted,
                       base::Unretained(this), std::move(callback)));
  } else {
    // TODO(crbug.com/40609350): Support clearing client hints here as well.
    GetCookieStore()->DeleteAllAsync(
        base::BindOnce(&CookieManager::RemoveCookiesCompleted,
                       base::Unretained(this), std::move(callback)));
  }
}

void CookieManager::RemoveExpiredCookies(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj) {
  // HasCookies will call GetAllCookiesAsync, which in turn will force a GC.
  HasCookies(env, obj);
}

void CookieManager::FlushCookieStore(JNIEnv* env,
                                     const JavaParamRef<jobject>& obj) {
  ExecCookieTaskSync(base::BindOnce(&CookieManager::FlushCookieStoreAsyncHelper,
                                    base::Unretained(this)));
}

void CookieManager::FlushCookieStoreAsyncHelper(base::OnceClosure complete) {
  if (GetMojoCookieManager()) {
    GetMojoCookieManager()->FlushCookieStore(std::move(complete));
  } else {
    GetCookieStore()->FlushStore(std::move(complete));
  }
}

jboolean CookieManager::HasCookies(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj) {
  bool has_cookies;
  ExecCookieTaskSync(base::BindOnce(&CookieManager::HasCookiesAsyncHelper,
                                    base::Unretained(this), &has_cookies));
  return has_cookies;
}

// TODO(kristianm): Simplify this, copying the entire list around
// should not be needed.
void CookieManager::HasCookiesAsyncHelper(bool* result,
                                          base::OnceClosure complete) {
  if (GetMojoCookieManager()) {
    GetMojoCookieManager()->GetAllCookies(
        base::BindOnce(&CookieManager::HasCookiesCompleted,
                       base::Unretained(this), std::move(complete), result));
  } else {
    GetCookieStore()->GetAllCookiesAsync(
        base::BindOnce(&CookieManager::HasCookiesCompleted,
                       base::Unretained(this), std::move(complete), result));
  }
}

void CookieManager::HasCookiesCompleted(base::OnceClosure complete,
                                        bool* result,
                                        const CookieList& cookies) {
  *result = cookies.size() != 0;
  std::move(complete).Run();
}

bool CookieManager::GetAllowFileSchemeCookies() {
  base::AutoLock lock(allow_file_scheme_cookies_lock_);
  return allow_file_scheme_cookies_;
}

jboolean CookieManager::GetAllowFileSchemeCookies(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return GetAllowFileSchemeCookies();
}

void CookieManager::SetAllowFileSchemeCookies(JNIEnv* env,
                                              const JavaParamRef<jobject>& obj,
                                              jboolean allow) {
  ExecCookieTaskSync(
      base::BindOnce(&CookieManager::SetAllowFileSchemeCookiesAsyncHelper,
                     base::Unretained(this), allow));
}

void CookieManager::SetAllowFileSchemeCookiesAsyncHelper(
    bool allow,
    base::OnceClosure complete) {
  DCHECK(cookie_store_task_runner_->RunsTasksInCurrentSequence());
  if (GetMojoCookieManager()) {
    GetMojoCookieManager()->AllowFileSchemeCookies(
        allow,
        base::BindOnce(&CookieManager::SetAllowFileSchemeCookiesCompleted,
                       base::Unretained(this), std::move(complete), allow));
  } else {
    // If we have neither a Network Service CookieManager nor have created the
    // CookieStore, we may modify |allow_file_scheme_cookies_|.
    bool can_change_schemes = !cookie_store_created_;
    SetAllowFileSchemeCookiesCompleted(std::move(complete), allow,
                                       can_change_schemes);
  }
}

void CookieManager::SetAllowFileSchemeCookiesCompleted(
    base::OnceClosure complete,
    bool allow,
    bool can_change_schemes) {
  // Should only update |allow_file_scheme_cookies_| if
  // SetAllowFileSchemeCookiesAsyncHelper said this is OK.
  if (can_change_schemes) {
    base::AutoLock lock(allow_file_scheme_cookies_lock_);
    allow_file_scheme_cookies_ = allow;
  }
  std::move(complete).Run();
}

void CookieManager::ClearClientHintsCachedPerOriginMapIfNeeded() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // If we had a client hint cache clear pending, we should do it as soon as we
  // next check and see that the browser has been started.
  if (should_clear_client_hints_cached_per_origin_map_) {
    GetContext()->GetPrefService()->SetDict(
        prefs::kClientHintsCachedPerOriginMap, base::Value::Dict());
    should_clear_client_hints_cached_per_origin_map_ = false;
  }
}

static jlong JNI_AwCookieManager_GetDefaultCookieManager(JNIEnv* env) {
  return reinterpret_cast<intptr_t>(CookieManager::GetDefaultInstance());
}

AwBrowserContext* CookieManager::GetContext() const {
  if (parent_context_) {
    return parent_context_;
  } else {
    return AwBrowserContext::GetDefault();
  }
}

base::FilePath CookieManager::GetContextPath() const {
  if (parent_context_) {
    // Non-default profile
    return parent_context_->GetPath();
  } else {
    // Default profile
    return AwBrowserContext::BuildStoragePath(
        base::FilePath(AwBrowserContextStore::kDefaultContextPath));
  }
}

}  // namespace android_webview
