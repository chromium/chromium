// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_UPDATER_H_
#define CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_UPDATER_H_

#include <list>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <set>
#include <string>

#include "base/auto_reset.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/updater/extension_downloader.h"
#include "extensions/browser/updater/extension_downloader_delegate.h"
#include "extensions/browser/updater/extension_downloader_types.h"
#include "extensions/browser/updater/extension_update_data.h"
#include "extensions/browser/updater/update_service.h"
#include "extensions/common/extension_id.h"
#include "url/gurl.h"

class PrefService;
class Profile;
class ScopedProfileKeepAlive;

namespace extensions {

class CrxInstallError;
class CrxInstaller;
class ExtensionCache;
class ExtensionPrefs;
class ExtensionRegistry;
class ExtensionServiceInterface;
class ExtensionSet;
struct ExtensionUpdateCheckParams;
class ExtensionUpdaterTest;

// A class for doing auto-updates of installed Extensions. Used like this:
//
// std::unique_ptr<ExtensionUpdater> updater =
//    std::make_unique<ExtensionUpdater>(my_extensions_service,
//                                       extension_prefs,
//                                       pref_service,
//                                       profile,
//                                       update_frequency_secs,
//                                       downloader_factory);
// updater->Start();
// ....
// updater->Stop();
class ExtensionUpdater : public ExtensionDownloaderDelegate {
 public:
  using FinishedCallback = base::OnceClosure;

  struct CheckParams {
    // Creates a default CheckParams instance that checks for all extensions.
    CheckParams();
    ~CheckParams();

    CheckParams(const CheckParams& other) = delete;
    CheckParams& operator=(const CheckParams& other) = delete;

    CheckParams(CheckParams&& other);
    CheckParams& operator=(CheckParams&& other);

    // The set of extensions that should be checked for updates. If empty
    // all extensions will be included in the update check.
    std::list<ExtensionId> ids;

    // Normally extension updates get installed only when the extension is idle.
    // Setting this to true causes any updates that are found to be installed
    // right away.
    bool install_immediately = false;

    // An extension update check can be originated by a user or by a scheduled
    // task. When the value of |fetch_priority| is FOREGROUND, the update
    // request was initiated by a user.
    DownloadFetchPriority fetch_priority = DownloadFetchPriority::kBackground;

    // If set, will be called when an update is found and before an attempt to
    // download and install it is made.
    UpdateFoundCallback update_found_callback;

    // Callback to call when the update check is complete. Can be null, if
    // you're not interested in when this happens.
    FinishedCallback callback;
  };

  // A class for use in tests to skip scheduled update checks for extensions
  // during the lifetime of an instance of it. Only one instance should be alive
  // at any given time.
  class ScopedSkipScheduledCheckForTest {
   public:
    ScopedSkipScheduledCheckForTest();

    ScopedSkipScheduledCheckForTest(const ScopedSkipScheduledCheckForTest&) =
        delete;
    ScopedSkipScheduledCheckForTest& operator=(
        const ScopedSkipScheduledCheckForTest&) = delete;

    ~ScopedSkipScheduledCheckForTest();
  };

  // Holds a pointer to the passed |service|, using it for querying installed
  // extensions and installing updated ones. The |frequency_seconds| parameter
  // controls how often update checks are scheduled.
  ExtensionUpdater(ExtensionServiceInterface* service,
                   ExtensionPrefs* extension_prefs,
                   PrefService* prefs,
                   Profile* profile,
                   int frequency_seconds,
                   ExtensionCache* cache,
                   const ExtensionDownloader::Factory& downloader_factory);

  ExtensionUpdater(const ExtensionUpdater&) = delete;
  ExtensionUpdater& operator=(const ExtensionUpdater&) = delete;
  ~ExtensionUpdater() override;

  // Starts the updater running.  Should be called at most once.
  void Start();

  // Stops the updater running, cancelling any outstanding update manifest and
  // crx downloads. Does not cancel any in-progress installs.
  void Stop();

  // Posts a task to do an update check.  Does nothing if there is
  // already a pending task that has not yet run.
  void CheckSoon();

  // Starts an update check right now, instead of waiting for the next
  // regularly scheduled check or a pending check from CheckSoon().
  void CheckNow(CheckParams params);

  // Returns true iff CheckSoon() has been called but the update check
  // hasn't been performed yet.  This is used mostly by tests; calling
  // code should just call CheckSoon().
  bool WillCheckSoon() const;

  // Overrides the extension cache with |extension_cache| for testing.
  void SetExtensionCacheForTesting(ExtensionCache* extension_cache);

  // Overrides the extension downloader with |downloader| for testing.
  void SetExtensionDownloaderForTesting(
      std::unique_ptr<ExtensionDownloader> downloader);

  // After this is called, the next ExtensionUpdater instance to be started will
  // call CheckNow() instead of CheckSoon() for its initial update.
  static void UpdateImmediatelyForFirstRun();

  // For testing, changes the backoff policy for ExtensionDownloader's manifest
  // queue to get less initial delay and the tests don't time out.
  void SetBackoffPolicyForTesting(
      const net::BackoffEntry::Policy& backoff_policy);

  // Always fetch updates via update service, not the extension downloader.
  static base::AutoReset<bool> GetScopedUseUpdateServiceForTesting();

  // Set a callback to invoke when updating has started.
  void SetUpdatingStartedCallbackForTesting(base::RepeatingClosure callback);

  // A callback that is invoked when the next invocation of CxrInstaller
  // finishes (successfully or not).
  void SetCrxInstallerResultCallbackForTesting(
      ExtensionSystem::InstallUpdateCallback callback);

 private:
  friend class ExtensionUpdaterTest;
  friend class ExtensionUpdaterFileHandler;

  // FetchedCRXFile holds information about a CRX file we fetched to disk,
  // but have not yet installed.
  struct FetchedCRXFile {
    FetchedCRXFile();
    FetchedCRXFile(const CRXFileInfo& file,
                   bool file_ownership_passed,
                   const std::set<int>& request_ids,
                   InstallCallback callback);
    FetchedCRXFile(FetchedCRXFile&& other);
    FetchedCRXFile& operator=(FetchedCRXFile&& other);
    ~FetchedCRXFile();

    CRXFileInfo info;
    GURL download_url;
    bool file_ownership_passed;
    std::set<int> request_ids;
    InstallCallback callback;
    scoped_refptr<CrxInstaller> installer;
  };

  struct InProgressCheck {
    InProgressCheck();
    InProgressCheck(const InProgressCheck&) = delete;
    InProgressCheck& operator=(const InProgressCheck&) = delete;
    ~InProgressCheck();

    bool install_immediately = false;
    bool awaiting_update_service = false;
    UpdateFoundCallback update_found_callback;
    FinishedCallback callback;
    // Prevents the destruction of the Profile* while an update check is in
    // progress.
    // TODO(crbug.com/40174537): Find a way to pass the keepalive to
    // UpdateClient instead of holding it here.
    std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive;
    // The ids of extensions that have in-progress update checks.
    std::set<ExtensionId> in_progress_ids;
  };

  // Ensure that we have a valid ExtensionDownloader instance referenced by
  // |downloader|.
  void EnsureDownloaderCreated();

  // Schedules a task to call NextCheck after |frequency_| delay, plus
  // or minus 0 to 20% (to help spread load evenly on servers).
  void ScheduleNextCheck();

  // Add fetch records for extensions that are installed to the downloader,
  // ignoring |pending_ids| so the extension isn't fetched again.
  void AddToDownloader(const ExtensionSet* extensions,
                       const std::set<ExtensionId>& pending_ids,
                       int request_id,
                       DownloadFetchPriority fetch_priority,
                       ExtensionUpdateCheckParams* update_check_params);

  // Adds |extension| to the downloader, providing it with |fetch_priority|,
  // |request_id| and data extracted from the extension object.
  // |fetch_priority| parameter notifies the downloader the priority of this
  // extension update (either foreground or background).
  bool AddExtensionToDownloader(const Extension& extension,
                                int request_id,
                                DownloadFetchPriority fetch_priority);

  // Conduct a check as scheduled by ScheduleNextCheck.
  void NextCheck();

  // Posted by CheckSoon().
  void DoCheckSoon();

  // Implementation of ExtensionDownloaderDelegate.
  void OnExtensionDownloadStageChanged(const ExtensionId& id,
                                       Stage stage) override;
  void OnExtensionUpdateFound(const ExtensionId& id,
                              const std::set<int>& request_ids,
                              const base::Version& version) override;
  void OnExtensionDownloadCacheStatusRetrieved(const ExtensionId& id,
                                               CacheStatus status) override;
  void OnExtensionDownloadFailed(const ExtensionId& id,
                                 Error error,
                                 const PingResult& ping,
                                 const std::set<int>& request_ids,
                                 const FailureData& data) override;
  void OnExtensionDownloadRetry(const ExtensionId& id,
                                const FailureData& data) override;
  void OnExtensionDownloadFinished(const CRXFileInfo& file,
                                   bool file_ownership_passed,
                                   const GURL& download_url,
                                   const PingResult& ping,
                                   const std::set<int>& request_id,
                                   InstallCallback callback) override;
  bool GetPingDataForExtension(const ExtensionId& id,
                               DownloadPingData* ping_data) override;
  bool IsExtensionPending(const ExtensionId& id) override;
  bool GetExtensionExistingVersion(const ExtensionId& id,
                                   std::string* version) override;

  // Returns an `ExtensionUpdateData` prepopulated with the `pending_version`
  // and `pending_fingerprint` if there is a pending extension update.
  ExtensionUpdateData GetExtensionUpdateData(const ExtensionId& id);

  void UpdatePingData(const ExtensionId& id, const PingResult& ping_result);

  // Starts installing a crx file that has been fetched but not installed yet.
  void InstallCRXFile(FetchedCRXFile crx_file);

  // Send a notification that update checks are starting.
  void NotifyStarted();

  // Send a notification if we're finished updating.
  void NotifyIfFinished(int request_id);

  // |udpate_service_| will execute this function on finish.
  void OnUpdateServiceFinished(int request_id);

  void ExtensionCheckFinished(const ExtensionId& extension_id,
                              FinishedCallback callback);

  // Callback set in the crx installer and invoked when the crx file has passed
  // the expectations check. It takes the ownership of the file pointed to by
  // |crx_info|.
  void PutExtensionInCache(const CRXFileInfo& crx_info);

  // Deletes the crx file at |crx_path| if ownership is passed.
  void CleanUpCrxFileIfNeeded(const base::FilePath& crx_path,
                              bool file_ownership_passed);

  void OnInstallerDone(const base::UnguessableToken& token,
                       const std::optional<CrxInstallError>& error);

  // This function verifies if |extension_id| can be updated using
  // UpdateService.
  bool CanUseUpdateService(const ExtensionId& extension_id) const;

  // Whether Start() has been called but not Stop().
  bool alive_ = false;

  // Pointer back to the service that owns this ExtensionUpdater.
  raw_ptr<ExtensionServiceInterface> service_ = nullptr;

  // A closure passed into the ExtensionUpdater to teach it how to construct
  // new ExtensionDownloader instances.
  const ExtensionDownloader::Factory downloader_factory_;

  // Fetches the crx files for the extensions that have an available update.
  std::unique_ptr<ExtensionDownloader> downloader_;

  // Update service is responsible for updating Webstore extensions.
  // Note that |UpdateService| is a KeyedService class, which can only be
  // created through a |KeyedServiceFactory| singleton, thus |update_service_|
  // will be freed by the same factory singleton before the browser is
  // shutdown.
  raw_ptr<UpdateService> update_service_ = nullptr;

  base::TimeDelta frequency_;
  bool will_check_soon_ = false;

  raw_ptr<ExtensionPrefs, DanglingUntriaged> extension_prefs_ = nullptr;
  raw_ptr<PrefService, DanglingUntriaged> prefs_ = nullptr;
  raw_ptr<Profile, DanglingUntriaged> profile_ = nullptr;

  raw_ptr<ExtensionRegistry, DanglingUntriaged> registry_ = nullptr;

  std::map<int, InProgressCheck> requests_in_progress_;
  int next_request_id_ = 0;

  // CRX installs that are currently in progress. Used to get the FetchedCRXFile
  // when OnInstallerDone is called.
  std::map<base::UnguessableToken, FetchedCRXFile> running_crx_installs_;

  raw_ptr<ExtensionCache, DanglingUntriaged> extension_cache_ = nullptr;

  base::RepeatingClosure updating_started_callback_;

  ExtensionSystem::InstallUpdateCallback installer_result_callback_for_testing_;

  base::WeakPtrFactory<ExtensionUpdater> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_UPDATER_H_
