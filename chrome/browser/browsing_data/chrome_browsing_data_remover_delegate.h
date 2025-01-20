// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_CHROME_BROWSING_DATA_REMOVER_DELEGATE_H_
#define CHROME_BROWSER_BROWSING_DATA_CHROME_BROWSING_DATA_REMOVER_DELEGATE_H_

#include <memory>

#include "base/cancelable_callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/buildflags.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/nacl/common/buildflags.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/browsing_data_remover_delegate.h"
#include "device/fido/platform_credential_store.h"
#include "extensions/buildflags/buildflags.h"
#include "media/media_buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/network/public/mojom/network_context.mojom.h"

class Profile;
class ScopedProfileKeepAlive;
class WebappRegistry;

namespace base {
class WaitableEvent;
}

namespace content {
class BrowserContext;
class StoragePartition;
}

namespace webrtc_event_logging {
class WebRtcEventLogManager;
}  // namespace webrtc_event_logging

// A delegate used by BrowsingDataRemover to delete data specific to Chrome
// as the embedder.
class ChromeBrowsingDataRemoverDelegate
    : public content::BrowsingDataRemoverDelegate,
      public KeyedService {
 public:
  explicit ChromeBrowsingDataRemoverDelegate(
      content::BrowserContext* browser_context);

  ChromeBrowsingDataRemoverDelegate(const ChromeBrowsingDataRemoverDelegate&) =
      delete;
  ChromeBrowsingDataRemoverDelegate& operator=(
      const ChromeBrowsingDataRemoverDelegate&) = delete;

  ~ChromeBrowsingDataRemoverDelegate() override;

  // KeyedService:
  void Shutdown() override;

  // BrowsingDataRemoverDelegate:
  content::BrowsingDataRemoverDelegate::EmbedderOriginTypeMatcher
  GetOriginTypeMatcher() override;
  bool MayRemoveDownloadHistory() override;
  std::vector<std::string> GetDomainsForDeferredCookieDeletion(
      content::StoragePartition* storage_partition,
      uint64_t remove_mask) override;
  void RemoveEmbedderData(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      uint64_t remove_mask,
      content::BrowsingDataFilterBuilder* filter_builder,
      uint64_t origin_type_mask,
      base::OnceCallback<void(/*failed_data_types=*/uint64_t)> callback)
      override;
  void OnStartRemoving() override;
  void OnDoneRemoving() override;

#if BUILDFLAG(IS_ANDROID)
  void OverrideWebappRegistryForTesting(
      std::unique_ptr<WebappRegistry> webapp_registry);
#endif

  using DomainReliabilityClearer = base::RepeatingCallback<void(
      content::BrowsingDataFilterBuilder* filter_builder,
      network::mojom::NetworkContext_DomainReliabilityClearMode,
      network::mojom::NetworkContext::ClearDomainReliabilityCallback)>;
  void OverrideDomainReliabilityClearerForTesting(
      DomainReliabilityClearer clearer);

 private:
  using WebRtcEventLogManager = webrtc_event_logging::WebRtcEventLogManager;

  // For debugging purposes. Please add new deletion tasks at the end.
  // This enum is recorded in a histogram, so don't change or reuse ids.
  // LINT.IfChange(TracingDataType)
  enum class TracingDataType {
    kSynchronous = 1,
    kHistory = 2,
    // kHostNameResolution = 3, deprecated
    kNaclCache = 4,
    kPnaclCache = 5,
    kAutofillData = 6,
    kAutofillOrigins = 7,
    // kPluginData = 8, deprecated
    // kFlashLsoHelper = 9, deprecated
    kDomainReliability = 10,
    // kNetworkPredictor = 11, deprecated
    kWebrtcLogs = 12,
    kVideoDecodeHistory = 13,
    kCookies = 14,
    kPasswords = 15,
    kHttpAuthCache = 16,
    // See also kDisableAutoSigninForAccountPasswords.
    kDisableAutoSigninForProfilePasswords = 17,
    kPasswordsStatistics = 18,
    // kKeywordsModel = 19, deprecated
    kReportingCache = 20,
    kNetworkErrorLogging = 21,
    // kFlashDeauthorization = 22, deprecated
    kOfflinePages = 23,
    // kPrecache = 24, deprecated
    // kExploreSites = 25, deprecated
    // kLegacyStrikes = 26, deprecated
    kWebrtcEventLogs = 27,
    kCdmLicenses = 28,
    kHostCache = 29,
    kTpmAttestationKeys = 30,
    // kStrikes = 31, deprecated
    // kLeakedCredentials = 32, deprecated
    // kFieldInfo = 33, deprecated
    // kCompromisedCredentials = 34, deprecated
    kUserDataSnapshot = 35,
    // kMediaFeeds = 36, deprecated
    kAccountPasswords = 37,
    kAccountPasswordsSynced = 38,
    // kAccountCompromisedCredentials = 39, deprecated
    kFaviconCacheExpiration = 40,
    kSecurePaymentConfirmationCredentials = 41,
    kWebAppHistory = 42,
    kWebAuthnCredentials = 43,
    kWebrtcVideoPerfHistory = 44,
    kMediaDeviceSalts = 45,
    // See also kDisableAutoSigninForProfilePasswords.
    kDisableAutoSigninForAccountPasswords = 46,

    kMaxValue = kDisableAutoSigninForAccountPasswords,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/history/enums.xml:ChromeBrowsingDataRemoverTasks)

  // Returns the suffix for the
  // History.ClearBrowsingData.Duration.ChromeTask.{Task} histogram
  const char* GetHistogramSuffix(TracingDataType task);

  // Called by CreateTaskCompletionClosure().
  void OnTaskStarted(TracingDataType data_type);

  // Called by the closures returned by CreateTaskCompletionClosure().
  // Checks if all tasks have completed, and if so, calls callback_.
  void OnTaskComplete(TracingDataType data_type,
                      uint64_t data_type_mask,
                      base::TimeTicks started,
                      bool success);

  // Increments the number of pending tasks by one, and returns a OnceClosure
  // that calls OnTaskComplete(). The Remover is complete once all the closures
  // created by this method have been invoked.
  base::OnceClosure CreateTaskCompletionClosure(TracingDataType data_type);
  // Like CreateTaskCompletionClosure(), but allows tracking success/failure of
  // the task. If |success = false| is passed to the callback, |data_type_mask|
  // will be added to |failed_data_types_|.
  base::OnceCallback<void(bool /* success */)> CreateTaskCompletionCallback(
      TracingDataType data_type,
      uint64_t data_type_mask);

  // Same as CreateTaskCompletionClosure() but guarantees that
  // OnTaskComplete() is called if the task is dropped. That can typically
  // happen when the connection is closed while an interface call is made.
  base::OnceClosure CreateTaskCompletionClosureForMojo(
      TracingDataType data_type);

  // Records unfinished tasks from |pending_sub_tasks_| after a delay.
  void RecordUnfinishedSubTasks();

  // A helper method that checks if time period is for "all time".
  bool IsForAllTime() const;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void OnClearPlatformKeys(base::OnceClosure done, bool);
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
  // Called when plugin data has been cleared. Invokes NotifyIfDone.
  void OnWaitableEventSignaled(base::OnceClosure done,
                               base::WaitableEvent* waitable_event);
#endif

  std::unique_ptr<device::fido::PlatformCredentialStore> MakeCredentialStore();

  // See `deferred_disable_passwords_auto_signin_cb_`.
  void DisablePasswordsAutoSignin(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter);

  // The profile for which the data will be deleted.
  raw_ptr<Profile> profile_;

  // Prevents |profile_| from getting deleted. Only active between
  // OnStartRemoving() and OnDoneRemoving(), i.e. while there are tasks in
  // progress.
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;

  // Start time to delete from.
  base::Time delete_begin_;

  // End time to delete to.
  base::Time delete_end_;

  // Completion callback to call when all data are deleted.
  base::OnceCallback<void(uint64_t)> callback_;

  // Records which tasks of a deletion are currently active.
  std::set<TracingDataType> pending_sub_tasks_;

  uint64_t failed_data_types_ = 0;

  // Fires after some time to track slow tasks. Cancelled when all tasks
  // are finished.
  base::CancelableOnceClosure slow_pending_tasks_closure_;

  DomainReliabilityClearer domain_reliability_clearer_;

  // Used if we need to clear history.
  base::CancelableTaskTracker history_task_tracker_;

#if BUILDFLAG(IS_ANDROID)
  // WebappRegistry makes calls across the JNI. In unit tests, the Java side is
  // not initialised, so the registry must be mocked out.
  std::unique_ptr<WebappRegistry> webapp_registry_;
#endif

#if !BUILDFLAG(IS_ANDROID)
  // On desktop, some per-account sync settings must be cleared when cookies are
  // deleted. This flag is used to defer the process until after sync uploads
  // deletions of any other data.
  bool should_clear_sync_account_settings_ = false;
#endif

  // PasswordStore::DisableAutoSignInForOrigins() is required when wiping
  // DATA_TYPE_COOKIES, but that must be deferred until any password deletions
  // have completed, to avoid resurrecting passwords (c.f. crbug.com/325323180).
  // This field serves that: it'll be executed in OnTasksComplete() when all
  // other tasks are done. Executing it adds to `pending_sub_tasks_` again.
  // OnBrowsingDataRemoverDone() is only called after the (async) auto-signin
  // disabling has completed.
  // This field is similar to `should_clear_sync_account_settings_` above,
  // except that clearing settings is synchronous, disabling auto sign-in isn't.
  base::OnceClosure deferred_disable_passwords_auto_signin_cb_;

  std::unique_ptr<device::fido::PlatformCredentialStore> credential_store_;

  base::WeakPtrFactory<ChromeBrowsingDataRemoverDelegate> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_BROWSING_DATA_CHROME_BROWSING_DATA_REMOVER_DELEGATE_H_
