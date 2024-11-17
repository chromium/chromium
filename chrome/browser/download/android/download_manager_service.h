// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_MANAGER_SERVICE_H_
#define CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_MANAGER_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/download/android/download_open_source.h"
#include "chrome/browser/download/download_manager_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "components/download/public/common/all_download_event_notifier.h"
#include "components/download/public/common/in_progress_download_manager.h"
#include "content/public/browser/download_manager.h"

using base::android::JavaParamRef;

class Profile;
class ProfileKey;

namespace download {
class DownloadItem;
class SimpleDownloadManagerCoordinator;
}  // namespace download

// Native side of DownloadManagerService.java. The native object is owned by its
// Java object.
class DownloadManagerService
    : public download::AllDownloadEventNotifier::Observer,
      public ProfileObserver {
 public:
  static void CreateAutoResumptionHandler();

  static void OnDownloadCanceled(download::DownloadItem* download,
                                 bool has_no_external_storage);

  static DownloadManagerService* GetInstance();

  static base::android::ScopedJavaLocalRef<jobject> CreateJavaDownloadInfo(
      JNIEnv* env,
      download::DownloadItem* item);

  DownloadManagerService();

  DownloadManagerService(const DownloadManagerService&) = delete;
  DownloadManagerService& operator=(const DownloadManagerService&) = delete;

  ~DownloadManagerService() override;

  // Called to Initialize this object. If |is_profile_added| is false,
  // it means only a minimal browser is launched. OnProfileAdded() will
  // be called later when the profile is added.
  void Init(JNIEnv* env, jobject obj, bool is_profile_added);

  // Called when the profile is added to the ProfileManager and fully
  // initialized.
  void OnProfileAdded(JNIEnv* env, jobject obj, Profile* profile);

  void OnProfileAdded(Profile* profile);

  // Called to handle subsequent steps, after a download was determined as a OMA
  // download type.
  void HandleOMADownload(download::DownloadItem* download,
                         int64_t system_download_id);

  // Called to open a given download item.
  void OpenDownload(download::DownloadItem* download, int source);

  // Called to open a download item whose GUID is equal to |jdownload_guid|.
  void OpenDownload(JNIEnv* env,
                    jobject obj,
                    std::string& download_guid,
                    const JavaParamRef<jobject>& j_profile_key,
                    jint source);

  // Called to resume downloading the item that has GUID equal to
  // |jdownload_guid|..
  void ResumeDownload(JNIEnv* env,
                      jobject obj,
                      std::string& download_guid,
                      const JavaParamRef<jobject>& j_profile_key);

  // Called to cancel a download item that has GUID equal to |jdownload_guid|.
  // If the DownloadItem is not yet created, retry after a while.
  void CancelDownload(JNIEnv* env,
                      jobject obj,
                      std::string& download_guid,
                      const JavaParamRef<jobject>& j_profile_key);

  // Called to pause a download item that has GUID equal to |jdownload_guid|.
  // If the DownloadItem is not yet created, do nothing as it is already paused.
  void PauseDownload(JNIEnv* env,
                     jobject obj,
                     std::string& download_guid,
                     const JavaParamRef<jobject>& j_profile_key);

  // Called to remove a download item that has GUID equal to |jdownload_guid|.
  void RemoveDownload(JNIEnv* env,
                      jobject obj,
                      std::string& download_guid,
                      const JavaParamRef<jobject>& j_profile_key);

  // Called to rename a download item that has GUID equal to |id|.
  void RenameDownload(JNIEnv* env,
                      const JavaParamRef<jobject>& obj,
                      std::string& id,
                      std::string& name,
                      const JavaParamRef<jobject>& callback,
                      const JavaParamRef<jobject>& j_profile_key);

  // Returns whether or not the given download can be opened by the browser.
  bool IsDownloadOpenableInBrowser(JNIEnv* env,
                                   jobject obj,
                                   const JavaParamRef<jstring>& jdownload_guid,
                                   const JavaParamRef<jobject>& j_profile_key);

  // Called to request that the DownloadManagerService return data about all
  // downloads in the user's history.
  void GetAllDownloads(JNIEnv* env,
                       const JavaParamRef<jobject>& obj,
                       const JavaParamRef<jobject>& j_profile_key);

  // Called to check if the files associated with any downloads have been
  // removed by an external action.
  void CheckForExternallyRemovedDownloads(
      JNIEnv* env,
      const JavaParamRef<jobject>& obj,
      const JavaParamRef<jobject>& j_profile_key);

  // Called to update the last access time associated with a download.
  void UpdateLastAccessTime(JNIEnv* env,
                            const JavaParamRef<jobject>& obj,
                            std::string& download_guid,
                            const JavaParamRef<jobject>& j_profile_key);

  // AllDownloadEventNotifier::Observer methods.
  void OnDownloadsInitialized(
      download::SimpleDownloadManagerCoordinator* coordinator,
      bool active_downloads_only) override;
  void OnManagerGoingDown(
      download::SimpleDownloadManagerCoordinator* coordinator) override;
  void OnDownloadCreated(
      download::SimpleDownloadManagerCoordinator* coordinator,
      download::DownloadItem* item) override;
  void OnDownloadUpdated(
      download::SimpleDownloadManagerCoordinator* coordinator,
      download::DownloadItem* item) override;
  void OnDownloadRemoved(
      download::SimpleDownloadManagerCoordinator* coordinator,
      download::DownloadItem* item) override;

  // ProfileObserver:
  void OnOffTheRecordProfileCreated(Profile* off_the_record) override;

  // Called by the java code to create and insert an interrupted download to
  // |in_progress_manager_| for testing purpose.
  void CreateInterruptedDownloadForTest(JNIEnv* env,
                                        jobject obj,
                                        std::string& url,
                                        std::string& download_guid,
                                        std::string& target_path);

  // Retrives the in-progress manager and give up the ownership.
  std::unique_ptr<download::InProgressDownloadManager>
  RetrieveInProgressDownloadManager(content::BrowserContext* context);

  // Gets a download item from DownloadManager or InProgressManager.
  download::DownloadItem* GetDownload(const std::string& download_guid,
                                      ProfileKey* profile_key);

  // Helper method to record the interrupt reason UMA for the first background
  // download.
  void RecordFirstBackgroundInterruptReason(JNIEnv* env,
                                            const JavaParamRef<jobject>& obj,
                                            std::string& download_guid,
                                            jboolean download_started);

  // Open the download page the given profile, and the source of the opening
  // action is |download_open_source|.
  void OpenDownloadsPage(Profile* profile,
                         DownloadOpenSource download_open_source);

 private:
  // For testing.
  friend class DownloadManagerServiceTest;
  friend struct base::DefaultSingletonTraits<DownloadManagerService>;

  enum DownloadAction { RESUME, PAUSE, CANCEL, REMOVE, UNKNOWN };

  using PendingDownloadActions = std::map<std::string, DownloadAction>;
  using Coordinators = std::map<
      ProfileKey*,
      raw_ptr<download::SimpleDownloadManagerCoordinator, CtnExperimental>>;

  // Helper function to start the download resumption.
  void ResumeDownloadInternal(const std::string& download_guid,
                              ProfileKey* profile_key);

  // Helper function to cancel a download.
  void CancelDownloadInternal(const std::string& download_guid,
                              ProfileKey* profile_key);

  // Helper function to pause a download.
  void PauseDownloadInternal(const std::string& download_guid,
                             ProfileKey* profile_key);

  // Helper function to remove a download.
  void RemoveDownloadInternal(const std::string& download_guid,
                              ProfileKey* profile_key);

  // Helper function to send info about all downloads to the Java-side.
  void GetAllDownloadsInternal(ProfileKey* profile_key);

  // Called to notify the java side that download resumption failed.
  void OnResumptionFailed(const std::string& download_guid);

  void OnResumptionFailedInternal(const std::string& download_guid);

  // Called when all pending downloads are loaded.
  void OnPendingDownloadsLoaded();

  using ResumeCallback = base::OnceCallback<void(bool)>;
  void set_resume_callback_for_testing(ResumeCallback resume_cb) {
    resume_callback_for_testing_ = std::move(resume_cb);
  }

  // Helper method to reset the SimpleDownloadManagerCoordinator if needed.
  void ResetCoordinatorIfNeeded(ProfileKey* profile_key);

  // Helper method to reset the SimpleDownloadManagerCoordinator for a given
  // profile type.
  void UpdateCoordinator(
      download::SimpleDownloadManagerCoordinator* coordinator,
      ProfileKey* profile_key);

  // Called to get the content::DownloadManager instance.
  content::DownloadManager* GetDownloadManager(ProfileKey* profile_key);

  // Retrieves the SimpleDownloadManagerCoordinator this object is listening to.
  download::SimpleDownloadManagerCoordinator* GetCoordinator(
      ProfileKey* profile_key);

  void InitializeForProfile(ProfileKey* profile_key);

  // Reference to the Java object.
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  bool is_manager_initialized_;
  bool is_pending_downloads_loaded_;

  std::vector<raw_ptr<ProfileKey, VectorExperimental>>
      profiles_with_pending_get_downloads_actions_;

  PendingDownloadActions pending_actions_;

  void EnqueueDownloadAction(const std::string& download_guid,
                             DownloadAction download_action);

  ResumeCallback resume_callback_for_testing_;

  base::ScopedMultiSourceObservation<Profile, ProfileObserver>
      observed_profiles_{this};

  Coordinators coordinators_;
};

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_MANAGER_SERVICE_H_
