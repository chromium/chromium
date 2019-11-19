// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_MANAGER_SERVICE_H_
#define CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_MANAGER_SERVICE_H_

#include <map>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "chrome/browser/download/download_manager_utils.h"
#include "components/download/public/common/all_download_event_notifier.h"
#include "components/download/public/common/in_progress_download_manager.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

using base::android::JavaParamRef;

class ProfileKey;

namespace download {
class DownloadItem;
class SimpleDownloadManagerCoordinator;
}  // namespace download

// Native side of DownloadManagerService.java. The native object is owned by its
// Java object.
class DownloadManagerService
    : public download::AllDownloadEventNotifier::Observer,
      public content::NotificationObserver {
 public:
  static void CreateAutoResumptionHandler();

  static void OnDownloadCanceled(download::DownloadItem* download,
                                 bool has_no_external_storage);

  static DownloadManagerService* GetInstance();

  static base::android::ScopedJavaLocalRef<jobject> CreateJavaDownloadInfo(
      JNIEnv* env,
      download::DownloadItem* item);

  DownloadManagerService();
  ~DownloadManagerService() override;

  // Called to Initialize this object. If |is_profile_created| is false,
  // it means only the service manager is launched. OnProfileCreated() will
  // be called later when the profile is created.
  void Init(JNIEnv* env, jobject obj, bool is_profile_created);

  // Called when the prfile is created.
  void OnProfileCreated(JNIEnv* env, jobject obj);

  // Called to handle subsequent steps, after a download was determined as a OMA
  // download type.
  void HandleOMADownload(download::DownloadItem* download,
                         int64_t system_download_id);

  // Called to open a given download item.
  void OpenDownload(download::DownloadItem* download, int source);

  // Called to open a download item whose GUID is equal to |jdownload_guid|.
  void OpenDownload(JNIEnv* env,
                    jobject obj,
                    const JavaParamRef<jstring>& jdownload_guid,
                    bool is_off_the_record,
                    jint source);

  // Called to resume downloading the item that has GUID equal to
  // |jdownload_guid|..
  void ResumeDownload(JNIEnv* env,
                      jobject obj,
                      const JavaParamRef<jstring>& jdownload_guid,
                      bool is_off_the_record,
                      bool has_user_gesture);

  // Called to retry a download.
  void RetryDownload(JNIEnv* env,
                     jobject obj,
                     const JavaParamRef<jstring>& jdownload_guid,
                     bool is_off_the_record,
                     bool has_user_gesture);

  // Called to cancel a download item that has GUID equal to |jdownload_guid|.
  // If the DownloadItem is not yet created, retry after a while.
  void CancelDownload(JNIEnv* env,
                      jobject obj,
                      const JavaParamRef<jstring>& jdownload_guid,
                      bool is_off_the_record);

  // Called to pause a download item that has GUID equal to |jdownload_guid|.
  // If the DownloadItem is not yet created, do nothing as it is already paused.
  void PauseDownload(JNIEnv* env,
                     jobject obj,
                     const JavaParamRef<jstring>& jdownload_guid,
                     bool is_off_the_record);

  // Called to remove a download item that has GUID equal to |jdownload_guid|.
  void RemoveDownload(JNIEnv* env,
                      jobject obj,
                      const JavaParamRef<jstring>& jdownload_guid,
                      bool is_off_the_record);

  // Called to rename a download item that has GUID equal to |id|.
  void RenameDownload(JNIEnv* env,
                      const JavaParamRef<jobject>& obj,
                      const JavaParamRef<jstring>& id,
                      const JavaParamRef<jstring>& name,
                      const JavaParamRef<jobject>& callback,
                      bool is_off_the_record);

  // Returns whether or not the given download can be opened by the browser.
  bool IsDownloadOpenableInBrowser(JNIEnv* env,
                                   jobject obj,
                                   const JavaParamRef<jstring>& jdownload_guid,
                                   bool is_off_the_record);

  // Called to request that the DownloadManagerService return data about all
  // downloads in the user's history.
  void GetAllDownloads(JNIEnv* env,
                       const JavaParamRef<jobject>& obj,
                       bool is_off_the_record);

  // Called to check if the files associated with any downloads have been
  // removed by an external action.
  void CheckForExternallyRemovedDownloads(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj,
                                          bool is_off_the_record);

  // Called to update the last access time associated with a download.
  void UpdateLastAccessTime(JNIEnv* env,
                            const JavaParamRef<jobject>& obj,
                            const JavaParamRef<jstring>& jdownload_guid,
                            bool is_off_the_record);

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

  // content::NotificationObserver methods.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Called by the java code to create and insert an interrupted download to
  // |in_progress_manager_| for testing purpose.
  void CreateInterruptedDownloadForTest(
      JNIEnv* env,
      jobject obj,
      const JavaParamRef<jstring>& jurl,
      const JavaParamRef<jstring>& jdownload_guid,
      const JavaParamRef<jstring>& jtarget_path);

  // Retrives the in-progress manager and give up the ownership.
  download::InProgressDownloadManager* RetriveInProgressDownloadManager(
      content::BrowserContext* context);

  // Gets a download item from DownloadManager or InProgressManager.
  download::DownloadItem* GetDownload(const std::string& download_guid,
                                      bool is_off_the_record);

  // Helper method to record the interrupt reason UMA for the first background
  // download.
  void RecordFirstBackgroundInterruptReason(
      JNIEnv* env,
      const JavaParamRef<jobject>& obj,
      const JavaParamRef<jstring>& jdownload_guid,
      jboolean download_started);

 private:
  // For testing.
  friend class DownloadManagerServiceTest;
  friend struct base::DefaultSingletonTraits<DownloadManagerService>;

  // Helper function to start the download resumption.
  void ResumeDownloadInternal(const std::string& download_guid,
                              bool is_off_the_record,
                              bool has_user_gesture);

  // Helper function to retry the download.
  void RetryDownloadInternal(const std::string& download_guid,
                             bool is_off_the_record,
                             bool has_user_gesture);

  // Helper function to cancel a download.
  void CancelDownloadInternal(const std::string& download_guid,
                              bool is_off_the_record);

  // Helper function to pause a download.
  void PauseDownloadInternal(const std::string& download_guid,
                             bool is_off_the_record);

  // Helper function to remove a download.
  void RemoveDownloadInternal(const std::string& download_guid,
                              bool is_off_the_record);

  // Helper function to send info about all downloads to the Java-side.
  void GetAllDownloadsInternal(bool is_off_the_record);

  // Called to notify the java side that download resumption failed.
  void OnResumptionFailed(const std::string& download_guid);

  void OnResumptionFailedInternal(const std::string& download_guid);

  // Called when all pending downloads are loaded.
  void OnPendingDownloadsLoaded();

  typedef base::Callback<void(bool)> ResumeCallback;
  void set_resume_callback_for_testing(const ResumeCallback& resume_cb) {
    resume_callback_for_testing_ = resume_cb;
  }

  // Helper method to reset the SimpleDownloadManagerCoordinator if needed.
  void ResetCoordinatorIfNeeded(ProfileKey* profile_key);

  // Helper method to reset the SimpleDownloadManagerCoordinator for a given
  // profile type.
  void UpdateCoordinator(
      download::SimpleDownloadManagerCoordinator* coordinator,
      bool is_off_the_record);

  // Called to get the content::DownloadManager instance.
  content::DownloadManager* GetDownloadManager(bool is_off_the_record);

  // Retrieves the SimpleDownloadManagerCoordinator this object is listening to.
  download::SimpleDownloadManagerCoordinator* GetCoordinator(
      bool is_off_the_record);

  // Reference to the Java object.
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  bool is_manager_initialized_;
  bool is_pending_downloads_loaded_;

  enum PendingGetDownloadsFlags {
    NONE = 0,
    REGULAR = 1 << 0,
    OFF_THE_RECORD = 1 << 1,
  };
  int pending_get_downloads_actions_;

  enum DownloadAction { RESUME, RETRY, PAUSE, CANCEL, REMOVE, UNKNOWN };

  // Holds params provided to the download function calls.
  struct DownloadActionParams {
    explicit DownloadActionParams(DownloadAction download_action);
    DownloadActionParams(DownloadAction download_action, bool user_gesture);
    DownloadActionParams(const DownloadActionParams& other);

    ~DownloadActionParams() = default;

    DownloadAction action;
    bool has_user_gesture;
  };

  using PendingDownloadActions = std::map<std::string, DownloadActionParams>;
  PendingDownloadActions pending_actions_;

  void EnqueueDownloadAction(const std::string& download_guid,
                             const DownloadActionParams& params);

  ResumeCallback resume_callback_for_testing_;

  // The Registrar used to register for notifications.
  content::NotificationRegistrar registrar_;

  download::SimpleDownloadManagerCoordinator* original_coordinator_;
  download::SimpleDownloadManagerCoordinator* off_the_record_coordinator_;

  DISALLOW_COPY_AND_ASSIGN(DownloadManagerService);
};

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_MANAGER_SERVICE_H_
