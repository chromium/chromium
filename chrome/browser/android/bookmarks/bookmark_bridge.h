// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_BOOKMARKS_BOOKMARK_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_BOOKMARKS_BOOKMARK_BRIDGE_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/compiler_specific.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/bookmarks/partner_bookmarks_shim.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/common/android/bookmark_id.h"
#include "components/prefs/pref_change_registrar.h"

namespace bookmarks {
class BookmarkModel;
class ManagedBookmarkService;
class ScopedGroupBookmarkActions;
}

class Profile;

// The delegate to fetch bookmarks information for the Android native
// bookmark page. This fetches the bookmarks, title, urls, folder
// hierarchy.
class BookmarkBridge : public bookmarks::BaseBookmarkModelObserver,
                        public PartnerBookmarksShim::Observer {
 public:
  BookmarkBridge(JNIEnv* env,
                 const base::android::JavaRef<jobject>& obj,
                 const base::android::JavaRef<jobject>& j_profile);
  void Destroy(JNIEnv*, const base::android::JavaParamRef<jobject>&);

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

  void GetPermanentNodeIDs(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_result_obj);

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

  void GetChildIDs(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   jlong id,
                   jint type,
                   jboolean get_folders,
                   jboolean get_bookmarks,
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
                      const base::android::JavaParamRef<jstring>& url);

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
                       jint max_results);

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

  void RemoveAllUserBookmarks(
      JNIEnv* env,
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
      const base::android::JavaParamRef<jobject>& j_parent_id_obj,
      jint index,
      const base::android::JavaParamRef<jstring>& j_title,
      const base::android::JavaParamRef<jstring>& j_url);

  void Undo(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  void StartGroupingUndos(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj);

  void EndGroupingUndos(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj);

  base::string16 GetTitle(const bookmarks::BookmarkNode* node) const;

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

  Profile* profile_;
  JavaObjectWeakGlobalRef weak_java_ref_;
  bookmarks::BookmarkModel* bookmark_model_;  // weak
  bookmarks::ManagedBookmarkService* managed_bookmark_service_;  // weak
  std::unique_ptr<bookmarks::ScopedGroupBookmarkActions>
      grouped_bookmark_actions_;
  PrefChangeRegistrar pref_change_registrar_;

  // Information about the Partner bookmarks (must check for IsLoaded()).
  // This is owned by profile.
  PartnerBookmarksShim* partner_bookmarks_shim_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkBridge);
};

#endif  // CHROME_BROWSER_ANDROID_BOOKMARKS_BOOKMARK_BRIDGE_H_
