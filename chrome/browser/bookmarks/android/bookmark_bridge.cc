// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

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
#include "chrome/browser/android/bookmarks/partner_bookmarks_reader.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/reading_list/android/reading_list_manager.h"
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
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "components/prefs/pref_service.h"
#include "components/query_parser/query_parser.h"
#include "components/reading_list/core/dual_reading_list_model.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/features.h"
#include "components/undo/bookmark_undo_service.h"
#include "components/undo/undo_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/BookmarkBridge_jni.h"

using base::android::AttachCurrentThread;
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

const bookmarks::BookmarkNode* GetNodeFromReadingListIfLoaded(
    const ReadingListManager* manager,
    const GURL& url) {
  if (manager->IsLoaded()) {
    return manager->Get(url);
  }

  return nullptr;
}

}  // namespace

// static
ScopedJavaLocalRef<jobject> JNI_BookmarkBridge_NativeGetForProfile(
    JNIEnv* env,
    Profile* profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!profile)
    return nullptr;

  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(profile);
  if (!model)
    return nullptr;

  BookmarkBridge* bookmark_bridge = static_cast<BookmarkBridge*>(
      model->GetUserData(kBookmarkBridgeUserDataKey));

  if (!bookmark_bridge) {
    // BookmarkModel factory redirects to the original profile, so it might
    // happen that profile refers to the incognito profile, even though we're
    // building the bridge for the regular profile. Some factories don't do
    // this by default, so we need to pass the original profile instead. This
    // is safe to do because BookmarkModel/Bridge is always built for the
    // regular profile.
    auto* original_profile = profile->GetOriginalProfile();
    bookmark_bridge = new BookmarkBridge(
        profile, model,
        ManagedBookmarkServiceFactory::GetForProfile(original_profile),
        ReadingListModelFactory::GetAsDualReadingListForBrowserContext(
            original_profile),
        PartnerBookmarksShim::BuildForBrowserContext(original_profile),
        IdentityManagerFactory::GetForProfile(original_profile));
    model->SetUserData(kBookmarkBridgeUserDataKey,
                       base::WrapUnique(bookmark_bridge));
  }

  return ScopedJavaLocalRef<jobject>(bookmark_bridge->GetJavaBookmarkModel());
}

BookmarkBridge::BookmarkBridge(
    Profile* profile,
    BookmarkModel* model,
    bookmarks::ManagedBookmarkService* managed_bookmark_service,
    reading_list::DualReadingListModel* dual_reading_list_model,
    PartnerBookmarksShim* partner_bookmarks_shim,
    signin::IdentityManager* identity_manager)
    : profile_(profile),
      bookmark_model_(model),
      managed_bookmark_service_(managed_bookmark_service),
      dual_reading_list_model_(dual_reading_list_model),
      id_gen_func_(
          base::BindRepeating([](int64_t* id) { return (*id)++; },
                              base::Owned(std::make_unique<int64_t>(0)))),
      local_or_syncable_reading_list_manager_(
          std::make_unique<ReadingListManagerImpl>(
              dual_reading_list_model->GetLocalOrSyncableModel(),
              id_gen_func_)),
      partner_bookmarks_shim_(partner_bookmarks_shim),
      identity_manager_(identity_manager),
      weak_ptr_factory_(this) {
  CHECK(profile);
  CHECK(model);
  CHECK(managed_bookmark_service);
  CHECK(partner_bookmarks_shim);
  CHECK(dual_reading_list_model);
  CHECK(identity_manager_);

  profile_observation_.Observe(profile_);
  bookmark_model_observation_.Observe(bookmark_model_);
  partner_bookmarks_shim_observation_.Observe(partner_bookmarks_shim_);
  reading_list_manager_observations_.AddObservation(
      local_or_syncable_reading_list_manager_.get());
  dual_reading_list_model_observation_.Observe(dual_reading_list_model_);
  identity_manager_observation_.Observe(identity_manager_);

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
    ExtensiveBookmarkChangesBeginning();

  java_bookmark_model_ = Java_BookmarkBridge_createBookmarkModel(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
}

BookmarkBridge::~BookmarkBridge() {
  reading_list_manager_observations_.RemoveAllObservations();
  partner_bookmarks_shim_observation_.Reset();
  bookmark_model_observation_.Reset();
  profile_observation_.Reset();
}

void BookmarkBridge::Destroy(JNIEnv* env) {
  // This will call the destructor because the user data is a unique pointer.
  bookmark_model_->RemoveUserData(kBookmarkBridgeUserDataKey);
}

jboolean BookmarkBridge::AreAccountBookmarkFoldersActive(JNIEnv* env) {
  if (!base::FeatureList::IsEnabled(
          syncer::kSyncEnableBookmarksInTransportMode)) {
    return false;
  }

  return bookmark_model_->account_mobile_node() != nullptr;
}

base::android::ScopedJavaLocalRef<jobject>
BookmarkBridge::GetMostRecentlyAddedUserBookmarkIdForUrl(JNIEnv* env,
                                                         const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const BookmarkNode* node = GetMostRecentlyAddedUserBookmarkIdForUrlImpl(url);
  if (node) {
    return JavaBookmarkIdCreateBookmarkId(env, node->id(),
                                          GetBookmarkType(node));
  }
  return nullptr;
}

const bookmarks::BookmarkNode*
BookmarkBridge::GetMostRecentlyAddedUserBookmarkIdForUrlImpl(const GURL& url) {
  std::vector<const bookmarks::BookmarkNode*> nodes;
  const auto* reading_list_node = GetNodeFromReadingListIfLoaded(
      local_or_syncable_reading_list_manager_.get(), url);
  if (reading_list_node) {
    nodes.push_back(reading_list_node);
  }

  if (account_reading_list_manager_) {
    reading_list_node = GetNodeFromReadingListIfLoaded(
        account_reading_list_manager_.get(), url);
    if (reading_list_node) {
      nodes.push_back(reading_list_node);
    }
  }

  // Get all the nodes for |url| from BookmarkModel and sort them by date added.
  bookmarks::ManagedBookmarkService* managed =
      ManagedBookmarkServiceFactory::GetForProfile(profile_);
  std::vector<raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>
      bookmark_model_result = bookmark_model_->GetNodesByURL(url);
  nodes.insert(nodes.end(), bookmark_model_result.begin(),
               bookmark_model_result.end());
  std::sort(nodes.begin(), nodes.end(), &bookmarks::MoreRecentlyAdded);

  // Return the first node matching the search criteria.
  for (const auto* node : nodes) {
    // Skip any managed nodes because they're not user bookmarks.
    if (managed->IsNodeManaged(node)) {
      continue;
    }
    return node;
  }

  return nullptr;
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
  // bookmarks bar, mobile node, other node, and managed node (if it exists,
  // doesn't apply to account bookmarks). Account bookmarks come first, and
  // local bookmarks after.
  const BookmarkNode* account_mobile_node =
      bookmark_model_->account_mobile_node();
  if (IsPermanentFolderVisible(ignore_visibility, account_mobile_node)) {
    top_level_folders.push_back(account_mobile_node);
  }

  const BookmarkNode* account_bookmark_bar_node =
      bookmark_model_->account_bookmark_bar_node();
  if (IsPermanentFolderVisible(ignore_visibility, account_bookmark_bar_node)) {
    top_level_folders.push_back(account_bookmark_bar_node);
  }

  const BookmarkNode* account_other_node =
      bookmark_model_->account_other_node();
  if (IsPermanentFolderVisible(ignore_visibility, account_other_node)) {
    top_level_folders.push_back(account_other_node);
  }

  const BookmarkNode* account_reading_list_node =
      account_reading_list_manager_ ? account_reading_list_manager_->GetRoot()
                                    : nullptr;
  if (IsPermanentFolderVisible(ignore_visibility, account_reading_list_node)) {
    top_level_folders.push_back(account_reading_list_node);
  }

  const BookmarkNode* mobile_node = bookmark_model_->mobile_node();
  // Partner bookmarks are child of the local mobile_node.
  if (IsPermanentFolderVisible(ignore_visibility, mobile_node) ||
      partner_bookmarks_shim_->HasPartnerBookmarks()) {
    top_level_folders.push_back(mobile_node);
  }

  const BookmarkNode* bookmark_bar_node = bookmark_model_->bookmark_bar_node();
  if (IsPermanentFolderVisible(ignore_visibility, bookmark_bar_node)) {
    top_level_folders.push_back(bookmark_bar_node);
  }

  const BookmarkNode* other_node = bookmark_model_->other_node();
  if (IsPermanentFolderVisible(ignore_visibility, other_node)) {
    top_level_folders.push_back(other_node);
  }

  const BookmarkNode* reading_list_node =
      local_or_syncable_reading_list_manager_->GetRoot();
  if (IsPermanentFolderVisible(ignore_visibility, reading_list_node)) {
    top_level_folders.push_back(reading_list_node);
  }

  // Managed node doesn't use the same IsPermanentFolderVisible logic because it
  // doesn't have a corresponding account folder and shouldn't be shown unless
  // determined to be visible through the node (aka by BookmarkClient).
  const BookmarkNode* managed_node =
      managed_bookmark_service_ ? managed_bookmark_service_->managed_node()
                                : nullptr;
  if (managed_node && managed_node->IsVisible()) {
    top_level_folders.push_back(managed_node);
  }

  return top_level_folders;
}

bool BookmarkBridge::IsPermanentFolderVisible(bool ignore_visibility,
                                              const BookmarkNode* folder) {
  // Null folders are never shown.
  if (!folder) {
    return false;
  }

  bool is_account_bookmark = IsAccountBookmarkImpl(folder);
  if (ignore_visibility) {
    // When butter is active ignore_visibility only applies to a subset of local
    // folder to avoid overwhelming the user with unnecessary folders
    // (crbug.com/325070543).
    if (!is_account_bookmark &&
        AreAccountBookmarkFoldersActive(/*env=*/nullptr)) {
      return folder->IsVisible();
    } else {
      return true;
    }
  }

  // Account folders only need to rely on the visibility.
  if (is_account_bookmark) {
    return folder->IsVisible();
  }

  const BookmarkNode* account_folder = GetCorrespondingAccountFolder(folder);
  if (account_folder == nullptr) {
    // If there's no corresponding account folder, then rely on the status quo
    // visibility.
    return folder->IsVisible();
  } else {
    // If there is a corresponding account folder, then the local folder should
    // only be shown when not empty.
    return folder->children().size() > 0;
  }
}

const BookmarkNode* BookmarkBridge::GetCorrespondingAccountFolder(
    const BookmarkNode* folder) {
  CHECK(!IsAccountBookmarkImpl(folder));

  if (folder == bookmark_model_->mobile_node()) {
    return bookmark_model_->account_mobile_node();
  } else if (folder == bookmark_model_->other_node()) {
    return bookmark_model_->account_other_node();
  } else if (folder == bookmark_model_->bookmark_bar_node()) {
    return bookmark_model_->account_bookmark_bar_node();
  } else if (folder == local_or_syncable_reading_list_manager_->GetRoot()) {
    return account_reading_list_manager_
               ? account_reading_list_manager_->GetRoot()
               : nullptr;
  }

  NOTREACHED();
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
  if (!mobile_node) {
    return nullptr;
  }

  ScopedJavaLocalRef<jobject> folder_id_obj = JavaBookmarkIdCreateBookmarkId(
      env, mobile_node->id(), GetBookmarkType(mobile_node));
  return folder_id_obj;
}

ScopedJavaLocalRef<jobject> BookmarkBridge::GetAccountOtherFolderId(
    JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const BookmarkNode* other_node = bookmark_model_->account_other_node();
  if (!other_node) {
    return nullptr;
  }

  ScopedJavaLocalRef<jobject> folder_id_obj = JavaBookmarkIdCreateBookmarkId(
      env, other_node->id(), GetBookmarkType(other_node));
  return folder_id_obj;
}

ScopedJavaLocalRef<jobject> BookmarkBridge::GetAccountDesktopFolderId(
    JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const BookmarkNode* desktop_node =
      bookmark_model_->account_bookmark_bar_node();
  if (!desktop_node) {
    return nullptr;
  }

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

base::android::ScopedJavaLocalRef<jobject>
BookmarkBridge::GetDefaultReadingListFolder(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // If the account reading list is available, then it should be used as the
  // default folder. Otherwise these would be saved into an empty local
  // reading list.
  if (account_reading_list_manager_ &&
      account_reading_list_manager_->GetRoot()) {
    return GetAccountReadingListFolder(env);
  }

  return GetLocalOrSyncableReadingListFolder(env);
}

base::android::ScopedJavaLocalRef<jobject>
BookmarkBridge::GetDefaultBookmarkFolder(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // If the account reading list is available, then it should be used as the
  // default folder. Otherwise these would be saved into an empty local
  // mobile folder.
  if (bookmark_model_->account_mobile_node()) {
    return GetAccountMobileFolderId(env);
  }

  return GetMobileFolderId(env);
}

std::string BookmarkBridge::GetBookmarkGuidByIdForTesting(JNIEnv* env,
                                                          jlong id,
                                                          jint type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const BookmarkNode* node = GetNodeByID(id, type);
  DCHECK(node) << "Bookmark with id " << id << " doesn't exist.";
  return node->uuid().AsLowercaseString();
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
                                      const std::u16string& title) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsLoaded());

  const BookmarkNode* bookmark = GetNodeByID(id, type);

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
                                    const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsLoaded());
  bookmark_model_->SetURL(GetNodeByID(id, type), url,
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
  std::vector<uint8_t> data(size);
  meta->SerializeToArray(data.data(), size);

  return base::android::ToJavaByteArray(env, data);
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
                                     const std::u16string& j_query,
                                     const JavaParamRef<jobjectArray>& j_tags,
                                     jint type,
                                     jint max_results) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(bookmark_model_->loaded());

  power_bookmarks::PowerBookmarkQueryFields query;
  query.word_phrase_query = std::make_unique<std::u16string>(j_query);
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
  std::vector<const BookmarkNode*> results =
      power_bookmarks::GetBookmarksMatchingProperties(bookmark_model_, query,
                                                      max_results);

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
  power_bookmarks::PowerBookmarkQueryFields query;
  query.type = static_cast<power_bookmarks::PowerBookmarkType>(type);
  std::vector<const BookmarkNode*> results =
      power_bookmarks::GetBookmarksMatchingProperties(bookmark_model_, query,
                                                      -1);

  FilterUnreachableBookmarks(&results);
  AddBookmarkNodesToBookmarkIdList(env, j_list, results);
}

ScopedJavaLocalRef<jobject> BookmarkBridge::AddFolder(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_parent_id_obj,
    jint index,
    const std::u16string& title) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsLoaded());

  long bookmark_id = JavaBookmarkIdGetId(env, j_parent_id_obj);
  int type = JavaBookmarkIdGetType(env, j_parent_id_obj);
  const BookmarkNode* parent = GetNodeByID(bookmark_id, type);

  const BookmarkNode* new_node =
      bookmark_model_->AddFolder(parent, static_cast<size_t>(index), title);
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
  DeleteBookmarkImpl(node, type);
}

void BookmarkBridge::DeleteBookmarkImpl(const BookmarkNode* node, int type) {
  // TODO(crbug.com/40063642): Switch to an early returns after debugging why
  // this is called with a nullptr.
  if (!node) {
    LOG(ERROR) << "Deleting null bookmark, type:" << type;
    DUMP_WILL_BE_NOTREACHED();
    return;
  }

  // TODO(crbug.com/40063642): Switch back to a D/CHECK after debugging
  // why this is called with an uneditable node.
  // See https://crbug.com/981172.
  if (!IsEditable(node)) {
    LOG(ERROR) << "Deleting non editable bookmark, type:" << type;
    NOTREACHED_IN_MIGRATION();
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
    BookmarkNodeRemoved(reading_list_parent, index, node, removed_urls,
                        FROM_HERE);

    // Inside the Delete method, node will be destroyed and node->url will be
    // also destroyed. This causes heap-use-after-free at
    // ReadingListModelImpl::RemoveEntryByURLImpl. To avoid the
    // heap-use-after-free, make a copy of node->url() and use it.
    GURL url(node->url());
    reading_list_manager->Delete(url);
  } else {
    bookmark_model_->Remove(node, bookmarks::metrics::BookmarkEditSource::kUser,
                            FROM_HERE);
  }
}

void BookmarkBridge::RemoveAllUserBookmarks(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsLoaded());
  bookmark_model_->RemoveAllUserBookmarks(FROM_HERE);
  local_or_syncable_reading_list_manager_->DeleteAll();
  if (account_reading_list_manager_) {
    account_reading_list_manager_->DeleteAll();
  }
}

void BookmarkBridge::MoveBookmark(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_bookmark_id_obj,
    const JavaParamRef<jobject>& j_parent_id_obj,
    jint j_index) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsLoaded());

  long bookmark_id = JavaBookmarkIdGetId(env, j_bookmark_id_obj);
  int type = JavaBookmarkIdGetType(env, j_bookmark_id_obj);
  const BookmarkNode* node = GetNodeByID(bookmark_id, type);
  DCHECK(IsEditable(node));

  long parent_bookmark_id = JavaBookmarkIdGetId(env, j_parent_id_obj);
  int parent_type = JavaBookmarkIdGetType(env, j_parent_id_obj);
  const BookmarkNode* parent_node =
      GetNodeByID(parent_bookmark_id, parent_type);
  MoveBookmarkImpl(node, type, parent_node, parent_type,
                   static_cast<size_t>(j_index));
}

void BookmarkBridge::MoveBookmarkImpl(const BookmarkNode* node,
                                      int type,
                                      const BookmarkNode* new_parent_node,
                                      int parent_type,
                                      int index) {
  // Bookmark should not be moved to its own parent folder.
  if (node->parent() == new_parent_node) {
    return;
  }

  // If the types of the parents don't match or we're dealing with a reading
  // list node, then we can't just defer to bookmark_model.
  if (type != parent_type ||
      parent_type == bookmarks::BOOKMARK_TYPE_READING_LIST) {
    MoveNodeBetweenReadingListAndBookmarks(node, type, new_parent_node,
                                           parent_type, index);
  } else {
    bookmark_model_->Move(node, new_parent_node, static_cast<size_t>(index));
  }
}

void BookmarkBridge::MoveNodeBetweenReadingListAndBookmarks(
    const BookmarkNode* node,
    int type,
    const BookmarkNode* new_parent_node,
    int parent_type,
    int index) {
  DCHECK(type != parent_type ||
         parent_type == bookmarks::BOOKMARK_TYPE_READING_LIST);

  const BookmarkNode* old_parent_node = node->parent();
  size_t old_index = old_parent_node->GetIndexOf(node).value();
  const BookmarkNode* new_node;

  // Suppress observer notifications while doing this work to make java only
  // see the move operation.
  {
    base::AutoReset<bool> auto_reset_suppress_observer_notifications(
        &suppress_observer_notifications_, true);

    // Add a new node to the correct model, depending on the parent_type.
    if (parent_type == bookmarks::BOOKMARK_TYPE_NORMAL) {
      new_node = bookmark_model_->AddNewURL(new_parent_node, index,
                                            node->GetTitle(), node->url());
    } else if (parent_type == bookmarks::BOOKMARK_TYPE_READING_LIST) {
      ReadingListManager* manager =
          GetReadingListManagerFromParentNode(new_parent_node);
      new_node = manager->Add(node->url(), base::UTF16ToUTF8(node->GetTitle()));
    } else {
      new_node = nullptr;
      NOTREACHED_IN_MIGRATION()
          << "Type swapping is only supported for reading list.";
    }

    // The add operations aren't guaranteed to succeed, so bail early if
    // new_node is null.
    if (!new_node) {
      return;
    }

    // Once the new node has been added successfully, remove the old node and
    // notify java observers of the event.
    DeleteBookmarkImpl(node, type);
  }

  BookmarkNodeMoved(old_parent_node, old_index, new_parent_node,
                    new_parent_node->GetIndexOf(new_node).value());
}

ScopedJavaLocalRef<jobject> BookmarkBridge::AddBookmark(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_parent_id_obj,
    jint index,
    const std::u16string& title,
    const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsLoaded());
  long bookmark_id = JavaBookmarkIdGetId(env, j_parent_id_obj);
  int type = JavaBookmarkIdGetType(env, j_parent_id_obj);
  const BookmarkNode* parent = GetNodeByID(bookmark_id, type);

  const BookmarkNode* new_node = bookmark_model_->AddNewURL(
      parent, static_cast<size_t>(index), title, url);
  DCHECK(new_node);
  ScopedJavaLocalRef<jobject> new_java_obj = JavaBookmarkIdCreateBookmarkId(
      env, new_node->id(), GetBookmarkType(new_node));
  return new_java_obj;
}

ScopedJavaLocalRef<jobject> BookmarkBridge::AddToReadingList(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_parent_id_obj,
    const std::string& title,
    const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsLoaded());

  const BookmarkNode* parent_node =
      GetNodeByID(JavaBookmarkIdGetId(env, j_parent_id_obj),
                  JavaBookmarkIdGetType(env, j_parent_id_obj));
  ReadingListManager* manager =
      GetReadingListManagerFromParentNode(parent_node);

  const BookmarkNode* node = manager->Add(url, title);
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
  SetReadStatusImpl(node->url(), j_read);
}

void BookmarkBridge::SetReadStatusImpl(const GURL& url, bool read) {
  // When marking an item as un/read, the same operation is done in both models
  // (if the url exists) as a convenience. See crbug.com/329280811 for details.
  if (local_or_syncable_reading_list_manager_->Get(url)) {
    local_or_syncable_reading_list_manager_->SetReadStatus(url, read);
  }
  if (account_reading_list_manager_ &&
      account_reading_list_manager_->Get(url)) {
    account_reading_list_manager_->SetReadStatus(url, read);
  }
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

bool BookmarkBridge::IsBookmarked(JNIEnv* env, const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return !GetMostRecentlyAddedUserBookmarkIdForUrl(env, url).is_null();
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

  // TODO(crbug.com/40924440): Folders need to use most recent child's time for
  // date_last_used.
  return Java_BookmarkBridge_createBookmarkItem(
      env, node->id(), type, GetTitle(node), url, node->is_folder(), parent_id,
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
  // TODO(crbug.com/40157934) return the wrong type when the backend is not
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
          identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
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
  if (!IsLoaded() || !java_bookmark_model_ ||
      suppress_observer_notifications_) {
    return;
  }

  Java_BookmarkBridge_bookmarkModelChanged(
      AttachCurrentThread(), ScopedJavaLocalRef<jobject>(java_bookmark_model_));
}

void BookmarkBridge::BookmarkModelLoaded(bool ids_reassigned) {
  NotifyIfDoneLoading();
}

void BookmarkBridge::BookmarkModelBeingDeleted() {
  if (!IsLoaded())
    return;

  DestroyJavaObject();
}

void BookmarkBridge::BookmarkNodeMoved(const BookmarkNode* old_parent,
                                       size_t old_index,
                                       const BookmarkNode* new_parent,
                                       size_t new_index) {
  if (!IsLoaded() || !java_bookmark_model_ ||
      suppress_observer_notifications_) {
    return;
  }

  Java_BookmarkBridge_bookmarkNodeMoved(
      AttachCurrentThread(), ScopedJavaLocalRef<jobject>(java_bookmark_model_),
      CreateJavaBookmark(old_parent), static_cast<int>(old_index),
      CreateJavaBookmark(new_parent), static_cast<int>(new_index));
}

void BookmarkBridge::BookmarkNodeAdded(const BookmarkNode* parent,
                                       size_t index,
                                       bool added_by_user) {
  if (!IsLoaded() || !java_bookmark_model_ ||
      suppress_observer_notifications_) {
    return;
  }

  Java_BookmarkBridge_bookmarkNodeAdded(
      AttachCurrentThread(), ScopedJavaLocalRef<jobject>(java_bookmark_model_),
      CreateJavaBookmark(parent), static_cast<int>(index));
}

void BookmarkBridge::BookmarkNodeRemoved(const BookmarkNode* parent,
                                         size_t old_index,
                                         const BookmarkNode* node,
                                         const std::set<GURL>& removed_urls,
                                         const base::Location& location) {
  if (!IsLoaded() || !java_bookmark_model_ ||
      suppress_observer_notifications_) {
    return;
  }

  Java_BookmarkBridge_bookmarkNodeRemoved(
      AttachCurrentThread(), ScopedJavaLocalRef<jobject>(java_bookmark_model_),
      CreateJavaBookmark(parent), static_cast<int>(old_index),
      CreateJavaBookmark(node));
}

void BookmarkBridge::BookmarkAllUserNodesRemoved(
    const std::set<GURL>& removed_urls,
    const base::Location& location) {
  if (!IsLoaded() || !java_bookmark_model_ ||
      suppress_observer_notifications_) {
    return;
  }

  Java_BookmarkBridge_bookmarkAllUserNodesRemoved(
      AttachCurrentThread(), ScopedJavaLocalRef<jobject>(java_bookmark_model_));
}

void BookmarkBridge::BookmarkNodeChanged(const BookmarkNode* node) {
  if (!IsLoaded() || !java_bookmark_model_ ||
      suppress_observer_notifications_) {
    return;
  }

  Java_BookmarkBridge_bookmarkNodeChanged(
      AttachCurrentThread(), ScopedJavaLocalRef<jobject>(java_bookmark_model_),
      CreateJavaBookmark(node));
}

void BookmarkBridge::BookmarkNodeChildrenReordered(const BookmarkNode* node) {
  if (!IsLoaded() || !java_bookmark_model_ ||
      suppress_observer_notifications_) {
    return;
  }

  Java_BookmarkBridge_bookmarkNodeChildrenReordered(
      AttachCurrentThread(), ScopedJavaLocalRef<jobject>(java_bookmark_model_),
      CreateJavaBookmark(node));
}

void BookmarkBridge::ExtensiveBookmarkChangesBeginning() {
  if (!IsLoaded() || !java_bookmark_model_ ||
      suppress_observer_notifications_) {
    return;
  }

  Java_BookmarkBridge_extensiveBookmarkChangesBeginning(
      AttachCurrentThread(), ScopedJavaLocalRef<jobject>(java_bookmark_model_));
}

void BookmarkBridge::ExtensiveBookmarkChangesEnded() {
  if (!IsLoaded() || !java_bookmark_model_ ||
      suppress_observer_notifications_) {
    return;
  }

  Java_BookmarkBridge_extensiveBookmarkChangesEnded(
      AttachCurrentThread(), ScopedJavaLocalRef<jobject>(java_bookmark_model_));
}

void BookmarkBridge::PartnerShimChanged(PartnerBookmarksShim* shim) {
  if (suppress_observer_notifications_) {
    return;
  }

  BookmarkModelChanged();
}

void BookmarkBridge::PartnerShimLoaded(PartnerBookmarksShim* shim) {
  if (suppress_observer_notifications_) {
    return;
  }

  NotifyIfDoneLoading();
}

void BookmarkBridge::ShimBeingDeleted(PartnerBookmarksShim* shim) {
  partner_bookmarks_shim_ = nullptr;
}

void BookmarkBridge::ReadingListLoaded() {
  NotifyIfDoneLoading();
}

void BookmarkBridge::ReadingListChanged() {
  if (suppress_observer_notifications_) {
    return;
  }

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

ReadingListManager*
BookmarkBridge::GetLocalOrSyncableReadingListManagerForTesting() {
  return local_or_syncable_reading_list_manager_.get();
}

ReadingListManager*
BookmarkBridge::GetAccountReadingListManagerIfAvailableForTesting() {
  return account_reading_list_manager_.get();
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

  NOTREACHED();
}

void BookmarkBridge::ReadingListModelLoaded(const ReadingListModel* model) {
  CreateOrDestroyAccountReadingListManagerIfNeeded();
}

void BookmarkBridge::ReadingListModelCompletedBatchUpdates(
    const ReadingListModel* model) {
  CreateOrDestroyAccountReadingListManagerIfNeeded();
}

void BookmarkBridge::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  if (!base::FeatureList::IsEnabled(
          syncer::kSyncEnableBookmarksInTransportMode)) {
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_BookmarkBridge_clearLastUsedParent(env);
}

void BookmarkBridge::CreateOrDestroyAccountReadingListManagerIfNeeded() {
  auto* account_reading_list_model =
      dual_reading_list_model_->GetAccountModelIfSyncing();
  if (account_reading_list_model_ == account_reading_list_model) {
    return;
  }

  account_reading_list_model_ = account_reading_list_model;

  if (account_reading_list_manager_) {
    reading_list_manager_observations_.RemoveObservation(
        account_reading_list_manager_.get());
    account_reading_list_manager_.reset();
  }

  if (account_reading_list_model_) {
    account_reading_list_manager_ = std::make_unique<ReadingListManagerImpl>(
        account_reading_list_model_, id_gen_func_);
    reading_list_manager_observations_.AddObservation(
        account_reading_list_manager_.get());
  }
}
