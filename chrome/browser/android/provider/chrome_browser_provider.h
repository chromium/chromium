// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PROVIDER_CHROME_BROWSER_PROVIDER_H_
#define CHROME_BROWSER_ANDROID_PROVIDER_CHROME_BROWSER_PROVIDER_H_

#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/scoped_observation.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/history/core/browser/android/android_history_types.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"

class AndroidHistoryProviderService;
class Profile;

namespace bookmarks {
class BookmarkModel;
class ModelLoader;
}  // namespace bookmarks

namespace history {
class TopSites;
}

// This class implements the native methods of ChromeBrowserProvider.java
class ChromeBrowserProvider : public bookmarks::BaseBookmarkModelObserver,
                              public history::HistoryServiceObserver {
 public:
  ChromeBrowserProvider(JNIEnv* env, jobject obj);

  // Adds either a new bookmark or bookmark folder based on |is_folder|.  The
  // bookmark is added to the beginning of the specified parent and if the
  // parent ID is not valid (i.e. < 0) then it will be added to the bookmark
  // bar.
  jlong AddBookmark(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>&,
                    const base::android::JavaParamRef<jstring>& jurl,
                    const base::android::JavaParamRef<jstring>& jtitle,
                    jboolean is_folder,
                    jlong parent_id);

  // Removes a bookmark (or folder) with the specified ID.
  jint RemoveBookmark(JNIEnv*,
                      const base::android::JavaParamRef<jobject>&,
                      jlong id);

  // Updates a bookmark (or folder) with the the new title and new URL.
  // The |url| field will be ignored if the bookmark node is a folder.
  // If a valid |parent_id| (>= 0) different from the currently specified
  // parent is given, the node will be moved to that folder as the first
  // child.
  jint UpdateBookmark(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>&,
                      jlong id,
                      const base::android::JavaParamRef<jstring>& url,
                      const base::android::JavaParamRef<jstring>& title,
                      jlong parent_id);

  // The below are methods to support Android public API.
  // Bookmark and history APIs. -----------------------------------------------
  jlong AddBookmarkFromAPI(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& url,
      const base::android::JavaParamRef<jobject>& created,
      const base::android::JavaParamRef<jobject>& isBookmark,
      const base::android::JavaParamRef<jobject>& date,
      const base::android::JavaParamRef<jbyteArray>& favicon,
      const base::android::JavaParamRef<jstring>& title,
      const base::android::JavaParamRef<jobject>& visits,
      jlong parent_id);

  base::android::ScopedJavaLocalRef<jobject> QueryBookmarkFromAPI(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobjectArray>& projection,
      const base::android::JavaParamRef<jstring>& selections,
      const base::android::JavaParamRef<jobjectArray>& selection_args,
      const base::android::JavaParamRef<jstring>& sort_order);

  jint UpdateBookmarkFromAPI(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& url,
      const base::android::JavaParamRef<jobject>& created,
      const base::android::JavaParamRef<jobject>& isBookmark,
      const base::android::JavaParamRef<jobject>& date,
      const base::android::JavaParamRef<jbyteArray>& favicon,
      const base::android::JavaParamRef<jstring>& title,
      const base::android::JavaParamRef<jobject>& visits,
      jlong parent_id,
      const base::android::JavaParamRef<jstring>& selections,
      const base::android::JavaParamRef<jobjectArray>& selection_args);

  jint RemoveBookmarkFromAPI(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& selections,
      const base::android::JavaParamRef<jobjectArray>& selection_args);

  jint RemoveHistoryFromAPI(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& selections,
      const base::android::JavaParamRef<jobjectArray>& selection_args);

  jlong AddSearchTermFromAPI(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& search_term,
      const base::android::JavaParamRef<jobject>& date);

  base::android::ScopedJavaLocalRef<jobject> QuerySearchTermFromAPI(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobjectArray>& projection,
      const base::android::JavaParamRef<jstring>& selections,
      const base::android::JavaParamRef<jobjectArray>& selection_args,
      const base::android::JavaParamRef<jstring>& sort_order);

  jint RemoveSearchTermFromAPI(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& selections,
      const base::android::JavaParamRef<jobjectArray>& selection_args);

  jint UpdateSearchTermFromAPI(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& search_term,
      const base::android::JavaParamRef<jobject>& date,
      const base::android::JavaParamRef<jstring>& selections,
      const base::android::JavaParamRef<jobjectArray>& selection_args);

 private:
  ~ChromeBrowserProvider() override;

  // Override bookmarks::BaseBookmarkModelObserver.
  void BookmarkModelChanged() override;
  void ExtensiveBookmarkChangesBeginning(
      bookmarks::BookmarkModel* model) override;
  void ExtensiveBookmarkChangesEnded(bookmarks::BookmarkModel* model) override;

  // Deals with updates to the history service.
  void OnHistoryChanged();

  // Override history::HistoryServiceObserver.
  void OnURLVisited(history::HistoryService* history_service,
                    ui::PageTransition transition,
                    const history::URLRow& row,
                    const history::RedirectList& redirects,
                    base::Time visit_time) override;
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;
  void OnKeywordSearchTermUpdated(history::HistoryService* history_service,
                                  const history::URLRow& row,
                                  history::KeywordID keyword_id,
                                  const base::string16& term) override;
  void OnKeywordSearchTermDeleted(history::HistoryService* history_service,
                                  history::URLID url_id) override;
  void HistoryServiceBeingDeleted(
      history::HistoryService* history_service) override;

  bool GetJavaProviderOrDeleteSelf(
      base::android::ScopedJavaLocalRef<jobject>* out_ref,
      JNIEnv* env);

  JavaObjectWeakGlobalRef weak_java_provider_;

  // Profile must outlive this object.
  //
  // HistoryService and history::TopSites lifetime is bound to the lifetime of
  // Profile, they are safe to use as long as the Profile is alive.
  Profile* profile_;
  scoped_refptr<history::TopSites> top_sites_;

  base::WeakPtr<bookmarks::BookmarkModel> bookmark_model_;
  scoped_refptr<bookmarks::ModelLoader> bookmark_model_loader_;

  std::unique_ptr<AndroidHistoryProviderService> service_;

  base::CancelableTaskTracker cancelable_task_tracker_;

  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      scoped_history_service_observer_{this};

  bool handling_extensive_changes_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserProvider);
};

#endif  // CHROME_BROWSER_ANDROID_PROVIDER_CHROME_BROWSER_PROVIDER_H_
