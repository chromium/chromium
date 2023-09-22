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
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/supports_user_data.h"
#include "chrome/browser/android/bookmarks/partner_bookmarks_shim.h"
#include "chrome/browser/image_service/image_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/reading_list/android/reading_list_manager.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/common/android/bookmark_id.h"
#include "components/prefs/pref_change_registrar.h"
#include "url/android/gurl_android.h"

namespace bookmarks {
class BookmarkModel;
class ManagedBookmarkService;
class ScopedGroupBookmarkActions;
}  // namespace bookmarks

class Profile;

// The delegate to fetch bookmarks information for the Android native
// bookmark page. This fetches the bookmarks, title, urls, folder
// hierarchy.
// The life cycle of the bridge is controlled by the BookmarkModel through the
// user data pattern. Native side of the bridge owns its Java counterpart.
class BookmarkBridge : public bookmarks::BaseBookmarkModelObserver,
                       public PartnerBookmarksShim::Observer,
                       public ReadingListManager::Observer,
                       public ProfileObserver,
                       public base::SupportsUserData::Data {
 public:
  BookmarkBridge(Profile* profile,
                 bookmarks::BookmarkModel* model,
                 bookmarks::ManagedBookmarkService* managed_bookmark_service,
                 PartnerBookmarksShim* partner_bookmarks_shim,
                 ReadingListManager* reading_list_manager,
                 page_image_service::ImageService* image_service);

  BookmarkBridge(const BookmarkBridge&) = delete;
  BookmarkBridge& operator=(const BookmarkBridge&) = delete;
  ~BookmarkBridge() override;

  void Destroy(JNIEnv*);

  void GetImageUrlForBookmark(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_url,
      const base::android::JavaParamRef<jobject>& j_callback);

  base::android::ScopedJavaLocalRef<jobject> GetBookmarkIdForWebContents(
      JNIEnv* env,

      const base::android::JavaParamRef<jobject>& jweb_contents,
      jboolean only_editable);

  bool IsDoingExtensiveChanges(JNIEnv* env);

  jboolean IsEditBookmarksEnabled(JNIEnv* env);

  void LoadEmptyPartnerBookmarkShimForTesting(JNIEnv* env);

  // Loads a fake partner bookmarks shim for testing.
  // This is used in BookmarkBridgeTest.java.
  void LoadFakePartnerBookmarkShimForTesting(JNIEnv* env);

  base::android::ScopedJavaLocalRef<jobject> GetBookmarkById(JNIEnv* env,
                                                             jlong id,
                                                             jint type);

  void GetTopLevelFolderIds(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_result_obj);

  base::android::ScopedJavaLocalRef<jobject> GetReadingListFolder(JNIEnv* env);

  void GetAllFoldersWithDepths(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_folders_obj,
      const base::android::JavaParamRef<jobject>& j_depths_obj);

  base::android::ScopedJavaLocalRef<jobject> GetRootFolderId(JNIEnv* env);

  base::android::ScopedJavaLocalRef<jobject> GetMobileFolderId(JNIEnv* env);

  base::android::ScopedJavaLocalRef<jobject> GetOtherFolderId(JNIEnv* env);

  base::android::ScopedJavaLocalRef<jobject> GetDesktopFolderId(JNIEnv* env);

  base::android::ScopedJavaLocalRef<jobject> GetPartnerFolderId(JNIEnv* env);

  base::android::ScopedJavaLocalRef<jstring> GetBookmarkGuidByIdForTesting(
      JNIEnv* env,
      jlong id,
      jint type);

  void GetChildIds(JNIEnv* env,
                   jlong id,
                   jint type,
                   const base::android::JavaParamRef<jobject>& j_result_obj);

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
                        const base::android::JavaParamRef<jstring>& title);

  void SetBookmarkUrl(JNIEnv* env,
                      jlong id,
                      jint type,
                      const base::android::JavaParamRef<jobject>& url);

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
                       const base::android::JavaParamRef<jstring>& j_query,
                       const base::android::JavaParamRef<jobjectArray>& j_tags,
                       jint type,
                       jint max_results);

  void GetBookmarksOfType(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& j_list,
                          jint type);

  base::android::ScopedJavaLocalRef<jobject> AddFolder(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_parent_id_obj,
      jint index,
      const base::android::JavaParamRef<jstring>& j_title);

  void DeleteBookmark(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_bookmark_id_obj);

  void RemoveAllUserBookmarks(JNIEnv* env);

  void MoveBookmark(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_bookmark_id_obj,
      const base::android::JavaParamRef<jobject>& j_parent_id_obj,
      jint index);

  base::android::ScopedJavaLocalRef<jobject> AddBookmark(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_parent_id_obj,
      jint index,
      const base::android::JavaParamRef<jstring>& j_title,
      const base::android::JavaParamRef<jobject>& j_url);

  base::android::ScopedJavaLocalRef<jobject> AddToReadingList(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& j_title,
      const base::android::JavaParamRef<jobject>& j_url);

  base::android::ScopedJavaLocalRef<jobject> GetReadingListItem(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_url);

  void SetReadStatus(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& j_url,
                     jboolean j_read);

  void Undo(JNIEnv* env);

  void StartGroupingUndos(JNIEnv* env);

  void EndGroupingUndos(JNIEnv* env);

  bool IsBookmarked(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& gurl);

  std::u16string GetTitle(const bookmarks::BookmarkNode* node) const;

  jint GetUnreadCount(JNIEnv* env);

  // ProfileObserver override
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // Gets a reference to Java portion of the bridge.
  base::android::ScopedJavaGlobalRef<jobject> GetJavaBookmarkModel();

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
                         size_t index,
                         bool added_by_user) override;
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
  base::android::ScopedJavaGlobalRef<jobject> java_bookmark_model_;
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;                     // weak
  raw_ptr<bookmarks::ManagedBookmarkService> managed_bookmark_service_;  // weak
  std::unique_ptr<bookmarks::ScopedGroupBookmarkActions>
      grouped_bookmark_actions_;
  PrefChangeRegistrar pref_change_registrar_;

  // Information about the Partner bookmarks (must check for IsLoaded()).
  // This is owned by profile.
  raw_ptr<PartnerBookmarksShim> partner_bookmarks_shim_;

  // Holds reading list data. A keyed service owned by the profile.
  raw_ptr<ReadingListManager> reading_list_manager_;  // weak

  raw_ptr<page_image_service::ImageService> image_service_;  // weak

  // Observes the profile destruction and creation.
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};

  // Weak pointers for creating callbacks that won't call into a destroyed
  // object.
  base::WeakPtrFactory<BookmarkBridge> weak_ptr_factory_;
};

#endif  // CHROME_BROWSER_BOOKMARKS_ANDROID_BOOKMARK_BRIDGE_H_
