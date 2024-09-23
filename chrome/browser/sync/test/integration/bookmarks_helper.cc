// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/bookmarks_helper.h"

#include <stddef.h>

#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "base/containers/stack.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "base/types/optional_util.h"
#include "base/uuid.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/undo/bookmark_undo_service_factory.h"
#include "chrome/common/chrome_paths.h"
#include "components/bookmarks/browser/bookmark_client.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_util.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/protocol/bookmark_specifics.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"
#include "components/sync/protocol/unique_position.pb.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/test/entity_builder_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/tree_node_iterator.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace bookmarks_helper {

namespace {

const char kBookmarkBarTag[] = "bookmark_bar";
const char kSyncedBookmarksTag[] = "synced_bookmarks";
const char kOtherBookmarksTag[] = "other_bookmarks";

const BookmarkNode* GetPermanentNodeForServerTag(
    int profile_index,
    const std::string& server_defined_unique_tag) {
  if (server_defined_unique_tag == kBookmarkBarTag) {
    return GetBookmarkBarNode(profile_index);
  }
  if (server_defined_unique_tag == kSyncedBookmarksTag) {
    return GetSyncedBookmarksNode(profile_index);
  }
  if (server_defined_unique_tag == kOtherBookmarksTag) {
    return GetOtherNode(profile_index);
  }

  return nullptr;
}

void ApplyBookmarkFavicon(
    const BookmarkNode* bookmark_node,
    favicon::FaviconService* favicon_service,
    const GURL& icon_url,
    const scoped_refptr<base::RefCountedMemory>& bitmap_data) {
  // Some tests use no services.
  if (favicon_service == nullptr) {
    return;
  }

  favicon_service->AddPageNoVisitForBookmark(bookmark_node->url(),
                                             bookmark_node->GetTitle());

  GURL icon_url_to_use = icon_url;

  if (icon_url.is_empty()) {
    if (bitmap_data->size() == 0) {
      // Empty icon URL and no bitmap data means no icon mapping.
      favicon_service->DeleteFaviconMappings({bookmark_node->url()},
                                             favicon_base::IconType::kFavicon);
      return;
    } else {
      // Ancient clients (prior to M25) may not be syncing the favicon URL. If
      // the icon URL is not synced, use the page URL as a fake icon URL as it
      // is guaranteed to be unique.
      icon_url_to_use = bookmark_node->url();
    }
  }

  // The client may have cached the favicon at 2x. Use MergeFavicon() as not to
  // overwrite the cached 2x favicon bitmap. Sync favicons are always
  // gfx::kFaviconSize in width and height. Store the favicon into history
  // as such.
  gfx::Size pixel_size(gfx::kFaviconSize, gfx::kFaviconSize);
  favicon_service->MergeFavicon(bookmark_node->url(), icon_url_to_use,
                                favicon_base::IconType::kFavicon, bitmap_data,
                                pixel_size);
}

// Helper class used to wait for changes to take effect on the favicon of a
// particular bookmark node in a particular bookmark model.
class FaviconChangeObserver : public bookmarks::BookmarkModelObserver {
 public:
  FaviconChangeObserver(BookmarkModel* model,
                        const BookmarkNode* node,
                        const std::optional<GURL>& expected_icon_url)
      : model_(model), node_(node), expected_icon_url_(expected_icon_url) {
    model->AddObserver(this);
  }

  FaviconChangeObserver(const FaviconChangeObserver&) = delete;
  FaviconChangeObserver& operator=(const FaviconChangeObserver&) = delete;

  ~FaviconChangeObserver() override { model_->RemoveObserver(this); }

  void WaitUntilFaviconChangedToIconURL() {
    DCHECK(!run_loop_.running());
    run_loop_.Run();
  }

  // bookmarks::BookmarkModelObserver:
  void BookmarkModelLoaded(bool ids_reassigned) override {}
  void BookmarkNodeMoved(const BookmarkNode* old_parent,
                         size_t old_index,
                         const BookmarkNode* new_parent,
                         size_t new_index) override {}
  void BookmarkNodeAdded(const BookmarkNode* parent,
                         size_t index,
                         bool added_by_user) override {}
  void BookmarkNodeRemoved(const BookmarkNode* parent,
                           size_t old_index,
                           const BookmarkNode* node,
                           const std::set<GURL>& removed_urls,
                           const base::Location& location) override {}
  void BookmarkAllUserNodesRemoved(const std::set<GURL>& removed_urls,
                                   const base::Location& location) override {}

  void BookmarkNodeChanged(const BookmarkNode* node) override {
    if (node == node_) {
      model_->GetFavicon(node);
    }
  }

  void BookmarkNodeChildrenReordered(const BookmarkNode* node) override {}

  void BookmarkNodeFaviconChanged(const BookmarkNode* node) override {
    if (node != node_) {
      return;
    }
    if (!node_->is_favicon_loaded()) {
      // Favicons are loaded lazily, trigger loading. Note, that this logic is
      // particularly important for favicon deletion, since simple check of
      // icon_url() is not sufficient (it can be null if favicon was actually
      // deleted or if it's not yet loaded).
      model_->GetFavicon(node_);
      return;
    }

    if (base::OptionalFromPtr(node_->icon_url()) == expected_icon_url_) {
      run_loop_.Quit();
    }
  }

 private:
  const raw_ptr<BookmarkModel> model_;
  const raw_ptr<const BookmarkNode> node_;
  const std::optional<GURL> expected_icon_url_;

  base::RunLoop run_loop_;
};

// Returns the number of nodes of node type |node_type| in |model| whose
// titles match the string |title|.
size_t CountNodesWithTitlesMatching(BookmarkModel* model,
                                    BookmarkNode::Type node_type,
                                    const std::u16string& title) {
  ui::TreeNodeIterator<const BookmarkNode> iterator(model->root_node());
  // Walk through the model tree looking for bookmark nodes of node type
  // |node_type| whose titles match |title|.
  size_t count = 0;
  while (iterator.has_next()) {
    const BookmarkNode* node = iterator.Next();
    if ((node->type() == node_type) && (node->GetTitle() == title)) {
      ++count;
    }
  }
  return count;
}

// Returns the number of nodes of node type |node_type| in |model|.
size_t CountNodes(BookmarkModel* model, BookmarkNode::Type node_type) {
  ui::TreeNodeIterator<const BookmarkNode> iterator(model->root_node());
  // Walk through the model tree looking for bookmark nodes of node type
  // |node_type|.
  size_t count = 0;
  while (iterator.has_next()) {
    const BookmarkNode* node = iterator.Next();
    if (node->type() == node_type) {
      ++count;
    }
  }
  return count;
}

// Checks if the favicon data in |bitmap_a| and |bitmap_b| are equivalent.
// Returns true if they match.
bool FaviconRawBitmapsMatch(const SkBitmap& bitmap_a,
                            const SkBitmap& bitmap_b) {
  if (bitmap_a.computeByteSize() == 0U && bitmap_b.computeByteSize() == 0U) {
    return true;
  }
  if ((bitmap_a.computeByteSize() != bitmap_b.computeByteSize()) ||
      (bitmap_a.width() != bitmap_b.width()) ||
      (bitmap_a.height() != bitmap_b.height())) {
    LOG(ERROR) << "Favicon size mismatch: " << bitmap_a.computeByteSize()
               << " (" << bitmap_a.width() << "x" << bitmap_a.height()
               << ") vs. " << bitmap_b.computeByteSize() << " ("
               << bitmap_b.width() << "x" << bitmap_b.height() << ")";
    return false;
  }
  void* node_pixel_addr_a = bitmap_a.getPixels();
  EXPECT_TRUE(node_pixel_addr_a);
  void* node_pixel_addr_b = bitmap_b.getPixels();
  EXPECT_TRUE(node_pixel_addr_b);
  if (memcmp(node_pixel_addr_a, node_pixel_addr_b,
             bitmap_a.computeByteSize()) != 0) {
    LOG(ERROR) << "Favicon bitmap mismatch";
    return false;
  } else {
    return true;
  }
}

// Represents a favicon image and the icon URL associated with it.
struct FaviconData {
  FaviconData(const gfx::Image& favicon_image, const GURL& favicon_url)
      : image(favicon_image), icon_url(favicon_url) {}

  gfx::Image image;
  GURL icon_url;
};

// Gets the favicon and icon URL associated with |node| in |model|. Returns
// nullopt if the favicon is still loading.
std::optional<FaviconData> GetFaviconData(BookmarkModel* model,
                                          const BookmarkNode* node) {
  // We may need to wait for the favicon to be loaded via
  // BookmarkModel::GetFavicon(), which is an asynchronous operation.
  if (!node->is_favicon_loaded()) {
    model->GetFavicon(node);
    // Favicon still loading, no data available just yet.
    return std::nullopt;
  }

  // Favicon loaded: return actual image, if there is one (the no-favicon case
  // is also considered loaded).
  return FaviconData(model->GetFavicon(node),
                     node->icon_url() ? *node->icon_url() : GURL());
}

// Sets the favicon for |profile| and |node|. |profile| may be
// |test()->verifier()|.
void SetFaviconImpl(Profile* profile,
                    const BookmarkNode* node,
                    const GURL& icon_url,
                    const gfx::Image& image,
                    FaviconSource favicon_source) {
  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(profile);

  FaviconChangeObserver observer(model, node, icon_url);
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  if (favicon_source == FROM_UI) {
    favicon_service->SetFavicons({node->url()}, icon_url,
                                 favicon_base::IconType::kFavicon, image);
  } else {
    ApplyBookmarkFavicon(node, favicon_service, icon_url, image.As1xPNGBytes());
  }

  // Wait for the favicon for |node| to be updated.
  observer.WaitUntilFaviconChangedToIconURL();
}

// Expires the favicon for |profile| and |node|. |profile| may be
// |test()->verifier()|.
void ExpireFaviconImpl(Profile* profile, const BookmarkNode* node) {
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  favicon_service->SetFaviconOutOfDateForPage(node->url());
}

// Used to call FaviconService APIs synchronously by making |callback| quit a
// RunLoop.
void OnGotFaviconData(
    base::OnceClosure callback,
    favicon_base::FaviconRawBitmapResult* output_bitmap_result,
    const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  *output_bitmap_result = bitmap_result;
  std::move(callback).Run();
}

// Deletes favicon mappings for |profile| and |node|. |profile| may be
// |test()->verifier()|.
void DeleteFaviconMappingsImpl(Profile* profile,
                               const BookmarkNode* node,
                               FaviconSource favicon_source) {
  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(profile);

  FaviconChangeObserver observer(model, node,
                                 /*expected_icon_url=*/std::nullopt);
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);

  if (favicon_source == FROM_UI) {
    favicon_service->DeleteFaviconMappings({node->url()},
                                           favicon_base::IconType::kFavicon);
  } else {
    ApplyBookmarkFavicon(
        node, favicon_service, /*icon_url=*/GURL(),
        scoped_refptr<base::RefCountedString>(new base::RefCountedString()));
  }

  // Wait for the favicon for |node| to be deleted.
  observer.WaitUntilFaviconChangedToIconURL();
}

// Checks if the favicon in |node_a| from |model_a| matches that of |node_b|
// from |model_b|. Returns true if they match.
bool FaviconsMatch(BookmarkModel* model_a,
                   BookmarkModel* model_b,
                   const BookmarkNode* node_a,
                   const BookmarkNode* node_b) {
  DCHECK(!node_a->is_folder());
  DCHECK(!node_b->is_folder());

  std::optional<FaviconData> favicon_data_a = GetFaviconData(model_a, node_a);
  std::optional<FaviconData> favicon_data_b = GetFaviconData(model_b, node_b);

  // If either of the two favicons is still loading, let's return false now
  // because observers will get notified when the load completes. Note that even
  // the lack of favicon is considered a loaded favicon.
  if (!favicon_data_a.has_value() || !favicon_data_b.has_value()) {
    return false;
  }

  if (favicon_data_a->icon_url != favicon_data_b->icon_url) {
    return false;
  }

  gfx::Image image_a = favicon_data_a->image;
  gfx::Image image_b = favicon_data_b->image;

  if (image_a.IsEmpty() && image_b.IsEmpty()) {
    return true;  // Two empty images are equivalent.
  }

  if (image_a.IsEmpty() != image_b.IsEmpty()) {
    return false;
  }

  // Compare only the 1x bitmaps as only those are synced.
  SkBitmap bitmap_a = image_a.AsImageSkia().GetRepresentation(1.0f).GetBitmap();
  SkBitmap bitmap_b = image_b.AsImageSkia().GetRepresentation(1.0f).GetBitmap();
  return FaviconRawBitmapsMatch(bitmap_a, bitmap_b);
}

// Does a deep comparison of BookmarkNode fields in |model_a| and |model_b|.
// Returns true if they are all equal.
bool NodesMatch(const BookmarkNode* node_a, const BookmarkNode* node_b) {
  if (node_a == nullptr || node_b == nullptr) {
    return node_a == node_b;
  }
  if (node_a->is_folder() != node_b->is_folder()) {
    LOG(ERROR) << "Cannot compare folder with bookmark";
    return false;
  }
  if (node_a->GetTitle() != node_b->GetTitle()) {
    LOG(ERROR) << "Title mismatch: " << node_a->GetTitle() << " vs. "
               << node_b->GetTitle();
    return false;
  }
  if (node_a->url() != node_b->url()) {
    LOG(ERROR) << "URL mismatch: " << node_a->url() << " vs. " << node_b->url();
    return false;
  }
  if (node_a->parent()->GetIndexOf(node_a) !=
      node_b->parent()->GetIndexOf(node_b)) {
    LOG(ERROR) << "Index mismatch: "
               << node_a->parent()->GetIndexOf(node_a).value() << " vs. "
               << node_b->parent()->GetIndexOf(node_b).value();
    return false;
  }
  if (node_a->uuid() != node_b->uuid()) {
    LOG(ERROR) << "UUID mismatch: " << node_a->uuid() << " vs. "
               << node_b->uuid();
    return false;
  }
  if (node_a->parent()->is_root() != node_b->parent()->is_root() ||
      node_a->parent()->is_permanent_node() !=
          node_b->parent()->is_permanent_node()) {
    LOG(ERROR) << "Permanent parent node mismatch: "
               << node_a->parent()->is_permanent_node() << " vs. "
               << node_b->parent()->is_permanent_node();
    return false;
  }
  if (!node_a->parent()->is_root() &&
      node_a->parent()->uuid() != node_b->parent()->uuid()) {
    LOG(ERROR) << "Parent node mismatch: " << node_a->parent()->GetTitle()
               << " vs. " << node_b->parent()->GetTitle();
    return false;
  }
  return true;
}

// Checks if the hierarchies in |model_a| and |model_b| are equivalent in
// terms of the data model and favicon. Returns true if they both match.
// Note: Some peripheral fields like creation times are allowed to mismatch.
bool BookmarkModelsMatch(BookmarkModel* model_a, BookmarkModel* model_b) {
  // base::Unretained() is safe because these iterators are short-lived.
  ui::TreeNodeIterator<const BookmarkNode> iterator_a(
      model_a->root_node(),
      base::BindRepeating(&bookmarks::BookmarkClient::IsNodeManaged,
                          base::Unretained(model_a->client())));
  ui::TreeNodeIterator<const BookmarkNode> iterator_b(
      model_b->root_node(),
      base::BindRepeating(&bookmarks::BookmarkClient::IsNodeManaged,
                          base::Unretained(model_b->client())));
  while (iterator_a.has_next()) {
    const BookmarkNode* node_a = iterator_a.Next();
    if (!iterator_b.has_next()) {
      LOG(ERROR) << "Models do not match.";
      return false;
    }
    const BookmarkNode* node_b = iterator_b.Next();
    if (!NodesMatch(node_a, node_b)) {
      LOG(ERROR) << "Nodes do not match";
      return false;
    }
    if (node_a->is_folder() || node_b->is_folder()) {
      continue;
    }
    if (!FaviconsMatch(model_a, model_b, node_a, node_b)) {
      LOG(ERROR) << "Favicons do not match";
      return false;
    }
  }
  return !iterator_b.has_next();
}

std::vector<const BookmarkNode*> GetAllBookmarkNodes(
    const BookmarkModel* model) {
  std::vector<const BookmarkNode*> all_nodes;

  // Add root node separately as iterator does not include it.
  all_nodes.push_back(model->root_node());

  ui::TreeNodeIterator<const BookmarkNode> iterator(model->root_node());
  while (iterator.has_next()) {
    all_nodes.push_back(iterator.Next());
  }

  return all_nodes;
}

void TriggerAllFaviconLoading(BookmarkModel* model) {
  for (const BookmarkNode* node : GetAllBookmarkNodes(model)) {
    if (!node->is_favicon_loaded()) {
      // GetFavicon() kicks off the loading.
      model->GetFavicon(node);
    }
  }
}

}  // namespace

testing::Matcher<std::unique_ptr<bookmarks::BookmarkNode>>
IsFolderWithTitleAndChildren(
    const std::string& title,
    testing::Matcher<BookmarkNode::TreeNodes> children_matcher) {
  return testing::AllOf(
      IsFolderWithTitle(title),
      testing::Pointee(testing::Property(&bookmarks::BookmarkNode::children,
                                         std::move(children_matcher))));
}

BookmarkUndoService* GetBookmarkUndoService(int index) {
  return BookmarkUndoServiceFactory::GetForProfile(
      sync_datatype_helper::test()->GetProfile(index));
}

BookmarkModel* GetBookmarkModel(int index) {
  return BookmarkModelFactory::GetForBrowserContext(
      sync_datatype_helper::test()->GetProfile(index));
}

const BookmarkNode* GetBookmarkBarNode(int index) {
  return GetBookmarkModel(index)->bookmark_bar_node();
}

const BookmarkNode* GetOtherNode(int index) {
  return GetBookmarkModel(index)->other_node();
}

const BookmarkNode* GetSyncedBookmarksNode(int index) {
  return GetBookmarkModel(index)->mobile_node();
}

const BookmarkNode* GetManagedNode(int index) {
  return ManagedBookmarkServiceFactory::GetForProfile(
             sync_datatype_helper::test()->GetProfile(index))
      ->managed_node();
}

const BookmarkNode* AddURL(int profile,
                           const std::string& title,
                           const GURL& url) {
  return AddURL(profile, GetBookmarkBarNode(profile), 0, title, url);
}

const BookmarkNode* AddURL(int profile,
                           size_t index,
                           const std::string& title,
                           const GURL& url) {
  return AddURL(profile, GetBookmarkBarNode(profile), index, title, url);
}

const BookmarkNode* AddURL(int profile,
                           const BookmarkNode* parent,
                           size_t index,
                           const std::string& title,
                           const GURL& url) {
  BookmarkModel* model = GetBookmarkModel(profile);
  if (bookmarks::GetBookmarkNodeByID(model, parent->id()) != parent) {
    LOG(ERROR) << "Node " << parent->GetTitle() << " does not belong to "
               << "Profile " << profile;
    return nullptr;
  }
  const BookmarkNode* result =
      model->AddURL(parent, index, base::UTF8ToUTF16(title), url);
  if (!result) {
    LOG(ERROR) << "Could not add bookmark " << title << " to Profile "
               << profile;
    return nullptr;
  }
  return result;
}

const BookmarkNode* AddFolder(int profile, const std::string& title) {
  return AddFolder(profile, GetBookmarkBarNode(profile), 0, title);
}

const BookmarkNode* AddFolder(int profile,
                              size_t index,
                              const std::string& title) {
  return AddFolder(profile, GetBookmarkBarNode(profile), index, title);
}

const BookmarkNode* AddFolder(int profile,
                              const BookmarkNode* parent,
                              size_t index,
                              const std::string& title) {
  BookmarkModel* model = GetBookmarkModel(profile);
  if (bookmarks::GetBookmarkNodeByID(model, parent->id()) != parent) {
    LOG(ERROR) << "Node " << parent->GetTitle() << " does not belong to "
               << "Profile " << profile;
    return nullptr;
  }
  const BookmarkNode* result =
      model->AddFolder(parent, index, base::UTF8ToUTF16(title));
  EXPECT_TRUE(result);
  if (!result) {
    LOG(ERROR) << "Could not add folder " << title << " to Profile " << profile;
    return nullptr;
  }
  return result;
}

void SetTitle(int profile,
              const BookmarkNode* node,
              const std::string& new_title) {
  BookmarkModel* model = GetBookmarkModel(profile);
  ASSERT_EQ(bookmarks::GetBookmarkNodeByID(model, node->id()), node)
      << "Node " << node->GetTitle() << " does not belong to "
      << "Profile " << profile;
  model->SetTitle(node, base::UTF8ToUTF16(new_title),
                  bookmarks::metrics::BookmarkEditSource::kOther);
}

void SetFavicon(int profile,
                const BookmarkNode* node,
                const GURL& icon_url,
                const gfx::Image& image,
                FaviconSource favicon_source) {
  BookmarkModel* model = GetBookmarkModel(profile);
  ASSERT_EQ(bookmarks::GetBookmarkNodeByID(model, node->id()), node)
      << "Node " << node->GetTitle() << " does not belong to "
      << "Profile " << profile;
  ASSERT_EQ(BookmarkNode::URL, node->type())
      << "Node " << node->GetTitle() << " must be a url.";
  SetFaviconImpl(sync_datatype_helper::test()->GetProfile(profile), node,
                 icon_url, image, favicon_source);
}

void ExpireFavicon(int profile, const BookmarkNode* node) {
  BookmarkModel* model = GetBookmarkModel(profile);
  ASSERT_EQ(bookmarks::GetBookmarkNodeByID(model, node->id()), node)
      << "Node " << node->GetTitle() << " does not belong to "
      << "Profile " << profile;
  ASSERT_EQ(BookmarkNode::URL, node->type())
      << "Node " << node->GetTitle() << " must be a url.";

  ExpireFaviconImpl(sync_datatype_helper::test()->GetProfile(profile), node);
}

void CheckFaviconExpired(int profile, const GURL& icon_url) {
  base::RunLoop run_loop;

  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(
          sync_datatype_helper::test()->GetProfile(profile),
          ServiceAccessType::EXPLICIT_ACCESS);
  base::CancelableTaskTracker task_tracker;
  favicon_base::FaviconRawBitmapResult bitmap_result;
  favicon_service->GetRawFavicon(
      icon_url, favicon_base::IconType::kFavicon, 0,
      base::BindOnce(&OnGotFaviconData, run_loop.QuitClosure(), &bitmap_result),
      &task_tracker);
  run_loop.Run();

  ASSERT_TRUE(bitmap_result.is_valid());
  ASSERT_TRUE(bitmap_result.expired);
}

void CheckHasNoFavicon(int profile, const GURL& page_url) {
  base::RunLoop run_loop;

  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(
          sync_datatype_helper::test()->GetProfile(profile),
          ServiceAccessType::EXPLICIT_ACCESS);
  base::CancelableTaskTracker task_tracker;
  favicon_base::FaviconRawBitmapResult bitmap_result;
  favicon_service->GetRawFaviconForPageURL(
      page_url, {favicon_base::IconType::kFavicon}, 0,
      /*fallback_to_host=*/false,
      base::BindOnce(&OnGotFaviconData, run_loop.QuitClosure(), &bitmap_result),
      &task_tracker);
  run_loop.Run();

  ASSERT_FALSE(bitmap_result.is_valid());
}

void DeleteFaviconMappings(int profile,
                           const BookmarkNode* node,
                           FaviconSource favicon_source) {
  BookmarkModel* model = GetBookmarkModel(profile);
  ASSERT_EQ(bookmarks::GetBookmarkNodeByID(model, node->id()), node)
      << "Node " << node->GetTitle() << " does not belong to "
      << "Profile " << profile;
  ASSERT_EQ(BookmarkNode::URL, node->type())
      << "Node " << node->GetTitle() << " must be a url.";

  DeleteFaviconMappingsImpl(sync_datatype_helper::test()->GetProfile(profile),
                            node, favicon_source);
}

const BookmarkNode* SetURL(int profile,
                           const BookmarkNode* node,
                           const GURL& new_url) {
  BookmarkModel* model = GetBookmarkModel(profile);
  if (bookmarks::GetBookmarkNodeByID(model, node->id()) != node) {
    LOG(ERROR) << "Node " << node->GetTitle() << " does not belong to "
               << "Profile " << profile;
    return nullptr;
  }
  if (node->is_url()) {
    model->SetURL(node, new_url,
                  bookmarks::metrics::BookmarkEditSource::kOther);
  }
  return node;
}

void Move(int profile,
          const BookmarkNode* node,
          const BookmarkNode* new_parent,
          size_t index) {
  BookmarkModel* model = GetBookmarkModel(profile);
  ASSERT_EQ(bookmarks::GetBookmarkNodeByID(model, node->id()), node)
      << "Node " << node->GetTitle() << " does not belong to "
      << "Profile " << profile;
  model->Move(node, new_parent, index);
}

void Remove(int profile, const BookmarkNode* parent, size_t index) {
  BookmarkModel* model = GetBookmarkModel(profile);
  ASSERT_EQ(bookmarks::GetBookmarkNodeByID(model, parent->id()), parent)
      << "Node " << parent->GetTitle() << " does not belong to "
      << "Profile " << profile;
  model->Remove(parent->children()[index].get(),
                bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);
}

void SortChildren(int profile, const BookmarkNode* parent) {
  BookmarkModel* model = GetBookmarkModel(profile);
  ASSERT_EQ(bookmarks::GetBookmarkNodeByID(model, parent->id()), parent)
      << "Node " << parent->GetTitle() << " does not belong to "
      << "Profile " << profile;
  model->SortChildren(parent);
}

void ReverseChildOrder(int profile, const BookmarkNode* parent) {
  ASSERT_EQ(
      bookmarks::GetBookmarkNodeByID(GetBookmarkModel(profile), parent->id()),
      parent)
      << "Node " << parent->GetTitle() << " does not belong to "
      << "Profile " << profile;
  if (parent->children().empty()) {
    return;
  }
  for (size_t i = 0; i < parent->children().size(); ++i) {
    Move(profile, parent->children().back().get(), parent, i);
  }
}

bool ModelsMatch(int profile_a, int profile_b) {
  return BookmarkModelsMatch(GetBookmarkModel(profile_a),
                             GetBookmarkModel(profile_b));
}

bool AllModelsMatch() {
  for (int i = 1; i < sync_datatype_helper::test()->num_clients(); ++i) {
    if (!ModelsMatch(0, i)) {
      LOG(ERROR) << "Model " << i << " does not match Model 0.";
      return false;
    }
  }
  return true;
}

bool ContainsDuplicateBookmarks(int profile) {
  ui::TreeNodeIterator<const BookmarkNode> iterator(
      GetBookmarkModel(profile)->root_node());
  while (iterator.has_next()) {
    const BookmarkNode* node = iterator.Next();
    if (node->is_folder()) {
      continue;
    }
    std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes =
        GetBookmarkModel(profile)->GetNodesByURL(node->url());
    EXPECT_GE(nodes.size(), 1U);
    for (std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>::
             const_iterator it = nodes.begin();
         it != nodes.end(); ++it) {
      if (node->id() != (*it)->id() && node->parent() == (*it)->parent() &&
          node->GetTitle() == (*it)->GetTitle()) {
        return true;
      }
    }
  }
  return false;
}

bool HasNodeWithURL(int profile, const GURL& url) {
  return !GetBookmarkModel(profile)->GetNodesByURL(url).empty();
}

const BookmarkNode* GetUniqueNodeByURL(int profile, const GURL& url) {
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes =
      GetBookmarkModel(profile)->GetNodesByURL(url);
  EXPECT_EQ(1U, nodes.size());
  if (nodes.empty()) {
    return nullptr;
  }
  return nodes[0];
}

size_t CountAllBookmarks(int profile) {
  return CountNodes(GetBookmarkModel(profile), BookmarkNode::URL);
}

size_t CountBookmarksWithTitlesMatching(int profile, const std::string& title) {
  return CountNodesWithTitlesMatching(
      GetBookmarkModel(profile), BookmarkNode::URL, base::UTF8ToUTF16(title));
}

size_t CountBookmarksWithUrlsMatching(int profile, const GURL& url) {
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes =
      GetBookmarkModel(profile)->GetNodesByURL(url);
  return nodes.size();
}

size_t CountFoldersWithTitlesMatching(int profile, const std::string& title) {
  return CountNodesWithTitlesMatching(GetBookmarkModel(profile),
                                      BookmarkNode::FOLDER,
                                      base::UTF8ToUTF16(title));
}

bool ContainsBookmarkNodeWithUuid(int profile, const base::Uuid& uuid) {
  for (const BookmarkNode* node :
       GetAllBookmarkNodes(GetBookmarkModel(profile))) {
    if (node->uuid() == uuid) {
      return true;
    }
  }
  return false;
}

gfx::Image CreateFavicon(SkColor color) {
  const int dip_width = 16;
  const int dip_height = 16;
  std::vector<float> favicon_scales = favicon_base::GetFaviconScales();
  gfx::ImageSkia favicon;
  for (float scale : favicon_scales) {
    int pixel_width = dip_width * scale;
    int pixel_height = dip_height * scale;
    SkBitmap bmp;
    bmp.allocN32Pixels(pixel_width, pixel_height);
    bmp.eraseColor(color);
    favicon.AddRepresentation(gfx::ImageSkiaRep(bmp, scale));
  }
  return gfx::Image(favicon);
}

gfx::Image Create1xFaviconFromPNGFile(const std::string& path) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  const char* kPNGExtension = ".png";
  if (!base::EndsWith(path, kPNGExtension,
                      base::CompareCase::INSENSITIVE_ASCII)) {
    return gfx::Image();
  }

  base::FilePath full_path;
  if (!base::PathService::Get(chrome::DIR_TEST_DATA, &full_path)) {
    return gfx::Image();
  }

  full_path = full_path.AppendASCII("sync").AppendASCII(path);
  std::string contents;
  base::ReadFileToString(full_path, &contents);
  return gfx::Image::CreateFrom1xPNGBytes(
      base::MakeRefCounted<base::RefCountedString>(std::move(contents)));
}

std::string IndexedURL(size_t i) {
  return "http://www.host.ext:1234/path/filename/" + base::NumberToString(i);
}

std::string IndexedURLTitle(size_t i) {
  return "URL Title " + base::NumberToString(i);
}

std::string IndexedFolderName(size_t i) {
  return "Folder Name " + base::NumberToString(i);
}

std::string IndexedSubfolderName(size_t i) {
  return "Subfolder Name " + base::NumberToString(i);
}

std::string IndexedSubsubfolderName(size_t i) {
  return "Subsubfolder Name " + base::NumberToString(i);
}

std::unique_ptr<syncer::LoopbackServerEntity> CreateBookmarkServerEntity(
    const std::string& title,
    const GURL& url) {
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(title);
  return bookmark_builder.BuildBookmark(url);
}

AnyBookmarkChangeObserver::AnyBookmarkChangeObserver(
    const base::RepeatingClosure& cb)
    : cb_(cb) {}

AnyBookmarkChangeObserver::~AnyBookmarkChangeObserver() = default;

void AnyBookmarkChangeObserver::BookmarkModelChanged() {
  cb_.Run();
}

void AnyBookmarkChangeObserver::BookmarkNodeFaviconChanged(
    const bookmarks::BookmarkNode* node) {
  cb_.Run();
}

BookmarkModelStatusChangeChecker::BookmarkModelStatusChangeChecker() = default;

BookmarkModelStatusChangeChecker::~BookmarkModelStatusChangeChecker() {
  for (const auto& [model, observer] : observers_) {
    model->RemoveObserver(observer.get());
  }
}

void BookmarkModelStatusChangeChecker::Observe(
    bookmarks::BookmarkModel* model) {
  auto observer =
      std::make_unique<AnyBookmarkChangeObserver>(base::BindRepeating(
          &SingleBookmarkModelStatusChangeChecker::PostCheckExitCondition,
          weak_ptr_factory_.GetWeakPtr()));
  model->AddObserver(observer.get());
  observers_.emplace_back(model, std::move(observer));
}

void BookmarkModelStatusChangeChecker::CheckExitCondition() {
  pending_check_exit_condition_ = false;
  StatusChangeChecker::CheckExitCondition();
}

void BookmarkModelStatusChangeChecker::PostCheckExitCondition() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (pending_check_exit_condition_) {
    // Already posted.
    return;
  }

  pending_check_exit_condition_ = true;

  // PostTask() instead of CheckExitCondition() directly to make sure that the
  // checker doesn't immediately kick in while bookmarks are modified.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&BookmarkModelStatusChangeChecker::CheckExitCondition,
                     weak_ptr_factory_.GetWeakPtr()));
}

BookmarksMatchChecker::BookmarksMatchChecker() {
  for (int i = 0; i < sync_datatype_helper::test()->num_clients(); ++i) {
    Observe(GetBookmarkModel(i));
  }
}

bool BookmarksMatchChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for matching models";
  return AllModelsMatch();
}

bool BookmarksMatchChecker::Wait() {
  for (int i = 0; i < sync_datatype_helper::test()->num_clients(); ++i) {
    TriggerAllFaviconLoading(GetBookmarkModel(i));
  }
  return BookmarkModelStatusChangeChecker::Wait();
}

SingleBookmarkModelStatusChangeChecker::SingleBookmarkModelStatusChangeChecker(
    int profile_index)
    : profile_index_(profile_index),
      bookmark_model_(GetBookmarkModel(profile_index)) {
  Observe(bookmark_model_);
}

SingleBookmarkModelStatusChangeChecker::
    ~SingleBookmarkModelStatusChangeChecker() = default;

int SingleBookmarkModelStatusChangeChecker::profile_index() const {
  return profile_index_;
}

BookmarkModel* SingleBookmarkModelStatusChangeChecker::bookmark_model() const {
  return bookmark_model_;
}

SingleBookmarksModelMatcherChecker::SingleBookmarksModelMatcherChecker(
    int profile_index,
    const Matcher& matcher)
    : SingleBookmarkModelStatusChangeChecker(profile_index),
      matcher_(matcher) {}

SingleBookmarksModelMatcherChecker::~SingleBookmarksModelMatcherChecker() =
    default;

bool SingleBookmarksModelMatcherChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  const std::vector<const BookmarkNode*> all_bookmark_nodes =
      GetAllBookmarkNodes(bookmark_model());

  testing::StringMatchResultListener result_listener;
  const bool matches = testing::ExplainMatchResult(matcher_, all_bookmark_nodes,
                                                   &result_listener);
  if (TimedOut() && !matches && result_listener.str().empty()) {
    // Some matchers don't provide details via ExplainMatchResult().
    *os << "Expected: ";
    matcher_.DescribeTo(os);
    *os << "  Actual: " << testing::PrintToString(all_bookmark_nodes);
  } else {
    *os << result_listener.str();
  }
  return matches;
}

BookmarksTitleChecker::BookmarksTitleChecker(int profile_index,
                                             const std::string& title,
                                             int expected_count)
    : SingleBookmarkModelStatusChangeChecker(profile_index),
      profile_index_(profile_index),
      title_(title),
      expected_count_(expected_count) {
  DCHECK_GE(expected_count, 0) << "expected_count must be non-negative.";
}

bool BookmarksTitleChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for bookmark count to match";
  int actual_count = CountBookmarksWithTitlesMatching(profile_index_, title_);
  return expected_count_ == actual_count;
}

BookmarkFaviconLoadedChecker::BookmarkFaviconLoadedChecker(int profile_index,
                                                           const GURL& page_url)
    : SingleBookmarkModelStatusChangeChecker(profile_index),
      bookmark_node_(GetUniqueNodeByURL(profile_index, page_url)) {
  DCHECK_NE(nullptr, bookmark_node_);
}

bool BookmarkFaviconLoadedChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for the favicon to be loaded for " << bookmark_node_->url();
  return bookmark_node_->is_favicon_loaded();
}

ServerBookmarksEqualityChecker::ServerBookmarksEqualityChecker(
    std::vector<ExpectedBookmark> expected_bookmarks,
    syncer::Cryptographer* cryptographer)
    : cryptographer_(cryptographer),
      expected_bookmarks_(std::move(expected_bookmarks)) {}

bool ServerBookmarksEqualityChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  *os << "Waiting for server-side bookmarks to match expected.";

  std::vector<sync_pb::SyncEntity> entities =
      fake_server()->GetSyncEntitiesByDataType(syncer::BOOKMARKS);
  if (expected_bookmarks_.size() != entities.size()) {
    return false;
  }

  // Make a copy so we can remove bookmarks that were found.
  std::vector<ExpectedBookmark> expected = expected_bookmarks_;
  for (const sync_pb::SyncEntity& entity : entities) {
    sync_pb::BookmarkSpecifics actual_specifics;
    if (entity.specifics().has_encrypted()) {
      // If no cryptographer was provided, we expect the specifics to have
      // unencrypted data.
      if (!cryptographer_) {
        return false;
      }
      sync_pb::EntitySpecifics entity_specifics;
      EXPECT_TRUE(cryptographer_->Decrypt(entity.specifics().encrypted(),
                                          &entity_specifics));
      actual_specifics = entity_specifics.bookmark();
    } else {
      // If the cryptographer was provided, we expect the specifics to have
      // encrypted data.
      if (cryptographer_) {
        return false;
      }
      actual_specifics = entity.specifics().bookmark();
    }

    auto it = base::ranges::find_if(
        expected, [actual_specifics](const ExpectedBookmark& bookmark) {
          return actual_specifics.legacy_canonicalized_title() ==
                     bookmark.title &&
                 actual_specifics.full_title() == bookmark.title &&
                 actual_specifics.url() == bookmark.url;
        });
    if (it != expected.end()) {
      expected.erase(it);
    } else {
      *os << "Could not find expected bookmark with title '"
          << actual_specifics.legacy_canonicalized_title() << "' and URL '"
          << actual_specifics.url() << "'";
      return false;
    }
  }

  return true;
}

ServerBookmarksEqualityChecker::~ServerBookmarksEqualityChecker() = default;

BookmarksUrlChecker::BookmarksUrlChecker(int profile,
                                         const GURL& url,
                                         int expected_count)
    : SingleBookmarkModelStatusChangeChecker(profile),
      url_(url),
      expected_count_(expected_count) {}

bool BookmarksUrlChecker::IsExitConditionSatisfied(std::ostream* os) {
  int actual_count = CountBookmarksWithUrlsMatching(profile_index(), url_);
  *os << "Expected " << expected_count_ << " bookmarks with URL " << url_
      << " but found " << actual_count;
  if (TimedOut()) {
    *os << " in "
        << testing::PrintToString(
               GetAllBookmarkNodes(GetBookmarkModel(profile_index())));
  }
  return expected_count_ == actual_count;
}

BookmarksUuidChecker::BookmarksUuidChecker(int profile, const base::Uuid& uuid)
    : SingleBookmarksModelMatcherChecker(profile,
                                         testing::Contains(HasUuid(uuid))) {}

BookmarksUuidChecker::~BookmarksUuidChecker() = default;

BookmarkModelMatchesFakeServerChecker::BookmarkModelMatchesFakeServerChecker(
    int profile,
    syncer::SyncServiceImpl* service,
    fake_server::FakeServer* fake_server)
    : SingleClientStatusChangeChecker(service),
      fake_server_(fake_server),
      profile_index_(profile) {}

bool BookmarkModelMatchesFakeServerChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  *os << "Waiting for server-side bookmarks to match bookmark model.";

  std::map<base::Uuid, sync_pb::SyncEntity> server_bookmarks_by_uuid;
  if (!GetServerBookmarksByUniqueUuid(&server_bookmarks_by_uuid)) {
    *os << "The server has duplicate entities having the same UUID.";
    return false;
  }

  const std::map<std::string, std::vector<base::Uuid>>
      server_uuids_by_parent_id =
          GetServerUuidsGroupedByParentSyncId(server_bookmarks_by_uuid);

  // |bookmarks_count| is used to check that the bookmark model doesn't have
  // less nodes than the fake server.
  size_t bookmarks_count = 0;
  const bookmarks::BookmarkNode* root_node =
      GetBookmarkModel(profile_index_)->root_node();
  ui::TreeNodeIterator<const bookmarks::BookmarkNode> iterator(root_node);
  while (iterator.has_next()) {
    const BookmarkNode* node = iterator.Next();

    // Do not check permanent nodes.
    if (node->is_permanent_node()) {
      continue;
    }

    auto iter = server_bookmarks_by_uuid.find(node->uuid());
    if (iter == server_bookmarks_by_uuid.end()) {
      *os << "Missing a node from on the server for UUID: " << node->uuid();
      return false;
    }
    const sync_pb::SyncEntity& server_entity = iter->second;

    bookmarks_count++;

    // Check that the server node has the same parent as the local |node|.
    if (!CheckParentNode(node, server_bookmarks_by_uuid, os)) {
      return false;
    }

    // Check that the local |node| and the server entity have the same position.
    auto parent_iter =
        server_uuids_by_parent_id.find(server_entity.parent_id_string());
    CHECK(parent_iter != server_uuids_by_parent_id.end(),
          base::NotFatalUntil::M130);
    auto server_position_iter =
        base::ranges::find(parent_iter->second, node->uuid());
    CHECK(server_position_iter != parent_iter->second.end(),
          base::NotFatalUntil::M130);
    const size_t server_position =
        server_position_iter - parent_iter->second.begin();
    const size_t local_position = node->parent()->GetIndexOf(node).value();
    if (server_position != local_position) {
      *os << "Different positions on the server and in the local model for "
             "node: "
          << node->GetTitle() << ", server position: " << server_position
          << ", local position: " << local_position;
      return false;
    }

    // Check titles and URLs.
    if (base::UTF16ToUTF8(node->GetTitle()) !=
        server_entity.specifics().bookmark().full_title()) {
      *os << " Title mismatch for node: " << node->GetTitle();
      return false;
    }

    if (node->is_folder() != server_entity.folder() ||
        node->is_folder() != (server_entity.specifics().bookmark().type() ==
                              sync_pb::BookmarkSpecifics::FOLDER)) {
      *os << " Node type mismatch for node: " << node->GetTitle();
      return false;
    }

    if (!node->is_folder() &&
        node->url() != server_entity.specifics().bookmark().url()) {
      *os << " Node URL mismatch for node: " << node->GetTitle();
      return false;
    }
  }

  if (server_bookmarks_by_uuid.size() != bookmarks_count) {
    // An iteration over the local bookmark model has been finished at the
    // moment. So the server can have only more entities than the local model if
    // their sizes differ.
    *os << " The fake server has more nodes than the bookmark model";
    return false;
  }

  return true;
}

bool BookmarkModelMatchesFakeServerChecker::CheckPermanentParentNode(
    const bookmarks::BookmarkNode* node,
    const sync_pb::SyncEntity& server_entity,
    std::ostream* os) const {
  // Parent node must be a permanent node.
  const BookmarkNode* parent_node = node->parent();
  DCHECK(parent_node->is_permanent_node());

  // Matching server entity must exist.
  DCHECK_EQ(node->uuid().AsLowercaseString(),
            server_entity.specifics().bookmark().guid());

  const std::map<std::string, sync_pb::SyncEntity>
      permanent_nodes_by_server_id =
          GetServerPermanentBookmarksGroupedBySyncId();

  auto permanent_parent_iter =
      permanent_nodes_by_server_id.find(server_entity.parent_id_string());
  if (permanent_parent_iter == permanent_nodes_by_server_id.end()) {
    *os << " A permanent parent node is missing on the server for node: "
        << node->GetTitle();
    return false;
  }

  if (parent_node !=
      GetPermanentNodeForServerTag(
          profile_index_,
          permanent_parent_iter->second.server_defined_unique_tag())) {
    *os << " Permanent parent node mismatch for node: " << node->GetTitle();
    return false;
  }
  return true;
}

bool BookmarkModelMatchesFakeServerChecker::CheckParentNode(
    const bookmarks::BookmarkNode* node,
    const std::map<base::Uuid, sync_pb::SyncEntity>& server_bookmarks_by_uuid,
    std::ostream* os) const {
  // Only one matching server entity must exist.
  DCHECK_EQ(1u, server_bookmarks_by_uuid.count(node->uuid()));
  auto iter = server_bookmarks_by_uuid.find(node->uuid());
  const sync_pb::SyncEntity& server_entity = iter->second;

  const BookmarkNode* parent_node = node->parent();
  if (server_entity.specifics().bookmark().parent_guid() !=
      parent_node->uuid().AsLowercaseString()) {
    *os << " Parent node's UUID in specifics does not match";
    return false;
  }

  if (parent_node->is_permanent_node()) {
    return CheckPermanentParentNode(node, server_entity, os);
  }

  auto parent_iter = server_bookmarks_by_uuid.find(parent_node->uuid());
  if (parent_iter == server_bookmarks_by_uuid.end()) {
    *os << " Missing a parent node on the server for node: "
        << node->GetTitle();
    return false;
  }

  if (parent_iter->second.id_string() != iter->second.parent_id_string()) {
    *os << " Parent mismatch found for node: " << node->GetTitle();
    return false;
  }
  return true;
}

std::map<std::string, sync_pb::SyncEntity>
BookmarkModelMatchesFakeServerChecker::
    GetServerPermanentBookmarksGroupedBySyncId() const {
  const std::vector<sync_pb::SyncEntity> server_permanent_bookmarks =
      fake_server_->GetPermanentSyncEntitiesByDataType(syncer::BOOKMARKS);
  std::map<std::string, sync_pb::SyncEntity> permanent_nodes_by_server_id;
  for (const sync_pb::SyncEntity& entity : server_permanent_bookmarks) {
    DCHECK(!entity.server_defined_unique_tag().empty());
    permanent_nodes_by_server_id[entity.id_string()] = entity;
  }
  return permanent_nodes_by_server_id;
}

bool BookmarkModelMatchesFakeServerChecker::GetServerBookmarksByUniqueUuid(
    std::map<base::Uuid, sync_pb::SyncEntity>* server_bookmarks_by_uuid) const {
  const std::vector<sync_pb::SyncEntity> server_bookmarks =
      fake_server_->GetSyncEntitiesByDataType(syncer::BOOKMARKS);
  for (const sync_pb::SyncEntity& entity : server_bookmarks) {
    // Skip permanent nodes.
    if (!entity.server_defined_unique_tag().empty()) {
      continue;
    }
    if (!server_bookmarks_by_uuid
             ->emplace(base::Uuid::ParseLowercase(
                           entity.specifics().bookmark().guid()),
                       entity)
             .second) {
      return false;
    }
  }
  return true;
}

std::map<std::string, std::vector<base::Uuid>>
BookmarkModelMatchesFakeServerChecker::GetServerUuidsGroupedByParentSyncId(
    const std::map<base::Uuid, sync_pb::SyncEntity>& server_bookmarks_by_uuid)
    const {
  std::map<std::string, std::vector<base::Uuid>> uuids_grouped_by_parent_id;
  for (const auto& [uuid, entity] : server_bookmarks_by_uuid) {
    uuids_grouped_by_parent_id[entity.parent_id_string()].push_back(uuid);
  }
  auto sort_by_position_fn = [&server_bookmarks_by_uuid](
                                 const base::Uuid& left,
                                 const base::Uuid& right) {
    const sync_pb::UniquePosition& left_position =
        server_bookmarks_by_uuid.at(left).unique_position();
    const sync_pb::UniquePosition& right_position =
        server_bookmarks_by_uuid.at(right).unique_position();
    return syncer::UniquePosition::FromProto(left_position)
        .LessThan(syncer::UniquePosition::FromProto(right_position));
  };

  for (auto& [parent_id, children_uuids] : uuids_grouped_by_parent_id) {
    base::ranges::sort(children_uuids, sort_by_position_fn);
  }
  return uuids_grouped_by_parent_id;
}

}  // namespace bookmarks_helper
