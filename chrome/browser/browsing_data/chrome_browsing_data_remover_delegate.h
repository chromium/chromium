// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_CHROME_BROWSING_DATA_REMOVER_DELEGATE_H_
#define CHROME_BROWSER_BROWSING_DATA_CHROME_BROWSING_DATA_REMOVER_DELEGATE_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "base/task/cancelable_task_tracker.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/buildflags.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/nacl/common/buildflags.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/browsing_data_remover_delegate.h"
#include "extensions/buildflags/buildflags.h"
#include "media/media_buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/network/public/mojom/network_context.mojom.h"

class Profile;
class ScopedProfileKeepAlive;
class WebappRegistry;

namespace content {
class BrowserContext;
}

namespace webrtc_event_logging {
class WebRtcEventLogManager;
}  // namespace webrtc_event_logging

// A delegate used by BrowsingDataRemover to delete data specific to Chrome
// as the embedder.
class ChromeBrowsingDataRemoverDelegate
    : public content::BrowsingDataRemoverDelegate,
      public KeyedService
{
 public:
  explicit ChromeBrowsingDataRemoverDelegate(
      content::BrowserContext* browser_context);
  ~ChromeBrowsingDataRemoverDelegate() override;

  // KeyedService:
  void Shutdown() override;

  // BrowsingDataRemoverDelegate:
  content::BrowsingDataRemoverDelegate::EmbedderOriginTypeMatcher
  GetOriginTypeMatcher() override;
  bool MayRemoveDownloadHistory() override;
  std::vector<std::string> GetDomainsForDeferredCookieDeletion(
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

#if defined(OS_ANDROID)
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
  // Entries must also be added to ChromeBrowsingDataRemoverTasks in enums.xml.
  enum class TracingDataType {
    kSynchronous = 1,
    kHistory = 2,
    kHostNameResolution = 3,
    kNaclCache = 4,
    kPnaclCache = 5,
    kAutofillData = 6,
    kAutofillOrigins = 7,
    kPluginData = 8,
    kFlashLsoHelper = 9,  // deprecated
    kDomainReliability = 10,
    kNetworkPredictor = 11,
    kWebrtcLogs = 12,
    kVideoDecodeHistory = 13,
    kCookies = 14,
    kPasswords = 15,
    kHttpAuthCache = 16,
    kDisableAutoSignin = 17,
    kPasswordsStatistics = 18,
    kKeywordsModel = 19,
    kReportingCache = 20,
    kNetworkErrorLogging = 21,
    kFlashDeauthorization = 22,
    kOfflinePages = 23,
    kPrecache = 24,
    kExploreSites = 25,
    kLegacyStrikes = 26,
    kWebrtcEventLogs = 27,
    kDrmLicenses = 28,
    kHostCache = 29,
    kTpmAttestationKeys = 30,
    kStrikes = 31,
    kLeakedCredentials = 32,  // deprecated
    kFieldInfo = 33,
    kCompromisedCredentials = 34,
    kUserDataSnapshot = 35,
    kMediaFeeds = 36,
    kAccountPasswords = 37,
    kAccountPasswordsSynced = 38,
    kAccountCompromisedCredentials = 39,
    kFaviconCacheExpiration = 40,
    kMaxValue = kFaviconCacheExpiration,
  };

  // Called by CreateTaskCompletionClosure().
  void OnTaskStarted(TracingDataType data_type);

  // Called by the closures returned by CreateTaskCompletionClosure().
  // Checks if all tasks have completed, and if so, calls callback_.
  void OnTaskComplete(TracingDataType data_type,
                      uint64_t data_type_mask,
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

  // The profile for which the data will be deleted.
  Profile* profile_;

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

#if defined(OS_ANDROID)
  // WebappRegistry makes calls across the JNI. In unit tests, the Java side is
  // not initialised, so the registry must be mocked out.
  std::unique_ptr<WebappRegistry> webapp_registry_;
#endif

  bool should_clear_password_account_storage_settings_ = false;

  base::WeakPtrFactory<ChromeBrowsingDataRemoverDelegate> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowsingDataRemoverDelegate);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_CHROME_BROWSING_DATA_REMOVER_DELEGATE_H_
