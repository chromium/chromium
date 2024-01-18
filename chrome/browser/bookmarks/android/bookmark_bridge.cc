// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/android/bookmark_bridge.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/containers/adapters.h"
#include "base/containers/stack.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/string_compare.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "chrome/android/chrome_jni_headers/BookmarkBridge_jni.h"
#include "chrome/browser/android/bookmarks/partner_bookmarks_reader.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/reading_list/android/reading_list_manager_impl.h"
#include "chrome/browser/reading_list/reading_list_model_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/undo/bookmark_undo_service_factory.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/browser/titled_url_match.h"
#include "components/bookmarks/common/android/bookmark_type.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/page_image_service/image_service.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "components/prefs/pref_service.h"
#include "components/query_parser/query_parser.h"
#include "components/reading_list/core/dual_reading_list_model.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/undo/bookmark_undo_service.h"
#include "components/undo/undo_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaIntArray;
using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using bookmarks::BookmarkType;
using bookmarks::android::JavaBookmarkIdCreateBookmarkId;
using bookmarks::android::JavaBookmarkIdGetId;
using bookmarks::android::JavaBookmarkIdGetType;
using content::BrowserThread;
using power_bookmarks::PowerBookmarkMeta;

namespace {
// The key used to connect the instance of the bookmark bridge to the bookmark
// model.
const char kBookmarkBridgeUserDataKey[] = "bookmark_bridge";

// Compares titles of different instance of BookmarkNode.
class BookmarkTitleComparer {
 public:
  explicit BookmarkTitleComparer(BookmarkBridge* bookmark_bridge,
                                 const icu::Collator* collator)
      : bookmark_bridge_(bookmark_bridge), collator_(collator) {}

  bool operator()(const BookmarkNode* lhs, const BookmarkNode* rhs) {
    if (collator_) {
      return base::i18n::CompareString16WithCollator(
                 *collator_, bookmark_bridge_->GetTitle(lhs),
                 bookmark_bridge_->GetTitle(rhs)) == UCOL_LESS;
    } else {
      return lhs->GetTitle() < rhs->GetTitle();
    }
  }

 private:
  raw_ptr<BookmarkBridge> bookmark_bridge_;  // weak
  raw_ptr<const icu::Collator> collator_;
};

std::unique_ptr<icu::Collator> GetICUCollator() {
  UErrorCode error = U_ZERO_ERROR;
  std::unique_ptr<icu::Collator> collator_;
  collator_.reset(icu::Collator::createInstance(error));
  if (U_FAILURE(error))
    collator_.reset(nullptr);

  return collator_;
}

// Handles the response from page_image_service::ImageService when requesting
// a salient image url.
void HandleImageUrlResponse(
    base::android::ScopedJavaGlobalRef<jobject> callback,
    const GURL& image_url) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::RunObjectCallbackAndroid(
      callback, url::GURLAndroid::FromNativeGURL(env, image_url));
}

}  // namespace

ScopedJavaLocalRef<jobject> JNI_BookmarkBridge_NativeGetForProfile(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  if (!profile)
    return nullptr;

  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(profile);
  if (!model)
    return nullptr;

  BookmarkBridge* bookmark_bridge = static_cast<BookmarkBridge*>(
      model->GetUserData(kBookmarkBridgeUserDataKey));

  if (!bookmark_bridge) {
    auto reading_list_id_generation_func =
        base::BindRepeating([](int64_t* id) { return (*id)++; },
                            base::Owned(std::make_unique<int64_t>(0)));
    auto* dual_reading_list =
        ReadingListModelFactory::GetAsDualReadingListForBrowserContext(profile);
    std::unique_ptr<ReadingListManagerImpl> account_reading_list_manager =
        nullptr;
    auto* account_model = dual_reading_list->GetAccountModelIfSyncing();
    if (account_model) {
      account_reading_list_manager = std::make_unique<ReadingListManagerImpl>(
          account_model, reading_list_id_generation_func);
    }
    bookmark_bridge = new BookmarkBridge(
        profile, model, ManagedBookmarkServiceFactory::GetForProfile(profile),
        PartnerBookmarksShim::BuildForBrowserContext(
            chrome::GetBrowserContextRedirectedInIncognito(profile)),
        std::make_unique<ReadingListManagerImpl>(
            dual_reading_list->GetLocalOrSyncableModel(),
            reading_list_id_generation_func),
        std::move(account_reading_list_manager),
        page_image_service::ImageServiceFactory::GetForBrowserContext(profile));
    model->SetUserData(kBookmarkBridgeUserDataKey,
                       base::WrapUnique(bookmark_bridge));
  }

  return ScopedJavaLocalRef<jobject>(bookmark_bridge->GetJavaBookmarkModel());
}

// TODO(crbug.com/1510547): Support the account reading list availability
// changing at runtime.
BookmarkBridge::BookmarkBridge(
    Profile* profile,
    BookmarkModel* model,
    bookmarks::ManagedBookmarkService* managed_bookmark_service,
    PartnerBookmarksShim* partner_bookmarks_shim,
    std::unique_ptr<ReadingListManager> local_or_syncable_reading_list_manager,
    std::unique_ptr<ReadingListManager> account_reading_list_manager,
    page_image_service::ImageService* image_service)
    : profile_(profile),
      bookmark_model_(model),
      managed_bookmark_service_(managed_bookmark_service),
      partner_bookmarks_shim_(partner_bookmarks_shim),
      local_or_syncable_reading_list_manager_(
          std::move(local_or_syncable_reading_list_manager)),
      account_reading_list_manager_(std::move(account_reading_list_manager)),
      image_service_(image_service),
      weak_ptr_factory_(this) {
  profile_observation_.Observe(profile_);
  bookmark_model_observation_.Observe(bookmark_model_);
  partner_bookmarks_shim_observation_.Observe(partner_bookmarks_shim_);
  reading_list_manager_observations_.AddObservation(
      local_or_syncable_reading_list_manager_.get());
  if (account_reading_list_manager_) {
    reading_list_manager_observations_.AddObservation(
        account_reading_list_manager_.get());
  }

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      bookmarks::prefs::kEditBookmarksEnabled,
      base::BindRepeating(&BookmarkBridge::EditBookmarksEnabledChanged,
                          base::Unretained(this)));

  NotifyIfDoneLoading();

  // Since a sync or import could have started before this class is
  // initialized, we need to make sure that our initial state is
  // up to date.
  if (bookmark_model_->IsDoingExtensiveChanges())
    ExtensiveBookmarkChangesBeginning(bookmark_model_);

  java_bookmark_model_ = Java_BookmarkBridge_createBookmarkModel(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
}

BookmarkBridge::~BookmarkBridge() {
  reading_list_manager_observations_.RemoveAllObservations();
  partner_bookmarks_shim_observation_.Reset();
  bookmark_model_observation_.Reset();
  profile_observation_.Reset();
}

void BookmarkBridge::Destroy(JNIEnv*) {
  // This will call the destructor because the user data is a unique pointer.
  bookmark_model_->RemoveUserData(kBookmarkBridgeUserDataKey);
}

void BookmarkBridge::GetImageUrlForBookmark(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_url,
    const JavaParamRef<jobject>& j_callback) {
  ScopedJavaGlobalRef<jobject> callback(j_callback);
  if (!image_service_) {
    base::android::RunObjectCallbackAndroid(callback, nullptr);
    return;
  }

  page_image_service::mojom::Options options;
  options.optimization_guide_images = true;
  image_service_->FetchImageFor(
      page_image_service::mojom::ClientId::Bookmarks,
      *url::GURLAndroid::ToNativeGURL(env, j_url), options,
      base::BindOnce(&HandleImageUrlResponse, callback));
}

base::android::ScopedJavaLocalRef<jobject>
BookmarkBridge::GetMostRecentlyAddedUserBookmarkIdForUrl(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::unique_ptr<GURL> url = url::GURLAndroid::ToNativeGURL(env, j_url);
  CHECK(url);

  const BookmarkNode* node = GetMostRecentlyAddedUserBookmarkIdForUrlImpl(*url);
  if (node) {
    return JavaBookmarkIdCreateBookmarkId(env, node->id(),
                                          GetBookmarkType(node));
  }
  return nullptr;
}

const bookmarks::BookmarkNode*
BookmarkBridge::GetMostRecentlyAddedUserBookmarkIdForUrlImpl(const GURL& url) {
  std::vector<const bookmarks::BookmarkNode*> nodes;
  const auto* reading_list_node =
      local_or_syncable_reading_list_manager_->Get(url);
  if (reading_list_node) {
    nodes.push_back(reading_list_node);
  }

  if (account_reading_list_manager_) {
    reading_list_node = account_reading_list_manager_->Get(url);
    if (reading_list_node) {
      nodes.push_back(reading_list_node);
    }
  }

  // Get all the nodes for |url| from BookmarkModel and sort them by date added.
  std::vector<raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>
      bookmark_model_result = bookmark_model_->GetNodesByURL(url);
  nodes.insert(nodes.end(), bookmark_model_result.begin(),
               bookmark_model_result.end());
  std::sort(nodes.begin(), nodes.end(), &bookmarks::MoreRecentlyAdded);

  if (nodes.size() == 0) {
    return nullptr;
  }

  // Return the first node matching the search criteria.
  return nodes.front();
}

jboolean BookmarkBridge::IsEditBookmarksEnabled(JNIEnv* env) {
  return IsEditBookmarksEnabled();
}

void BookmarkBridge::LoadEmptyPartnerBookmarkShimForTesting(JNIEnv* env) {
  if (partner_bookmarks_shim_->IsLoaded())
    return;
  partner_bookmarks_shim_->SetPartnerBookmarksRoot(
      PartnerBookmarksReader::CreatePartnerBookmarksRootForTesting());
  PartnerBookmarksShim::DisablePartnerBookmarksEditing();
  DCHECK(partner_bookmarks_shim_->IsLoaded());
}

// Loads a fake partner bookmarks shim for testing.
// This is used in BookmarkBridgeTest.java.
void BookmarkBridge::LoadFakePartnerBookmarkShimForTesting(JNIEnv* env) {
  if (partner_bookmarks_shim_->IsLoaded())
    return;
  std::unique_ptr<BookmarkNode> root_partner_node =
      PartnerBookmarksReader::CreatePartnerBookmarksRootForTesting();
  BookmarkNode* partner_bookmark_a =
      root_partner_node->Add(std::make_unique<BookmarkNode>(
          1, base::Uuid::GenerateRandomV4(), GURL("http://www.a.com")));
  partner_bookmark_a->SetTitle(u"Partner Bookmark A");
  BookmarkNode* partner_bookmark_b =
      root_partner_node->Add(std::make_unique<BookmarkNode>(
          2, base::Uuid::GenerateRandomV4(), GURL("http://www.b.com")));
  partner_bookmark_b->SetTitle(u"Partner Bookmark B");
  partner_bookmarks_shim_->SetPartnerBookmarksRoot(
      std::move(root_partner_node));
  PartnerBookmarksShim::DisablePartnerBookmarksEditing();
  DCHECK(partner_bookmarks_shim_->IsLoaded());
}

ScopedJavaLocalRef<jobject> BookmarkBridge::GetBookmarkById(
    JNIEnv* env,
    jlong id,
    jint type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsLoaded());
  const BookmarkNode* node = GetNodeByID(id, type);
  return node ? CreateJavaBookmark(node) : ScopedJavaLocalRef<jobject>();
}

bool BookmarkBridge::IsDoingExtensiveChanges(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return bookmark_model_->IsDoingExtensiveChanges();
}

void BookmarkBridge::GetAllFoldersWithDepths(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_folders_obj,
    const JavaParamRef<jobject>& j_depths_obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsLoaded());

  std::unique_ptr<icu::Collator> collator = GetICUCollator();

  // Vector to temporarily contain all child bookmarks at same level for sorting
  std::vector<const BookmarkNode*> bookmarks = {
      bookmark_model_->mobile_node(),
      bookmark_model_->bookmark_bar_node(),
      bookmark_model_->other_node(),
  };

  // Push all sorted top folders in stack and give them depth of 0.
  // Note the order to push folders to stack should be opposite to the order in
  // output.
  base::stack<std::pair<const BookmarkNode*, int>> stk;
  for (const auto* bookmark : base::Reversed(bookmarks))
    stk.emplace(bookmark, 0);

  while (!stk.empty()) {
    const BookmarkNode* node = stk.top().first;
    int depth = stk.top().second;
    stk.pop();
    Java_BookmarkBridge_addToBookmarkIdListWithDepth(
        env, j_folders_obj, node->id(), GetBookmarkType(node), j_depths_obj,
        depth);
    bookmarks.clear();
    for (const auto& child : node->children()) {
      if (child->is_folder() &&
          !managed_bookmark_service_->IsNodeManaged(child.get())) {
        bookmarks.push_back(child.get());
      }
    }
    std::stable_sort(bookmarks.begin(), bookmarks.end(),
                     BookmarkTitleComparer(this, collator.get()));
    for (const auto* bookmark : base::Reversed(bookmarks))
      stk.emplace(bookmark, depth + 1);
  }
}

void BookmarkBridge::GetTopLevelFolderIds(
    JNIEnv* env,
    jboolean j_ignore_visibility,
    const JavaParamRef<jobject>& j_result_obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsLoaded());

  AddBookmarkNodesToBookmarkIdList(
      env, j_result_obj, GetTopLevelFolderIdsImpl(j_ignore_visibility));
}
std::vector<const BookmarkNode*> BookmarkBridge::GetTopLevelFolderIdsImpl(
    bool ignore_visibility) {
  std::vector<const BookmarkNode*> top_level_folders;
  // Query for the top-level folders:
  // bookmarks bar, mobile node, other node, and managed node (if it exists).
  // Account bookmarks come first, and local bookmarks after.

  for (const auto& root_child : bookmark_model_->root_node()->children()) {
    if (!ignore_visibility && !root_child->IsVisible()) {
      continue;
    }

    top_level_folders.push_back(root_child.get());
  }

  if (account_reading_list_manager_ &&
      account_reading_list_manager_->GetRoot()) {
    top_level_folders.push_back(account_reading_list_manager_->GetRoot());
  }

  if (local_or_syncable_reading_list_manager_->GetRoot()) {
    top_level_folders.push_back(
        local_or_syncable_reading_list_manager_->GetRoot());
  }

  return top_level_folders;
}

ScopedJavaLocalRef<jobject> BookmarkBridge::GetRootFolderId(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const BookmarkNode* root_node = bookmark_model_->root_node();
  ScopedJavaLocalRef<jobject> folder_id_obj = JavaBookmarkIdCreateBookmarkId(
      env, root_node->id(), GetBookmarkType(root_node));
  return folder_id_obj;
}

ScopedJavaLocalRef<jobject> BookmarkBridge::GetMobileFolderId(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const BookmarkNode* mobile_node = bookmark_model_->mobile_node();
  ScopedJavaLocalRef<jobject> folder_id_obj = JavaBookmarkIdCreateBookmarkId(
      env, mobile_node->id(), GetBookmarkType(mobile_node));
  return folder_id_obj;
}

ScopedJavaLocalRef<jobject> BookmarkBridge::GetOtherFolderId(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const BookmarkNode* other_node = bookmark_model_->other_node();
  ScopedJavaLocalRef<jobject> folder_id_obj = JavaBookmarkIdCreateBookmarkId(
      env, other_node->id(), GetBookmarkType(other_node));
  return folder_id_obj;
}

ScopedJavaLocalRef<jobject> BookmarkBridge::GetDesktopFolderId(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const BookmarkNode* desktop_node = bookmark_model_->bookmark_bar_node();
  ScopedJavaLocalRef<jobject> folder_id_obj = JavaBookmarkIdCreateBookmarkId(
      env, desktop_node->id(), GetBookmarkType(desktop_node));
  return folder_id_obj;
}

ScopedJavaLocalRef<jobject> BookmarkBridge::GetAccountMobileFolderId(
    JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const BookmarkNode* mobile_node = bookmark_model_->account_mobile_node();
  ScopedJavaLocalRef<jobject> folder_id_obj = JavaBookmarkIdCreateBookmarkId(
      env, mobile_node->id(), GetBookmarkType(mobile_node));
  return folder_id_obj;
}

ScopedJavaLocalRef<jobject> BookmarkBridge::GetAccountOtherFolderId(
    JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const BookmarkNode* other_node = bookmark_model_->account_other_node();
  ScopedJavaLocalRef<jobject> folder_id_obj = JavaBookmarkIdCreateBookmarkId(
      env, other_node->id(), GetBookmarkType(other_node));
  return folder_id_obj;
}

ScopedJavaLocalRef<jobject> BookmarkBridge::GetAccountDesktopFolderId(
    JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const BookmarkNode* desktop_node =
      bookmark_model_->account_bookmark_bar_node();
  ScopedJavaLocalRef<jobject> folder_id_obj = JavaBookmarkIdCreateBookmarkId(
      env, desktop_node->id(), GetBookmarkType(desktop_node));
  return folder_id_obj;
}

ScopedJavaLocalRef<jobject> BookmarkBridge::GetPartnerFolderId(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!partner_bookmarks_shim_->IsLoaded()) {
    return nullptr;
  }

  const BookmarkNode* partner_node =
      partner_bookmarks_shim_->GetPartnerBookmarksRoot();
  if (!partner_node) {
    return nullptr;
  }

  ScopedJavaLocalRef<jobject> folder_id_obj = JavaBookmarkIdCreateBookmarkId(
      env, partner_node->id(), GetBookmarkType(partner_node));
  return folder_id_obj;
}

base::android::ScopedJavaLocalRef<jobject>
BookmarkBridge::GetLocalOrSyncableReadingListFolder(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const BookmarkNode* root_node =
      local_or_syncable_reading_list_manager_->GetRoot();
  ScopedJavaLocalRef<jobject> folder_id_obj = JavaBookmarkIdCreateBookmarkId(
      env, root_node->id(), GetBookmarkType(root_node));
  return folder_id_obj;
}

base::android::ScopedJavaLocalRef<jobject>
BookmarkBridge::GetAccountReadingListFolder(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!account_reading_list_manager_) {
    return nullptr;
  }

  const BookmarkNode* root_node = account_reading_list_manager_->GetRoot();
  ScopedJavaLocalRef<jobject> folder_id_obj = JavaBookmarkIdCreateBookmarkId(
      env, root_node->id(), GetBookmarkType(root_node));
  return folder_id_obj;
}

// TODO(crbug.com/1501998): Add logic to determine when to use account/local.
base::android::ScopedJavaLocalRef<jobject>
BookmarkBridge::GetDefaultReadingListFolder(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return GetLocalOrSyncableReadingListFolder(env);
}

base::android::ScopedJavaLocalRef<jstring>
BookmarkBridge::GetBookmarkGuidByIdForTesting(
    JNIEnv* env,
    jlong id,
    jint type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const BookmarkNode* node = GetNodeByID(id, type);
  DCHECK(node) << "Bookmark with id " << id << " doesn't exist.";
  return base::android::ConvertUTF8ToJavaString(
      env, node->uuid().AsLowercaseString());
}

jint BookmarkBridge::GetChildCount(JNIEnv* env, jlong id, jint type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsLoaded());
  const BookmarkNode* node = GetNodeByID(id, type);
  return static_cast<jint>(node->children().size());
}

void BookmarkBridge::GetChildIds(JNIEnv* env,
                                 jlong id,
                                 jint type,
                                 const JavaParamRef<jobject>& j_result_obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsLoaded());

  const BookmarkNode* parent = GetNodeByID(id, type);
  if (!parent->is_folder() || !IsReachable(parent))
    return;

  AddBookmarkNodesToBookmarkIdList(env, j_result_obj, GetChildIdsImpl(parent));
}

std::vector<const bookmarks::BookmarkNode*> BookmarkBridge::GetChildIdsImpl(
    const bookmarks::BookmarkNode* parent) {
  std::vector<const BookmarkNode*> children;
  for (const auto& child : parent->children()) {
    if (IsFolderAvailable(child.get()) && IsReachable(child.get())) {
      children.push_back(child.get());
    }
  }

  if (parent == bookmark_model_->mobile_node() &&
      partner_bookmarks_shim_->HasPartnerBookmarks() &&
      IsReachable(partner_bookmarks_shim_->GetPartnerBookmarksRoot())) {
    children.push_back(partner_bookmarks_shim_->GetPartnerBookmarksRoot());
  }

  return children;
}

ScopedJavaLocalRef<jobject> BookmarkBridge::GetChildAt(
    JNIEnv* env,
    jlong id,
    jint type,
    jint index) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsLoaded());

  const BookmarkNode* parent = GetNodeByID(id, type);
  DCHECK(parent);
  const BookmarkNode* child =
      parent->children()[static_cast<size_t>(index)].get();
  return JavaBookmarkIdCreateBookmarkId(env, child->id(),
                                        GetBookmarkType(child));
}

jint BookmarkBridge::GetTotalBookmarkCount(
    JNIEnv* env,
    jlong id,
    jint type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsLoaded());

  std::queue<const BookmarkNode*> nodes;
  const BookmarkNode* parent = GetNodeByID(id, type);
  DCHECK(parent->is_folder());

  int count = 0;
  nodes.push(parent);
  while (!nodes.empty()) {
    const BookmarkNode* node = nodes.front();
    nodes.pop();

    for (const auto& child : node->children()) {
      // Do not count deleted partner bookmarks or folders, which will have
      // empty titles. See PartnerBookmarkShim::RemoveBookmark().
      if (partner_bookmarks_shim_->IsPartnerBookmark(child.get()) &&
          partner_bookmarks_shim_->GetTitle(child.get()).empty())
        continue;
      if (child->is_folder())
        nodes.push(child.get());
      else
        ++count;
    }
    // If we are looking at the mobile bookmarks folder,
    // and we have partner bookmarks
    if (node == bookmark_model_->mobile_node() &&
        partner_bookmarks_shim_->HasPartnerBookmarks() &&
        IsReachable(partner_bookmarks_shim_->GetPartnerBookmarksRoot())) {
      nodes.push(partner_bookmarks_shim_->GetPartnerBookmarksRoot());
    }
  }

  return count;
}

void BookmarkBridge::SetBookmarkTitle(JNIEnv* env,
                                      jlong id,
                                      jint type,
                                      const JavaParamRef<jstring>& j_title) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsLoaded());

  const BookmarkNode* bookmark = GetNodeByID(id, type);
  const std::u16string title =
      base::android::ConvertJavaStringToUTF16(env, j_title);

  if (partner_bookmarks_shim_->IsPartnerBookmark(bookmark)) {
    partner_bookmarks_shim_->RenameBookmark(bookmark, title);
  } else if (local_or_syncable_reading_list_manager_->IsReadingListBookmark(
                 bookmark)) {
    local_or_syncable_reading_list_manager_->SetTitle(bookmark->url(), title);
  } else if (account_reading_list_manager_ &&
             account_reading_list_manager_->IsReadingListBookmark(bookmark)) {
    account_reading_list_manager_->SetTitle(bookmark->url(), title);
  } else {
    bookmark_model_->SetTitle(bookmark, title,
                              bookmarks::metrics::BookmarkEditSource::kUser);
  }
}

void BookmarkBridge::SetBookmarkUrl(JNIEnv* env,
                                    jlong id,
                                    jint type,
                                    const JavaParamRef<jobject>& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsLoaded());
  bookmark_model_->SetURL(GetNodeByID(id, type),
                          *url::GURLAndroid::ToNativeGURL(env, url),
                          bookmarks::metrics::BookmarkEditSource::kUser);
}

void BookmarkBridge::SetPowerBookmarkMeta(
    JNIEnv* env,
    jlong id,
    jint type,
    const JavaParamRef<jbyteArray>& bytes) {
  const BookmarkNode* node = GetNodeByID(id, type);
  if (!node || bytes.is_null())
    return;

  std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
      std::make_unique<power_bookmarks::PowerBookmarkMeta>();
  std::vector<uint8_t> byte_vec;
  base::android::JavaByteArrayToByteVector(env, bytes, &byte_vec);
  if (meta->ParseFromArray(byte_vec.data(), byte_vec.size())) {
    power_bookmarks::SetNodePowerBookmarkMeta(bookmark_model_, node,
                                              std::move(meta));
  } else {
    DCHECK(false) << "Failed to parse bytes from java into PowerBookmarkMeta!";
  }
}

ScopedJavaLocalRef<jbyteArray> BookmarkBridge::GetPowerBookmarkMeta(
    JNIEnv* env,
    jlong id,
    jint type) {
  const BookmarkNode* node = GetNodeByID(id, type);
  std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
      power_bookmarks::GetNodePowerBookmarkMeta(bookmark_model_, node);

  if (!meta)
    return ScopedJavaLocalRef<jbyteArray>(nullptr);

  int size = meta->ByteSize();
  std::string proto_bytes;
  meta->SerializeToString(&proto_bytes);
  std::vector<uint8_t> data;
  data.resize(size);
  meta->SerializeToArray(data.data(), size);

  return base::android::ToJavaByteArray(env, data.data(), size);
}

void BookmarkBridge::DeletePowerBookmarkMeta(
    JNIEnv* env,
    jlong id,
    jint type) {
  const BookmarkNode* node = GetNodeByID(id, type);

  if (!node)
    return;

  power_bookmarks::DeleteNodePowerBookmarkMeta(bookmark_model_, node);
}

bool BookmarkBridge::DoesBookmarkExist(JNIEnv* env, jlong id, jint type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsLoaded());

  const BookmarkNode* node = GetNodeByID(id, type);

  if (!node)
    return false;

  if (type == BookmarkType::BOOKMARK_TYPE_NORMAL ||
      type == BookmarkType::BOOKMARK_TYPE_READING_LIST) {
    return true;
  } else {
    DCHECK(type == BookmarkType::BOOKMARK_TYPE_PARTNER);
    return partner_bookmarks_shim_->IsReachable(node);
  }
}

void BookmarkBridge::GetBookmarksForFolder(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_folder_id_obj,
    const JavaParamRef<jobject>& j_result_obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsLoaded());

  long folder_id = JavaBookmarkIdGetId(env, j_folder_id_obj);
  int type = JavaBookmarkIdGetType(env, j_folder_id_obj);
  const BookmarkNode* folder = GetFolderWithFallback(folder_id, type);

  if (!folder->is_folder() || !IsReachable(folder))
    return;

  // Recreate the java bookmarkId object due to fallback.
  ScopedJavaLocalRef<jobject> folder_id_obj = JavaBookmarkIdCreateBookmarkId(
      env, folder->id(), GetBookmarkType(folder));

  // Get the folder contents.
  for (const auto& node : folder->children()) {
    if (IsFolderAvailable(node.get()))
      ExtractBookmarkNodeInformation(node.get(), j_result_obj);
  }

  if (folder == bookmark_model_->mobile_node() &&
      partner_bookmarks_shim_->HasPartnerBookmarks()) {
    ExtractBookmarkNodeInformation(
        partner_bookmarks_shim_->GetPartnerBookmarksRoot(), j_result_obj);
  }
}

jboolean BookmarkBridge::IsFolderVisible(JNIEnv* env, jlong id, jint type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (type == BookmarkType::BOOKMARK_TYPE_NORMAL ||
      type == BookmarkType::BOOKMARK_TYPE_READING_LIST) {
    const BookmarkNode* node = bookmarks::GetBookmarkNodeByID(
        bookmark_model_, static_cast<int64_t>(id));
    return node->IsVisible();
  }
  DCHECK_EQ(BookmarkType::BOOKMARK_TYPE_PARTNER, type);
  const BookmarkNode* node =
      partner_bookmarks_shim_->GetNodeByID(static_cast<long>(id));
  return partner_bookmarks_shim_->IsReachable(node);
}

void BookmarkBridge::SearchBookmarks(JNIEnv* env,
                                     const JavaParamRef<jobject>& j_list,
                                     const JavaParamRef<jstring>& j_query,
                                     const JavaParamRef<jobjectArray>& j_tags,
                                     jint type,
                                     jint max_results) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(bookmark_model_->loaded());

  power_bookmarks::PowerBookmarkQueryFields query;
  query.word_phrase_query = std::make_unique<std::u16string>(
      base::android::ConvertJavaStringToUTF16(env, j_query));
  if (query.word_phrase_query->empty()) {
    query.word_phrase_query.reset();
  }

  if (!j_tags.is_null()) {
    base::android::AppendJavaStringArrayToStringVector(env, j_tags,
                                                       &query.tags);
  }

  if (type >= 0) {
    query.type = static_cast<power_bookmarks::PowerBookmarkType>(type);
  }

  std::vector<const BookmarkNode*> results =
      SearchBookmarksImpl(query, max_results);
  AddBookmarkNodesToBookmarkIdList(env, j_list, results);
}

std::vector<const BookmarkNode*> BookmarkBridge::SearchBookmarksImpl(
    power_bookmarks::PowerBookmarkQueryFields& query,
    int max_results) {
  std::vector<const BookmarkNode*> results;
  power_bookmarks::GetBookmarksMatchingProperties(bookmark_model_, query,
                                                  max_results, &results);

  local_or_syncable_reading_list_manager_->GetMatchingNodes(query, max_results,
                                                            &results);
  if (account_reading_list_manager_) {
    account_reading_list_manager_->GetMatchingNodes(query, max_results,
                                                    &results);
  }
  if (partner_bookmarks_shim_->HasPartnerBookmarks() &&
      IsReachable(partner_bookmarks_shim_->GetPartnerBookmarksRoot())) {
    partner_bookmarks_shim_->GetPartnerBookmarksMatchingProperties(
        query, max_results, &results);
  }
  DCHECK((int)results.size() <= max_results || max_results == -1);

  FilterUnreachableBookmarks(&results);
  return results;
}

void BookmarkBridge::GetBookmarksOfType(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_list,
    jint type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::vector<const BookmarkNode*> results;
  power_bookmarks::PowerBookmarkQueryFields query;
  query.type = static_cast<power_bookmarks::PowerBookmarkType>(type);
  power_bookmarks::GetBookmarksMatchingProperties(bookmark_model_, query, -1,
                                                  &results);

  FilterUnreachableBookmarks(&results);
  AddBookmarkNodesToBookmarkIdList(env, j_list, results);
}

ScopedJavaLocalRef<jobject> BookmarkBridge::AddFolder(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_parent_id_obj,
    jint index,
    const JavaParamRef<jstring>& j_title) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsLoaded());

  long bookmark_id = JavaBookmarkIdGetId(env, j_parent_id_obj);
  int type = JavaBookmarkIdGetType(env, j_parent_id_obj);
  const BookmarkNode* parent = GetNodeByID(bookmark_id, type);

  const BookmarkNode* new_node = bookmark_model_->AddFolder(
      parent, static_cast<size_t>(index),
      base::android::ConvertJavaStringToUTF16(env, j_title));
  DCHECK(new_node);
  ScopedJavaLocalRef<jobject> new_java_obj = JavaBookmarkIdCreateBookmarkId(
      env, new_node->id(), GetBookmarkType(new_node));
  return new_java_obj;
}

void BookmarkBridge::DeleteBookmark(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_bookmark_id_obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsLoaded());

  long bookmark_id = JavaBookmarkIdGetId(env, j_bookmark_id_obj);
  int type = JavaBookmarkIdGetType(env, j_bookmark_id_obj);
  const BookmarkNode* node = GetNodeByID(bookmark_id, type);

  // TODO(crbug.com/1425438): Switch to an early returns after debugging why
  // this is called with a nullptr.
  if (!node) {
    LOG(ERROR) << "Deleting null bookmark, type:" << type;
    NOTREACHED();
    return;
  }

  // TODO(crbug.com/1425438): Switch back to a D/CHECK after debugging
  // why this is called with an uneditable node.
  // See https://crbug.com/981172.
  if (!IsEditable(node)) {
    LOG(ERROR) << "Deleting non editable bookmark, type:" << type;
    NOTREACHED();
    return;
  }

  if (partner_bookmarks_shim_->IsPartnerBookmark(node)) {
    partner_bookmarks_shim_->RemoveBookmark(node);
  } else if (type == BookmarkType::BOOKMARK_TYPE_READING_LIST) {
    const BookmarkNode* reading_list_parent = node->parent();
    ReadingListManager* reading_list_manager =
        GetReadingListManagerFromParentNode(reading_list_parent);

    size_t index = reading_list_parent->GetIndexOf(node).value();
    // Intentionally left empty.
    std::set<GURL> removed_urls;
    // Observer must be trigger prior, the underlying BookmarkNode* will be
    // deleted immediately after the delete call.
    BookmarkNodeRemoved(bookmark_model_, reading_list_parent, index, node,
                        removed_urls);

    // Inside the Delete method, node will be destroyed and node->url will be
    // also destroyed. This causes heap-use-after-free at
    // ReadingListModelImpl::RemoveEntryByURLImpl. To avoid the
    // heap-use-after-free, make a copy of node->url() and use it.
    GURL url(node->url());
    reading_list_manager->Delete(url);
  } else {
    bookmark_model_->Remove(node,
                            bookmarks::metrics::BookmarkEditSource::kUser);
  }
}

void BookmarkBridge::RemoveAllUserBookmarks(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsLoaded());
  bookmark_model_->RemoveAllUserBookmarks();
}

void BookmarkBridge::MoveBookmark(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_bookmark_id_obj,
    const JavaParamRef<jobject>& j_parent_id_obj,
    jint index) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsLoaded());

  long bookmark_id = JavaBookmarkIdGetId(env, j_bookmark_id_obj);
  int type = JavaBookmarkIdGetType(env, j_bookmark_id_obj);
  const BookmarkNode* node = GetNodeByID(bookmark_id, type);
  DCHECK(IsEditable(node));
  bookmark_id = JavaBookmarkIdGetId(env, j_parent_id_obj);
  type = JavaBookmarkIdGetType(env, j_parent_id_obj);
  const BookmarkNode* new_parent_node = GetNodeByID(bookmark_id, type);
  // Bookmark should not be moved to its own parent folder
  if (node->parent() != new_parent_node) {
    bookmark_model_->Move(node, new_parent_node, static_cast<size_t>(index));
  }
}

ScopedJavaLocalRef<jobject> BookmarkBridge::AddBookmark(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_parent_id_obj,
    jint index,
    const JavaParamRef<jstring>& j_title,
    const JavaParamRef<jobject>& j_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsLoaded());
  long bookmark_id = JavaBookmarkIdGetId(env, j_parent_id_obj);
  int type = JavaBookmarkIdGetType(env, j_parent_id_obj);
  const BookmarkNode* parent = GetNodeByID(bookmark_id, type);

  std::unique_ptr<GURL> url = url::GURLAndroid::ToNativeGURL(env, j_url);

  const BookmarkNode* new_node = bookmark_model_->AddNewURL(
      parent, static_cast<size_t>(index),
      base::android::ConvertJavaStringToUTF16(env, j_title), *url);
  DCHECK(new_node);
  ScopedJavaLocalRef<jobject> new_java_obj = JavaBookmarkIdCreateBookmarkId(
      env, new_node->id(), GetBookmarkType(new_node));
  return new_java_obj;
}

ScopedJavaLocalRef<jobject> BookmarkBridge::AddToReadingList(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_parent_id_obj,
    const JavaParamRef<jstring>& j_title,
    const JavaParamRef<jobject>& j_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsLoaded());

  const BookmarkNode* parent_node =
      GetNodeByID(JavaBookmarkIdGetId(env, j_parent_id_obj),
                  JavaBookmarkIdGetType(env, j_parent_id_obj));
  ReadingListManager* manager =
      GetReadingListManagerFromParentNode(parent_node);

  const BookmarkNode* node =
      manager->Add(*url::GURLAndroid::ToNativeGURL(env, j_url),
                   base::android::ConvertJavaStringToUTF8(env, j_title));
  return node ? JavaBookmarkIdCreateBookmarkId(env, node->id(),
                                               GetBookmarkType(node))
              : ScopedJavaLocalRef<jobject>();
}

void BookmarkBridge::SetReadStatus(JNIEnv* env,
                                   const JavaParamRef<jobject>& j_id,
                                   jboolean j_read) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsLoaded());

  const BookmarkNode* node = GetNodeByID(JavaBookmarkIdGetId(env, j_id),
                                         JavaBookmarkIdGetType(env, j_id));
  ReadingListManager* manager =
      GetReadingListManagerFromParentNode(node->parent());

  manager->SetReadStatus(node->url(), j_read);
}

int BookmarkBridge::GetUnreadCount(JNIEnv* env,
                                   const JavaParamRef<jobject>& j_id) {
  const BookmarkNode* node = GetNodeByID(JavaBookmarkIdGetId(env, j_id),
                                         JavaBookmarkIdGetType(env, j_id));
  ReadingListManager* manager = GetReadingListManagerFromParentNode(node);

  int count = 0;
  for (const auto& child_node : manager->GetRoot()->children()) {
    count += manager->GetReadStatus(child_node.get()) ? 0 : 1;
  }
  return count;
}

jboolean BookmarkBridge::IsAccountBookmark(JNIEnv* env,
                                           const JavaParamRef<jobject>& j_id) {
  return IsAccountBookmarkImpl(GetNodeByID(JavaBookmarkIdGetId(env, j_id),
                                           JavaBookmarkIdGetType(env, j_id)));
}

bool BookmarkBridge::IsAccountBookmarkImpl(const BookmarkNode* node) {
  if (account_reading_list_manager_ &&
      account_reading_list_manager_->IsReadingListBookmark(node)) {
    return true;
  }

  std::set<const BookmarkNode*> account_bookmark_root_folders = {
      bookmark_model_->account_bookmark_bar_node(),
      bookmark_model_->account_other_node(),
      bookmark_model_->account_mobile_node()};
  while (node != nullptr) {
    if (account_bookmark_root_folders.find(node) !=
        account_bookmark_root_folders.end()) {
      return true;
    }
    node = node->parent();
  }

  return false;
}

void BookmarkBridge::Undo(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsLoaded());
  BookmarkUndoService* undo_service =
      BookmarkUndoServiceFactory::GetForProfile(profile_);
  UndoManager* undo_manager = undo_service->undo_manager();
  undo_manager->Undo();
}

void BookmarkBridge::StartGroupingUndos(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsLoaded());
  DCHECK(!grouped_bookmark_actions_.get());  // shouldn't have started already
  grouped_bookmark_actions_ =
      std::make_unique<bookmarks::ScopedGroupBookmarkActions>(bookmark_model_);
}

void BookmarkBridge::EndGroupingUndos(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsLoaded());
  DCHECK(grouped_bookmark_actions_.get());  // should only call after start
  grouped_bookmark_actions_.reset();
}

bool BookmarkBridge::IsBookmarked(JNIEnv* env,
                                  const JavaParamRef<jobject>& gurl) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return bookmark_model_->IsBookmarked(
      *url::GURLAndroid::ToNativeGURL(env, gurl));
}

std::u16string BookmarkBridge::GetTitle(const BookmarkNode* node) const {
  if (partner_bookmarks_shim_->IsPartnerBookmark(node))
    return partner_bookmarks_shim_->GetTitle(node);

  return node->GetTitle();
}

ScopedJavaLocalRef<jobject> BookmarkBridge::CreateJavaBookmark(
    const BookmarkNode* node) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  JNIEnv* env = AttachCurrentThread();

  const BookmarkNode* parent = GetParentNode(node);
  int64_t parent_id = parent ? parent->id() : -1;

  GURL url;
  if (node->is_url())
    url = node->url();

  int type = GetBookmarkType(node);
  bool read = false;
  if (account_reading_list_manager_ &&
      account_reading_list_manager_->IsReadingListBookmark(node)) {
    read = account_reading_list_manager_->GetReadStatus(node);
  } else if (local_or_syncable_reading_list_manager_->IsReadingListBookmark(
                 node)) {
    read = local_or_syncable_reading_list_manager_->GetReadStatus(node);
  }

  // TODO(crbug.com/1467559): Folders need to use most recent child's time for
  // date_last_used.
  return Java_BookmarkBridge_createBookmarkItem(
      env, node->id(), type, ConvertUTF16ToJavaString(env, GetTitle(node)),
      url::GURLAndroid::FromNativeGURL(env, url), node->is_folder(), parent_id,
      GetBookmarkType(parent), IsEditable(node), IsManaged(node),
      node->date_added().InMillisecondsSinceUnixEpoch(), read,
      node->date_last_used().InMillisecondsSinceUnixEpoch(),
      IsAccountBookmarkImpl(node));
}

void BookmarkBridge::ExtractBookmarkNodeInformation(
    const BookmarkNode* node,
    const JavaRef<jobject>& j_result_obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  if (!IsReachable(node))
    return;
  Java_BookmarkBridge_addToList(env, j_result_obj, CreateJavaBookmark(node));
}

const BookmarkNode* BookmarkBridge::GetNodeByID(long node_id, int type) {
  const BookmarkNode* node = nullptr;
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (type == BookmarkType::BOOKMARK_TYPE_PARTNER) {
    node = partner_bookmarks_shim_->GetNodeByID(static_cast<int64_t>(node_id));
  } else if (type == BookmarkType::BOOKMARK_TYPE_READING_LIST) {
    // First check the account reading list if it's available.
    if (account_reading_list_manager_) {
      node = account_reading_list_manager_->GetNodeByID(
          static_cast<int64_t>(node_id));
    }

    // If there were no results, check the local/syncable reading list.
    if (!node) {
      node = local_or_syncable_reading_list_manager_->GetNodeByID(
          static_cast<int64_t>(node_id));
    }
  } else {
    node = bookmarks::GetBookmarkNodeByID(bookmark_model_,
                                          static_cast<int64_t>(node_id));
  }
  return node;
}

const BookmarkNode* BookmarkBridge::GetFolderWithFallback(long folder_id,
                                                          int type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const BookmarkNode* folder = GetNodeByID(folder_id, type);
  if (!folder || folder->type() == BookmarkNode::URL ||
      !IsFolderAvailable(folder)) {
    if (!managed_bookmark_service_->managed_node()->children().empty())
      folder = managed_bookmark_service_->managed_node();
    else
      folder = bookmark_model_->mobile_node();
  }
  return folder;
}

bool BookmarkBridge::IsEditBookmarksEnabled() const {
  return profile_->GetPrefs()->GetBoolean(
      bookmarks::prefs::kEditBookmarksEnabled);
}

void BookmarkBridge::EditBookmarksEnabledChanged() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!java_bookmark_model_)
    return;

  Java_BookmarkBridge_editBookmarksEnabledChanged(
      AttachCurrentThread(), ScopedJavaLocalRef<jobject>(java_bookmark_model_));
}

bool BookmarkBridge::IsEditable(const BookmarkNode* node) const {
  if (!node || (node->type() != BookmarkNode::FOLDER &&
                node->type() != BookmarkNode::URL)) {
    return false;
  }
  if (!IsEditBookmarksEnabled() || bookmark_model_->is_permanent_node(node))
    return false;
  if (partner_bookmarks_shim_->IsPartnerBookmark(node)) {
    return partner_bookmarks_shim_->IsEditable(node);
  }
  if (local_or_syncable_reading_list_manager_->IsReadingListBookmark(node)) {
    return local_or_syncable_reading_list_manager_->GetRoot() != node;
  }
  if (account_reading_list_manager_ &&
      account_reading_list_manager_->IsReadingListBookmark(node)) {
    return account_reading_list_manager_->GetRoot() != node;
  }

  return !managed_bookmark_service_->IsNodeManaged(node);
}

bool BookmarkBridge::IsManaged(const BookmarkNode* node) const {
  return bookmarks::IsDescendantOf(node,
                                   managed_bookmark_service_->managed_node());
}

const BookmarkNode* BookmarkBridge::GetParentNode(const BookmarkNode* node) {
  DCHECK(IsLoaded());
  if (node == partner_bookmarks_shim_->GetPartnerBookmarksRoot())
    return bookmark_model_->mobile_node();

  if (node == local_or_syncable_reading_list_manager_->GetRoot()) {
    return bookmark_model_->root_node();
  }
  if (account_reading_list_manager_ &&
      node == account_reading_list_manager_->GetRoot()) {
    return bookmark_model_->root_node();
  }

  return node->parent();
}

int BookmarkBridge::GetBookmarkType(const BookmarkNode* node) {
  // TODO(crbug.com/1150559) return the wrong type when the backend is not
  // loaded?
  if (partner_bookmarks_shim_->IsLoaded() &&
      partner_bookmarks_shim_->IsPartnerBookmark(node))
    return BookmarkType::BOOKMARK_TYPE_PARTNER;

  if (local_or_syncable_reading_list_manager_->IsLoaded() &&
      local_or_syncable_reading_list_manager_->IsReadingListBookmark(node)) {
    return BookmarkType::BOOKMARK_TYPE_READING_LIST;
  }
  if (account_reading_list_manager_ &&
      account_reading_list_manager_->IsLoaded() &&
      account_reading_list_manager_->IsReadingListBookmark(node)) {
    return BookmarkType::BOOKMARK_TYPE_READING_LIST;
  }

  return BookmarkType::BOOKMARK_TYPE_NORMAL;
}

bool BookmarkBridge::IsReachable(const BookmarkNode* node) const {
  if (!partner_bookmarks_shim_->IsPartnerBookmark(node))
    return true;
  return partner_bookmarks_shim_->IsReachable(node);
}

bool BookmarkBridge::IsLoaded() const {
  return (bookmark_model_->loaded() && partner_bookmarks_shim_->IsLoaded() &&
          local_or_syncable_reading_list_manager_->IsLoaded() &&
          (!account_reading_list_manager_ ||
           account_reading_list_manager_->IsLoaded()));
}

bool BookmarkBridge::IsFolderAvailable(const BookmarkNode* folder) const {
  // The managed bookmarks folder is not shown if there are no bookmarks
  // configured via policy.
  if (folder == managed_bookmark_service_->managed_node() &&
      folder->children().empty())
    return false;

  auto* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_->GetOriginalProfile());
  return (folder->type() != BookmarkNode::BOOKMARK_BAR &&
          folder->type() != BookmarkNode::OTHER_NODE) ||
         (identity_manager &&
          identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
}

void BookmarkBridge::NotifyIfDoneLoading() {
  if (!IsLoaded() || !java_bookmark_model_)
    return;

  Java_BookmarkBridge_bookmarkModelLoaded(
      AttachCurrentThread(), ScopedJavaLocalRef<jobject>(java_bookmark_model_));
}

void BookmarkBridge::AddBookmarkNodesToBookmarkIdList(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_result_obj,
    const std::vector<const BookmarkNode*>& nodes) {
  for (const auto* node : nodes) {
    Java_BookmarkBridge_addToBookmarkIdList(env, j_result_obj, node->id(),
                                            GetBookmarkType(node));
  }
}

void BookmarkBridge::FilterUnreachableBookmarks(
    std::vector<const bookmarks::BookmarkNode*>* nodes) {
  std::erase_if(*nodes, [this](const bookmarks::BookmarkNode* node) {
    return !IsReachable(node);
  });
}

// ------------- Observer-related methods ------------- //

// Called when there are changes to the bookmark model. It is most
// likely changes to the partner bookmarks.
void BookmarkBridge::BookmarkModelChanged() {
  if (!IsLoaded() || !java_bookmark_model_)
    return;

  Java_BookmarkBridge_bookmarkModelChanged(
      AttachCurrentThread(), ScopedJavaLocalRef<jobject>(java_bookmark_model_));
}

void BookmarkBridge::BookmarkModelLoaded(BookmarkModel* model,
                                         bool ids_reassigned) {
  NotifyIfDoneLoading();
}

void BookmarkBridge::BookmarkModelBeingDeleted(BookmarkModel* model) {
  if (!IsLoaded())
    return;

  DestroyJavaObject();
}

void BookmarkBridge::BookmarkNodeMoved(BookmarkModel* model,
                                       const BookmarkNode* old_parent,
                                       size_t old_index,
                                       const BookmarkNode* new_parent,
                                       size_t new_index) {
  if (!IsLoaded() || !java_bookmark_model_)
    return;

  Java_BookmarkBridge_bookmarkNodeMoved(
      AttachCurrentThread(), ScopedJavaLocalRef<jobject>(java_bookmark_model_),
      CreateJavaBookmark(old_parent), static_cast<int>(old_index),
      CreateJavaBookmark(new_parent), static_cast<int>(new_index));
}

void BookmarkBridge::BookmarkNodeAdded(BookmarkModel* model,
                                       const BookmarkNode* parent,
                                       size_t index,
                                       bool added_by_user) {
  if (!IsLoaded() || !java_bookmark_model_)
    return;

  Java_BookmarkBridge_bookmarkNodeAdded(
      AttachCurrentThread(), ScopedJavaLocalRef<jobject>(java_bookmark_model_),
      CreateJavaBookmark(parent), static_cast<int>(index));
}

void BookmarkBridge::BookmarkNodeRemoved(BookmarkModel* model,
                                         const BookmarkNode* parent,
                                         size_t old_index,
                                         const BookmarkNode* node,
                                         const std::set<GURL>& removed_urls) {
  if (!IsLoaded() || !java_bookmark_model_)
    return;

  Java_BookmarkBridge_bookmarkNodeRemoved(
      AttachCurrentThread(), ScopedJavaLocalRef<jobject>(java_bookmark_model_),
      CreateJavaBookmark(parent), static_cast<int>(old_index),
      CreateJavaBookmark(node));
}

void BookmarkBridge::BookmarkAllUserNodesRemoved(
    BookmarkModel* model,
    const std::set<GURL>& removed_urls) {
  if (!IsLoaded() || !java_bookmark_model_)
    return;

  Java_BookmarkBridge_bookmarkAllUserNodesRemoved(
      AttachCurrentThread(), ScopedJavaLocalRef<jobject>(java_bookmark_model_));
}

void BookmarkBridge::BookmarkNodeChanged(BookmarkModel* model,
                                         const BookmarkNode* node) {
  if (!IsLoaded() || !java_bookmark_model_)
    return;

  Java_BookmarkBridge_bookmarkNodeChanged(
      AttachCurrentThread(), ScopedJavaLocalRef<jobject>(java_bookmark_model_),
      CreateJavaBookmark(node));
}

void BookmarkBridge::BookmarkNodeChildrenReordered(BookmarkModel* model,
                                                   const BookmarkNode* node) {
  if (!IsLoaded() || !java_bookmark_model_)
    return;

  Java_BookmarkBridge_bookmarkNodeChildrenReordered(
      AttachCurrentThread(), ScopedJavaLocalRef<jobject>(java_bookmark_model_),
      CreateJavaBookmark(node));
}

void BookmarkBridge::ExtensiveBookmarkChangesBeginning(BookmarkModel* model) {
  if (!IsLoaded() || !java_bookmark_model_)
    return;

  Java_BookmarkBridge_extensiveBookmarkChangesBeginning(
      AttachCurrentThread(), ScopedJavaLocalRef<jobject>(java_bookmark_model_));
}

void BookmarkBridge::ExtensiveBookmarkChangesEnded(BookmarkModel* model) {
  if (!IsLoaded() || !java_bookmark_model_)
    return;

  Java_BookmarkBridge_extensiveBookmarkChangesEnded(
      AttachCurrentThread(), ScopedJavaLocalRef<jobject>(java_bookmark_model_));
}

void BookmarkBridge::PartnerShimChanged(PartnerBookmarksShim* shim) {
  BookmarkModelChanged();
}

void BookmarkBridge::PartnerShimLoaded(PartnerBookmarksShim* shim) {
  NotifyIfDoneLoading();
}

void BookmarkBridge::ShimBeingDeleted(PartnerBookmarksShim* shim) {
  partner_bookmarks_shim_ = nullptr;
}

void BookmarkBridge::ReadingListLoaded() {
  NotifyIfDoneLoading();
}

void BookmarkBridge::ReadingListChanged() {
  BookmarkModelChanged();
}

void BookmarkBridge::ReorderChildren(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_bookmark_id_obj,
    jlongArray arr) {
  DCHECK(IsLoaded());
  // get the BookmarkNode* for the "parent" bookmark parameter
  const long bookmark_id = JavaBookmarkIdGetId(env, j_bookmark_id_obj);
  const int bookmark_type = JavaBookmarkIdGetType(env, j_bookmark_id_obj);

  const BookmarkNode* bookmark_node = GetNodeByID(bookmark_id, bookmark_type);

  // populate a vector
  std::vector<const BookmarkNode*> ordered_nodes;
  jsize arraySize = env->GetArrayLength(arr);
  jlong* elements = env->GetLongArrayElements(arr, 0);

  // iterate through array, adding the BookmarkNode*s of the objects
  for (int i = 0; i < arraySize; ++i) {
    ordered_nodes.push_back(GetNodeByID(elements[i], 0));
  }

  bookmark_model_->ReorderChildren(bookmark_node, ordered_nodes);
}

// Should destroy the bookmark bridge, if OTR profile is destroyed not to delete
// related resources twice.
void BookmarkBridge::OnProfileWillBeDestroyed(Profile* profile) {
  weak_ptr_factory_.InvalidateWeakPtrs();
  DestroyJavaObject();
}

ScopedJavaGlobalRef<jobject> BookmarkBridge::GetJavaBookmarkModel() {
  return java_bookmark_model_;
}

void BookmarkBridge::DestroyJavaObject() {
  if (!java_bookmark_model_)
    return;

  Java_BookmarkBridge_destroyFromNative(
      AttachCurrentThread(), ScopedJavaLocalRef<jobject>(java_bookmark_model_));
}

ReadingListManager* BookmarkBridge::GetReadingListManagerFromParentNode(
    const bookmarks::BookmarkNode* node) {
  if (account_reading_list_manager_ &&
      node == account_reading_list_manager_->GetRoot()) {
    return account_reading_list_manager_.get();
  } else if (node == local_or_syncable_reading_list_manager_->GetRoot()) {
    return local_or_syncable_reading_list_manager_.get();
  }

  NOTREACHED_NORETURN();
}
