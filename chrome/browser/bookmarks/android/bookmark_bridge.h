// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_ANDROID_BOOKMARK_BRIDGE_H_
#define CHROME_BROWSER_BOOKMARKS_ANDROID_BOOKMARK_BRIDGE_H_

#include <memory>
#include <set>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/supports_user_data.h"
#include "chrome/browser/android/bookmarks/partner_bookmarks_shim.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/reading_list/android/reading_list_manager.h"
#include "chrome/browser/reading_list/android/reading_list_manager_impl.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/scoped_group_bookmark_actions.h"
#include "components/bookmarks/common/android/bookmark_id.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/reading_list/core/dual_reading_list_model.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/reading_list/core/reading_list_model_observer.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "url/android/gurl_android.h"

class BookmarkBridgeTest;

// The delegate to fetch bookmarks information for the Android native
// bookmark page. This fetches the bookmarks, title, urls, folder
// hierarchy.
// The life cycle of the bridge is controlled by the BookmarkModel through the
// user data pattern. Native side of the bridge owns its Java counterpart.
class BookmarkBridge : public ProfileObserver,
                       public bookmarks::BaseBookmarkModelObserver,
                       public PartnerBookmarksShim::Observer,
                       public ReadingListManager::Observer,
                       public ReadingListModelObserver,
                       public signin::IdentityManager::Observer,
                       public base::SupportsUserData::Data {
 public:
  // All of the injected pointers must be non-null and must outlive `this`.
  BookmarkBridge(Profile* profile,
                 bookmarks::BookmarkModel* model,
                 bookmarks::ManagedBookmarkService* managed_bookmark_service,
                 reading_list::DualReadingListModel* dual_reading_list_model,
                 PartnerBookmarksShim* partner_bookmarks_shim,
                 signin::IdentityManager* identity_manager);

  BookmarkBridge(const BookmarkBridge&) = delete;
  BookmarkBridge& operator=(const BookmarkBridge&) = delete;
  ~BookmarkBridge() override;

  // Destroy the native object from Java.
  void Destroy(JNIEnv*);
  // Gets a reference to Java portion of the bridge.
  base::android::ScopedJavaGlobalRef<jobject> GetJavaBookmarkModel();
  int GetBookmarkType(const bookmarks::BookmarkNode* node);
  const bookmarks::BookmarkNode* GetParentNode(
      const bookmarks::BookmarkNode* node);

  jboolean AreAccountBookmarkFoldersActive(JNIEnv* env);

  base::android::ScopedJavaLocalRef<jobject>
  GetMostRecentlyAddedUserBookmarkIdForUrl(JNIEnv* env, const GURL& url);
  const bookmarks::BookmarkNode* GetMostRecentlyAddedUserBookmarkIdForUrlImpl(
      const GURL& url);

  bool IsDoingExtensiveChanges(JNIEnv* env);

  jboolean IsEditBookmarksEnabled(JNIEnv* env);

  void LoadEmptyPartnerBookmarkShimForTesting(JNIEnv* env);

  // Loads a fake partner bookmarks shim for testing.
  // This is used in BookmarkBridgeTest.java.
  void LoadFakePartnerBookmarkShimForTesting(JNIEnv* env);

  base::android::ScopedJavaLocalRef<jobject> GetBookmarkById(JNIEnv* env,
                                                             jlong id,
                                                             jint type);

  void GetAllFoldersWithDepths(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_folders_obj,
      const base::android::JavaParamRef<jobject>& j_depths_obj);

  void GetTopLevelFolderIds(
      JNIEnv* env,
      jboolean j_ignore_visibility,
      const base::android::JavaParamRef<jobject>& j_result_obj);
  std::vector<const bookmarks::BookmarkNode*> GetTopLevelFolderIdsImpl(
      bool ignore_visibility);
  base::android::ScopedJavaLocalRef<jobject> GetRootFolderId(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jobject> GetMobileFolderId(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jobject> GetOtherFolderId(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jobject> GetDesktopFolderId(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jobject> GetAccountMobileFolderId(
      JNIEnv* env);
  base::android::ScopedJavaLocalRef<jobject> GetAccountOtherFolderId(
      JNIEnv* env);
  base::android::ScopedJavaLocalRef<jobject> GetAccountDesktopFolderId(
      JNIEnv* env);
  base::android::ScopedJavaLocalRef<jobject> GetPartnerFolderId(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jobject>
  GetLocalOrSyncableReadingListFolder(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jobject> GetAccountReadingListFolder(
      JNIEnv* env);
  base::android::ScopedJavaLocalRef<jobject> GetDefaultReadingListFolder(
      JNIEnv* env);
  base::android::ScopedJavaLocalRef<jobject> GetDefaultBookmarkFolder(
      JNIEnv* env);

  std::string GetBookmarkGuidByIdForTesting(JNIEnv* env, jlong id, jint type);

  void GetChildIds(JNIEnv* env,
                   jlong id,
                   jint type,
                   const base::android::JavaParamRef<jobject>& j_result_obj);
  std::vector<const bookmarks::BookmarkNode*> GetChildIdsImpl(
      const bookmarks::BookmarkNode* parent);

  jint GetChildCount(JNIEnv* env,
                     jlong id,
                     jint type);

  base::android::ScopedJavaLocalRef<jobject> GetChildAt(JNIEnv* env,
                                                        jlong id,
                                                        jint type,
                                                        jint index);

  void ReorderChildren(
      JNIEnv* env,

      const base::android::JavaParamRef<jobject>& j_bookmark_id_obj,
      jlongArray arr);

  // Get the number of bookmarks in the sub tree of the specified bookmark node.
  // The specified node must be of folder type.
  jint GetTotalBookmarkCount(JNIEnv* env,
                             jlong id,
                             jint type);

  void SetBookmarkTitle(JNIEnv* env,
                        jlong id,
                        jint type,
                        const std::u16string& title);

  void SetBookmarkUrl(JNIEnv* env, jlong id, jint type, const GURL& url);

  void SetPowerBookmarkMeta(
      JNIEnv* env,
      jlong id,
      jint type,
      const base::android::JavaParamRef<jbyteArray>& bytes);

  base::android::ScopedJavaLocalRef<jbyteArray> GetPowerBookmarkMeta(
      JNIEnv* env,
      jlong id,
      jint type);

  void DeletePowerBookmarkMeta(JNIEnv* env,
                               jlong id,
                               jint type);

  bool DoesBookmarkExist(JNIEnv* env,
                         jlong id,
                         jint type);

  void GetBookmarksForFolder(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_folder_id_obj,
      const base::android::JavaParamRef<jobject>& j_result_obj);

  jboolean IsFolderVisible(JNIEnv* env,
                           jlong id,
                           jint type);

  void SearchBookmarks(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& j_list,
                       const std::u16string& query,
                       const base::android::JavaParamRef<jobjectArray>& j_tags,
                       jint type,
                       jint max_results);
  std::vector<const bookmarks::BookmarkNode*> SearchBookmarksImpl(
      power_bookmarks::PowerBookmarkQueryFields& query,
      int max_results);

  void GetBookmarksOfType(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& j_list,
                          jint type);

  base::android::ScopedJavaLocalRef<jobject> AddFolder(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_parent_id_obj,
      jint index,
      const std::u16string& title);

  void DeleteBookmark(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_bookmark_id_obj);

  void DeleteBookmarkImpl(const bookmarks::BookmarkNode* node, int type);

  void RemoveAllUserBookmarks(JNIEnv* env);

  void MoveBookmark(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_bookmark_id_obj,
      const base::android::JavaParamRef<jobject>& j_parent_id_obj,
      jint j_index);

  void MoveBookmarkImpl(const bookmarks::BookmarkNode* node,
                        int type,
                        const bookmarks::BookmarkNode* new_parent_node,
                        int parent_type,
                        int index);

  base::android::ScopedJavaLocalRef<jobject> AddBookmark(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_parent_id_obj,
      jint index,
      const std::u16string& title,
      const GURL& url);

  base::android::ScopedJavaLocalRef<jobject> AddToReadingList(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_parent_id_obj,
      const std::string& title,
      const GURL& url);

  void SetReadStatus(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& j_id,
                     jboolean j_read);
  void SetReadStatusImpl(const GURL& url, bool read);

  jint GetUnreadCount(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& j_id);

  jboolean IsAccountBookmark(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& j_id);
  bool IsAccountBookmarkImpl(const bookmarks::BookmarkNode* node);

  void Undo(JNIEnv* env);

  void StartGroupingUndos(JNIEnv* env);

  void EndGroupingUndos(JNIEnv* env);

  bool IsBookmarked(JNIEnv* env, const GURL& url);

  std::u16string GetTitle(const bookmarks::BookmarkNode* node) const;

  // ProfileObserver override
  void OnProfileWillBeDestroyed(Profile* profile) override;

  ReadingListManager* GetLocalOrSyncableReadingListManagerForTesting();
  ReadingListManager* GetAccountReadingListManagerIfAvailableForTesting();

 private:
  base::android::ScopedJavaLocalRef<jobject> CreateJavaBookmark(
      const bookmarks::BookmarkNode* node);
  void ExtractBookmarkNodeInformation(
      const bookmarks::BookmarkNode* node,
      const base::android::JavaRef<jobject>& j_result_obj);
  const bookmarks::BookmarkNode* GetNodeByID(long node_id, int type);
  const bookmarks::BookmarkNode* GetFolderWithFallback(long folder_id,
                                                       int type);
  bool IsEditBookmarksEnabled() const;
  void EditBookmarksEnabledChanged();
  // Returns whether |node| can be modified by the user.
  bool IsEditable(const bookmarks::BookmarkNode* node) const;
  // Returns whether |node| is a managed bookmark.
  bool IsManaged(const bookmarks::BookmarkNode* node) const;
  bool IsReachable(const bookmarks::BookmarkNode* node) const;
  bool IsLoaded() const;
  bool IsFolderAvailable(const bookmarks::BookmarkNode* folder) const;
  void NotifyIfDoneLoading();
  // Filters `nodes` on `IsReachable` and adds the result to the given
  // `j_result_obj`.
  void AddBookmarkNodesToBookmarkIdList(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_result_obj,
      const std::vector<const bookmarks::BookmarkNode*>& nodes);
  void FilterUnreachableBookmarks(
      std::vector<const bookmarks::BookmarkNode*>* nodes);
  // Returns the correct `ReadingListManager` given the corresponding `node`
  // which is the root.
  ReadingListManager* GetReadingListManagerFromParentNode(
      const bookmarks::BookmarkNode* node);
  // Moves `node` to be a child of `new_parent_node` which may require swapping
  // to/from ReadingListManager.
  void MoveNodeBetweenReadingListAndBookmarks(
      const bookmarks::BookmarkNode* node,
      int type,
      const bookmarks::BookmarkNode* new_parent_node,
      int parent_type,
      int index);
  bool IsPermanentFolderVisible(bool ignore_visibility,
                                const bookmarks::BookmarkNode* folder);
  const bookmarks::BookmarkNode* GetCorrespondingAccountFolder(
      const bookmarks::BookmarkNode* folder);

  // Override bookmarks::BaseBookmarkModelObserver.
  // Called when there are changes to the bookmark model that don't trigger
  // any of the other callback methods. For example, this is called when
  // partner bookmarks change.
  void BookmarkModelChanged() override;
  void BookmarkModelLoaded(bool ids_reassigned) override;
  void BookmarkModelBeingDeleted() override;
  void BookmarkNodeMoved(const bookmarks::BookmarkNode* old_parent,
                         size_t old_index,
                         const bookmarks::BookmarkNode* new_parent,
                         size_t new_index) override;
  void BookmarkNodeAdded(const bookmarks::BookmarkNode* parent,
                         size_t index,
                         bool added_by_user) override;
  void BookmarkNodeRemoved(const bookmarks::BookmarkNode* parent,
                           size_t old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& removed_urls,
                           const base::Location& location) override;
  void BookmarkAllUserNodesRemoved(const std::set<GURL>& removed_urls,
                                   const base::Location& location) override;
  void BookmarkNodeChanged(const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeChildrenReordered(
      const bookmarks::BookmarkNode* node) override;
  void ExtensiveBookmarkChangesBeginning() override;
  void ExtensiveBookmarkChangesEnded() override;

  // Override PartnerBookmarksShim::Observer
  void PartnerShimChanged(PartnerBookmarksShim* shim) override;
  void PartnerShimLoaded(PartnerBookmarksShim* shim) override;
  void ShimBeingDeleted(PartnerBookmarksShim* shim) override;

  // Override ReadingListManager::Observer
  void ReadingListLoaded() override;
  void ReadingListChanged() override;

  // Override ReadingListModelObserver
  void ReadingListModelLoaded(const ReadingListModel* model) override;
  void ReadingListModelCompletedBatchUpdates(
      const ReadingListModel* model) override;

  // signin::IdentityManager::Observer implementation:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;

  void DestroyJavaObject();
  void CreateOrDestroyAccountReadingListManagerIfNeeded();

  const raw_ptr<Profile> profile_;  // weak
  base::android::ScopedJavaGlobalRef<jobject> java_bookmark_model_;
  const raw_ptr<bookmarks::BookmarkModel> bookmark_model_;  // weak
  const raw_ptr<bookmarks::ManagedBookmarkService>
      managed_bookmark_service_;  // weak
  const raw_ptr<reading_list::DualReadingListModel>
      dual_reading_list_model_;  // weak
  const ReadingListManagerImpl::IdGenerationFunction id_gen_func_;
  // Holds reading list data as an in-memory BookmarkNode tree.
  const std::unique_ptr<ReadingListManager>
      local_or_syncable_reading_list_manager_;

  std::unique_ptr<bookmarks::ScopedGroupBookmarkActions>
      grouped_bookmark_actions_;
  PrefChangeRegistrar pref_change_registrar_;

  // Information about the Partner bookmarks (must check for IsLoaded()).
  // This is owned by profile.
  raw_ptr<PartnerBookmarksShim> partner_bookmarks_shim_;

  // Holds account reading list data, similar to above. Only non-null if the
  // account reading list is available.
  std::unique_ptr<ReadingListManager> account_reading_list_manager_;

  raw_ptr<ReadingListModel> account_reading_list_model_;  // weak
  raw_ptr<signin::IdentityManager> identity_manager_;  // weak

  // Observes the profile destruction and creation.
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};
  base::ScopedObservation<bookmarks::BookmarkModel,
                          bookmarks::BaseBookmarkModelObserver>
      bookmark_model_observation_{this};
  base::ScopedObservation<PartnerBookmarksShim, PartnerBookmarksShim::Observer>
      partner_bookmarks_shim_observation_{this};
  base::ScopedMultiSourceObservation<ReadingListManager,
                                     ReadingListManager::Observer>
      reading_list_manager_observations_{this};
  base::ScopedObservation<reading_list::DualReadingListModel,
                          ReadingListModelObserver>
      dual_reading_list_model_observation_{this};
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  bool suppress_observer_notifications_ = false;

  // Weak pointers for creating callbacks that won't call into a destroyed
  // object.
  base::WeakPtrFactory<BookmarkBridge> weak_ptr_factory_;
};

#endif  // CHROME_BROWSER_BOOKMARKS_ANDROID_BOOKMARK_BRIDGE_H_
