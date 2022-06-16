// Copyright 2013 The Chromium Authors. All rights reserved.
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
#include "base/guid.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/bookmarks/partner_bookmarks_shim.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/reading_list/android/reading_list_manager.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/common/android/bookmark_id.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/prefs/pref_change_registrar.h"
#include "url/android/gurl_android.h"

namespace bookmarks {
class BookmarkModel;
class ManagedBookmarkService;
class ScopedGroupBookmarkActions;
}  // namespace bookmarks

class OptimizationGuideKeyedService;
class Profile;

// The delegate to fetch bookmarks information for the Android native
// bookmark page. This fetches the bookmarks, title, urls, folder
// hierarchy.
class BookmarkBridge : public bookmarks::BaseBookmarkModelObserver,
                       public PartnerBookmarksShim::Observer,
                       public ReadingListManager::Observer,
                       public ProfileObserver {
 public:
  BookmarkBridge(JNIEnv* env,
                 const base::android::JavaRef<jobject>& obj,
                 const base::android::JavaRef<jobject>& j_profile);

  BookmarkBridge(const BookmarkBridge&) = delete;
  BookmarkBridge& operator=(const BookmarkBridge&) = delete;

  void Destroy(JNIEnv*, const base::android::JavaParamRef<jobject>&);

  base::android::ScopedJavaLocalRef<jobject> GetBookmarkIdForWebContents(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jweb_contents,
      jboolean only_editable);

  bool IsDoingExtensiveChanges(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj);

  jboolean IsEditBookmarksEnabled(JNIEnv* env);

  void LoadEmptyPartnerBookmarkShimForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  // Loads a fake partner bookmarks shim for testing.
  // This is used in BookmarkBridgeTest.java.
  void LoadFakePartnerBookmarkShimForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  base::android::ScopedJavaLocalRef<jobject> GetBookmarkByID(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jlong id,
      jint type);

  void GetTopLevelFolderParentIDs(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_result_obj);

  void GetTopLevelFolderIDs(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jboolean get_special,
      jboolean get_normal,
      const base::android::JavaParamRef<jobject>& j_result_obj);

  base::android::ScopedJavaLocalRef<jobject> GetReadingListFolder(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  void GetAllFoldersWithDepths(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_folders_obj,
      const base::android::JavaParamRef<jobject>& j_depths_obj);

  base::android::ScopedJavaLocalRef<jobject> GetRootFolderId(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  base::android::ScopedJavaLocalRef<jobject> GetMobileFolderId(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  base::android::ScopedJavaLocalRef<jobject> GetOtherFolderId(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  base::android::ScopedJavaLocalRef<jobject> GetDesktopFolderId(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  base::android::ScopedJavaLocalRef<jobject> GetPartnerFolderId(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  base::android::ScopedJavaLocalRef<jstring> GetBookmarkGuidByIdForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jlong id,
      jint type);

  void GetChildIDs(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   jlong id,
                   jint type,
                   const base::android::JavaParamRef<jobject>& j_result_obj);

  jint GetChildCount(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj,
                     jlong id,
                     jint type);

  base::android::ScopedJavaLocalRef<jobject> GetChildAt(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jlong id,
      jint type,
      jint index);

  void ReorderChildren(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_bookmark_id_obj,
      jlongArray arr);

  // Get the number of bookmarks in the sub tree of the specified bookmark node.
  // The specified node must be of folder type.
  jint GetTotalBookmarkCount(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj,
                             jlong id,
                             jint type);

  void SetBookmarkTitle(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj,
                        jlong id,
                        jint type,
                        const base::android::JavaParamRef<jstring>& title);

  void SetBookmarkUrl(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj,
                      jlong id,
                      jint type,
                      const base::android::JavaParamRef<jobject>& url);

  void SetPowerBookmarkMeta(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jlong id,
      jint type,
      const base::android::JavaParamRef<jbyteArray>& bytes);

  base::android::ScopedJavaLocalRef<jbyteArray> GetPowerBookmarkMeta(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jlong id,
      jint type);

  void DeletePowerBookmarkMeta(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj,
                               jlong id,
                               jint type);

  bool DoesBookmarkExist(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         jlong id,
                         jint type);

  void GetBookmarksForFolder(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_folder_id_obj,
      const base::android::JavaParamRef<jobject>& j_callback_obj,
      const base::android::JavaParamRef<jobject>& j_result_obj);

  jboolean IsFolderVisible(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj,
                           jlong id,
                           jint type);

  void GetCurrentFolderHierarchy(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_folder_id_obj,
      const base::android::JavaParamRef<jobject>& j_callback_obj,
      const base::android::JavaParamRef<jobject>& j_result_obj);

  void SearchBookmarks(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj,
                       const base::android::JavaParamRef<jobject>& j_list,
                       const base::android::JavaParamRef<jstring>& j_query,
                       const base::android::JavaParamRef<jobjectArray>& j_tags,
                       jint type,
                       jint max_results);

  void GetBookmarksOfType(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj,
                          const base::android::JavaParamRef<jobject>& j_list,
                          jint type);

  base::android::ScopedJavaLocalRef<jobject> AddFolder(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_parent_id_obj,
      jint index,
      const base::android::JavaParamRef<jstring>& j_title);

  void DeleteBookmark(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_bookmark_id_obj);

  void RemoveAllUserBookmarks(JNIEnv* env,
                              const base::android::JavaParamRef<jobject>& obj);

  void MoveBookmark(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_bookmark_id_obj,
      const base::android::JavaParamRef<jobject>& j_parent_id_obj,
      jint index);

  base::android::ScopedJavaLocalRef<jobject> AddBookmark(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_web_contents,
      const base::android::JavaParamRef<jobject>& j_parent_id_obj,
      jint index,
      const base::android::JavaParamRef<jstring>& j_title,
      const base::android::JavaParamRef<jobject>& j_url);

  base::android::ScopedJavaLocalRef<jobject> AddToReadingList(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& j_title,
      const base::android::JavaParamRef<jobject>& j_url);

  base::android::ScopedJavaLocalRef<jobject> GetReadingListItem(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_url);

  void SetReadStatus(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj,
                     const base::android::JavaParamRef<jobject>& j_url,
                     jboolean j_read);

  void Undo(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  void StartGroupingUndos(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj);

  void EndGroupingUndos(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj);

  bool IsBookmarked(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& gurl);

  std::u16string GetTitle(const bookmarks::BookmarkNode* node) const;

  // ProfileObserver override
  void OnProfileWillBeDestroyed(Profile* profile) override;

  void GetUpdatedProductPrices(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobjectArray>& gurls,
      const base::android::JavaParamRef<jobject>& callback);

  void OnProductPriceUpdated(
      base::android::ScopedJavaGlobalRef<jobject> callback,
      const GURL& url,
      const base::flat_map<
          optimization_guide::proto::OptimizationType,
          optimization_guide::OptimizationGuideDecisionWithMetadata>&
          decisions);

 private:
  ~BookmarkBridge() override;

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
  const bookmarks::BookmarkNode* GetParentNode(
      const bookmarks::BookmarkNode* node);
  int GetBookmarkType(const bookmarks::BookmarkNode* node);
  bool IsReachable(const bookmarks::BookmarkNode* node) const;
  bool IsLoaded() const;
  bool IsFolderAvailable(const bookmarks::BookmarkNode* folder) const;
  void NotifyIfDoneLoading();

  // Override bookmarks::BaseBookmarkModelObserver.
  // Called when there are changes to the bookmark model that don't trigger
  // any of the other callback methods. For example, this is called when
  // partner bookmarks change.
  void BookmarkModelChanged() override;
  void BookmarkModelLoaded(bookmarks::BookmarkModel* model,
                           bool ids_reassigned) override;
  void BookmarkModelBeingDeleted(bookmarks::BookmarkModel* model) override;
  void BookmarkNodeMoved(bookmarks::BookmarkModel* model,
                         const bookmarks::BookmarkNode* old_parent,
                         size_t old_index,
                         const bookmarks::BookmarkNode* new_parent,
                         size_t new_index) override;
  void BookmarkNodeAdded(bookmarks::BookmarkModel* model,
                         const bookmarks::BookmarkNode* parent,
                         size_t index) override;
  void BookmarkNodeRemoved(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* parent,
                           size_t old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& removed_urls) override;
  void BookmarkAllUserNodesRemoved(bookmarks::BookmarkModel* model,
                                   const std::set<GURL>& removed_urls) override;
  void BookmarkNodeChanged(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeChildrenReordered(
      bookmarks::BookmarkModel* model,
      const bookmarks::BookmarkNode* node) override;
  void ExtensiveBookmarkChangesBeginning(
      bookmarks::BookmarkModel* model) override;
  void ExtensiveBookmarkChangesEnded(bookmarks::BookmarkModel* model) override;

  // Override PartnerBookmarksShim::Observer
  void PartnerShimChanged(PartnerBookmarksShim* shim) override;
  void PartnerShimLoaded(PartnerBookmarksShim* shim) override;
  void ShimBeingDeleted(PartnerBookmarksShim* shim) override;

  // Override ReadingListManager::Observer
  void ReadingListLoaded() override;
  void ReadingListChanged() override;

  void DestroyJavaObject();

  raw_ptr<Profile> profile_;
  JavaObjectWeakGlobalRef weak_java_ref_;
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;                     // weak
  raw_ptr<bookmarks::ManagedBookmarkService> managed_bookmark_service_;  // weak
  std::unique_ptr<bookmarks::ScopedGroupBookmarkActions>
      grouped_bookmark_actions_;
  PrefChangeRegistrar pref_change_registrar_;

  // Information about the Partner bookmarks (must check for IsLoaded()).
  // This is owned by profile.
  raw_ptr<PartnerBookmarksShim> partner_bookmarks_shim_;

  // Holds reading list data. A keyed service owned by the profile.
  raw_ptr<ReadingListManager> reading_list_manager_;

  // Observes the profile destruction and creation.
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};

  // A means of accessing metadata about bookmarks.
  raw_ptr<OptimizationGuideKeyedService> opt_guide_;

  // Weak pointers for creating callbacks that won't call into a destroyed
  // object.
  base::WeakPtrFactory<BookmarkBridge> weak_ptr_factory_;
};

#endif  // CHROME_BROWSER_BOOKMARKS_ANDROID_BOOKMARK_BRIDGE_H_
