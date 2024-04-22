// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_BOOKMARKS_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_BOOKMARKS_HELPER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "chrome/browser/sync/test/integration/await_match_status_change_checker.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/multi_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_test_util.h"
#include "components/sync/engine/loopback_server/loopback_server_entity.h"
#include "components/sync/engine/nigori/cryptographer.h"
#include "components/sync/test/fake_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

class BookmarkUndoService;
class GURL;

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

namespace gfx {
class Image;
}  // namespace gfx

namespace bookmarks_helper {

MATCHER_P(HasUuid, expected_uuid, "") {
  const bookmarks::BookmarkNode* actual_node = arg;
  return actual_node->uuid() == expected_uuid;
}

// Helping matchers to check the hierarchy of bookmarks. All matchers work with
// either raw or smart pointer to BookmarkNode and verify its |title|. Usage
// example:
// EXPECT_THAT(
//     bookmark_bar->children(), ElementsAre(
//         IsUrlBookmarkWithTitleAndUrl("Title", GURL("http://url.com")),
//         IsFolderWithTitle("Folder title")));
//
// This example checks that the bookmark bar node has two children nodes: a URL
// and a folder. For complex hierarchy checks IsFolderWithTitleAndChildrenAre
// might be used to verify all children of the folder:
// EXPECT_THAT(
//     bookmark_bar->children(), ElementsAre(
//         IsUrlBookmarkWithTitleAndUrl("Title", GURL("http://url.com")),
//         IsFolderWithTitleAndChildrenAre("Folder title",
//             IsUrlBookmarkWithTitleAndUrl("Title 2", GURL("http://url2.com")),
//             IsUrlBookmarkWithTitleAndUrl("Title 3", GURL("http://url3.com"))
// )));
//
// IsFolderWithTitleAndChildren provides more general approach to verify
// folder's children using container matchers (e.g. UnorderedElementsAre,
// IsEmpty, SizeIs, etc):
// EXPECT_THAT(
//     bookmark_bar->children(), ElementsAre(
//         IsUrlBookmarkWithTitleAndUrl("Title", GURL("http://url.com")),
//         IsFolderWithTitleAndChildren("Folder title",
//             AllOf(SizeIs(2), Contains(
//                 IsUrlBookmarkWithTitleAndUrl("Title 3", "http://url.com")
// )))));

MATCHER_P(IsFolderWithTitle, title, "") {
  *result_listener << "the actual title is " << arg->GetTitle();
  return arg->is_folder() && base::UTF16ToUTF8(arg->GetTitle()) == title;
}

MATCHER_P2(IsUrlBookmarkWithTitleAndUrl, title, url, "") {
  *result_listener << "the actual title is " << arg->GetTitle()
                   << " and URL is " << arg->url();
  return arg->is_url() && base::UTF16ToUTF8(arg->GetTitle()) == title &&
         arg->url() == GURL(url);
}

testing::Matcher<std::unique_ptr<bookmarks::BookmarkNode>>
IsFolderWithTitleAndChildren(
    const std::string& title,
    testing::Matcher<bookmarks::BookmarkNode::TreeNodes> children_matcher);

template <class... Args>
testing::Matcher<std::unique_ptr<bookmarks::BookmarkNode>>
IsFolderWithTitleAndChildrenAre(const std::string& title,
                                Args... children_matchers) {
  return IsFolderWithTitleAndChildren(
      title, testing::ElementsAre(std::move(children_matchers)...));
}

// Used to access the bookmark undo service within a particular sync profile.
[[nodiscard]] BookmarkUndoService* GetBookmarkUndoService(int index);

// Used to access the bookmark model within a particular sync profile.
[[nodiscard]] bookmarks::BookmarkModel* GetBookmarkModel(int index);

// Used to access the bookmark bar within a particular sync profile.
[[nodiscard]] const bookmarks::BookmarkNode* GetBookmarkBarNode(int index);

// Used to access the "other bookmarks" node within a particular sync profile.
[[nodiscard]] const bookmarks::BookmarkNode* GetOtherNode(int index);

// Used to access the "Synced Bookmarks" node within a particular sync profile.
[[nodiscard]] const bookmarks::BookmarkNode* GetSyncedBookmarksNode(int index);

// Used to access the "Managed Bookmarks" node for the given profile.
[[nodiscard]] const bookmarks::BookmarkNode* GetManagedNode(int index);

// Adds a URL with address |url| and title |title| to the bookmark bar of
// profile |profile|. Returns a pointer to the node that was added.
const bookmarks::BookmarkNode* AddURL(int profile,
                                      const std::string& title,
                                      const GURL& url);

// Adds a URL with address |url| and title |title| to the bookmark bar of
// profile |profile| at position |index|. Returns a pointer to the node that
// was added.
const bookmarks::BookmarkNode* AddURL(int profile,
                                      size_t index,
                                      const std::string& title,
                                      const GURL& url);

// Adds a URL with address |url| and title |title| under the node |parent| of
// profile |profile| at position |index|. Returns a pointer to the node that
// was added.
const bookmarks::BookmarkNode* AddURL(int profile,
                                      const bookmarks::BookmarkNode* parent,
                                      size_t index,
                                      const std::string& title,
                                      const GURL& url);

// Adds a folder named |title| to the bookmark bar of profile |profile|.
// Returns a pointer to the folder that was added.
const bookmarks::BookmarkNode* AddFolder(int profile, const std::string& title);

// Adds a folder named |title| to the bookmark bar of profile |profile| at
// position |index|. Returns a pointer to the folder that was added.
const bookmarks::BookmarkNode* AddFolder(int profile,
                                         size_t index,
                                         const std::string& title);

// Adds a folder named |title| to the node |parent| in the bookmark model of
// profile |profile| at position |index|. Returns a pointer to the node that
// was added.
const bookmarks::BookmarkNode* AddFolder(int profile,
                                         const bookmarks::BookmarkNode* parent,
                                         size_t index,
                                         const std::string& title);

// Changes the title of the node |node| in the bookmark model of profile
// |profile| to |new_title|.
void SetTitle(int profile,
              const bookmarks::BookmarkNode* node,
              const std::string& new_title);

// The source of the favicon.
enum FaviconSource { FROM_UI, FROM_SYNC };

// Sets the |icon_url| and |image| data for the favicon for |node| in the
// bookmark model for |profile|. Waits until the favicon is loaded, but does so
// comparing the icon URL and hence is unreliable if the same icon URL has been
// used before.
void SetFavicon(int profile,
                const bookmarks::BookmarkNode* node,
                const GURL& icon_url,
                const gfx::Image& image,
                FaviconSource source);

// Expires the favicon for |node| in the bookmark model for |profile|.
void ExpireFavicon(int profile, const bookmarks::BookmarkNode* node);

// Checks whether the favicon at |icon_url| for |profile| is expired;
void CheckFaviconExpired(int profile, const GURL& icon_url);

// Deletes the favicon mappings for |node| in the bookmark model for |profile|.
void DeleteFaviconMappings(int profile,
                           const bookmarks::BookmarkNode* node,
                           FaviconSource favicon_source);

// Checks whether |page_url| for |profile| has no favicon mappings.
void CheckHasNoFavicon(int profile, const GURL& page_url);

// Changes the url of the node |node| in the bookmark model of profile
// |profile| to |new_url|. Returns a pointer to the node with the changed url.
const bookmarks::BookmarkNode* SetURL(int profile,
                                      const bookmarks::BookmarkNode* node,
                                      const GURL& new_url);

// Moves the node |node| in the bookmark model of profile |profile| so it ends
// up under the node |new_parent| at position |index|.
void Move(int profile,
          const bookmarks::BookmarkNode* node,
          const bookmarks::BookmarkNode* new_parent,
          size_t index);

// Removes the node in the bookmark model of profile |profile| under the node
// |parent| at position |index|.
void Remove(int profile, const bookmarks::BookmarkNode* parent, size_t index);

// Sorts the children of the node |parent| in the bookmark model of profile
// |profile|.
void SortChildren(int profile, const bookmarks::BookmarkNode* parent);

// Reverses the order of the children of the node |parent| in the bookmark
// model of profile |profile|.
void ReverseChildOrder(int profile, const bookmarks::BookmarkNode* parent);

// Checks if the bookmark models of |profile_a| and |profile_b| match each
// other. Returns true if they match.
[[nodiscard]] bool ModelsMatch(int profile_a, int profile_b);

// Checks if the bookmark models of all sync profiles match each other. Does
// not compare them with the verifier bookmark model. Returns true if they
// match.
[[nodiscard]] bool AllModelsMatch();

// Checks if the bookmark model of profile |profile| contains any instances of
// two bookmarks with the same URL under the same parent folder. Returns true
// if even one instance is found.
bool ContainsDuplicateBookmarks(int profile);

// Returns whether a node exists with the specified url.
bool HasNodeWithURL(int profile, const GURL& url);

// Gets the node in the bookmark model of profile |profile| that has the url
// |url|. Note: Only one instance of |url| is assumed to be present.
[[nodiscard]] const bookmarks::BookmarkNode* GetUniqueNodeByURL(
    int profile,
    const GURL& url);

// Returns the number of bookmarks in bookmark model of profile |profile|.
[[nodiscard]] size_t CountAllBookmarks(int profile);

// Returns the number of bookmarks in bookmark model of profile |profile|
// whose titles match the string |title|.
[[nodiscard]] size_t CountBookmarksWithTitlesMatching(int profile,
                                                      const std::string& title);

// Returns the number of bookmarks in bookmark model of profile |profile|
// whose URLs match the |url|.
[[nodiscard]] size_t CountBookmarksWithUrlsMatching(int profile,
                                                    const GURL& url);

// Returns the number of bookmark folders in the bookmark model of profile
// |profile| whose titles contain the query string |title|.
[[nodiscard]] size_t CountFoldersWithTitlesMatching(int profile,
                                                    const std::string& title);

// Returns whether there exists a BookmarkNode in the bookmark model of
// profile |profile| whose UUID matches `uuid`.
bool ContainsBookmarkNodeWithUuid(int profile, const base::Uuid& uuid);

// Creates a favicon of |color| with image reps of the platform's supported
// scale factors (eg MacOS) in addition to 1x.
gfx::Image CreateFavicon(SkColor color);

// Creates a 1x only favicon from the PNG file at |path|.
gfx::Image Create1xFaviconFromPNGFile(const std::string& path);

// Returns a URL identifiable by |i|.
std::string IndexedURL(size_t i);

// Returns a URL title identifiable by |i|.
std::string IndexedURLTitle(size_t i);

// Returns a folder name identifiable by |i|.
std::string IndexedFolderName(size_t i);

// Returns a subfolder name identifiable by |i|.
std::string IndexedSubfolderName(size_t i);

// Returns a subsubfolder name identifiable by |i|.
std::string IndexedSubsubfolderName(size_t i);

// Creates a server-side entity representing a bookmark with the given title and
// URL.
std::unique_ptr<syncer::LoopbackServerEntity> CreateBookmarkServerEntity(
    const std::string& title,
    const GURL& url);

// Helper class that reacts to any BookmarkModelObserver event by running a
// callback provided in the constructor.
class AnyBookmarkChangeObserver : public bookmarks::BaseBookmarkModelObserver {
 public:
  explicit AnyBookmarkChangeObserver(const base::RepeatingClosure& cb);
  ~AnyBookmarkChangeObserver() override;

  AnyBookmarkChangeObserver(const AnyBookmarkChangeObserver&) = delete;
  AnyBookmarkChangeObserver& operator=(const AnyBookmarkChangeObserver&) =
      delete;

  // BaseBookmarkModelObserver overrides.
  void BookmarkModelChanged() override;
  void BookmarkNodeFaviconChanged(const bookmarks::BookmarkNode* node) override;

 private:
  const base::RepeatingClosure cb_;
};

// Base class used for checkers that verify the state of an arbitrary number
// of BookmarkModel instances.
class BookmarkModelStatusChangeChecker : public StatusChangeChecker {
 public:
  BookmarkModelStatusChangeChecker();
  ~BookmarkModelStatusChangeChecker() override;

  BookmarkModelStatusChangeChecker(const BookmarkModelStatusChangeChecker&) =
      delete;
  BookmarkModelStatusChangeChecker& operator=(
      const BookmarkModelStatusChangeChecker&) = delete;

 protected:
  void Observe(bookmarks::BookmarkModel* model);

  // StatusChangeChecker override.
  void CheckExitCondition() override;

 private:
  // Equivalent of CheckExitCondition() that instead posts a task in the current
  // task runner.
  void PostCheckExitCondition();

  SEQUENCE_CHECKER(sequence_checker_);

  std::vector<std::pair<bookmarks::BookmarkModel*,
                        std::unique_ptr<AnyBookmarkChangeObserver>>>
      observers_;

  bool pending_check_exit_condition_ = false;
  base::WeakPtrFactory<BookmarkModelStatusChangeChecker> weak_ptr_factory_{
      this};
};

// Checker used to block until bookmarks match on all clients.
class BookmarksMatchChecker : public BookmarkModelStatusChangeChecker {
 public:
  BookmarksMatchChecker();

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;
  bool Wait() override;
};

// Base class used for checkers that verify the state of a single BookmarkModel
// instance.
class SingleBookmarkModelStatusChangeChecker
    : public BookmarkModelStatusChangeChecker {
 public:
  explicit SingleBookmarkModelStatusChangeChecker(int profile_index);
  ~SingleBookmarkModelStatusChangeChecker() override;

  SingleBookmarkModelStatusChangeChecker(
      const SingleBookmarkModelStatusChangeChecker&) = delete;
  SingleBookmarkModelStatusChangeChecker& operator=(
      const SingleBookmarkModelStatusChangeChecker&) = delete;

 protected:
  int profile_index() const;
  bookmarks::BookmarkModel* bookmark_model() const;

 private:
  const int profile_index_;
  const raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
};

// Generic status change checker that waits until a predicate as defined by
// a gMock matches becomes true.
class SingleBookmarksModelMatcherChecker
    : public SingleBookmarkModelStatusChangeChecker {
 public:
  using Matcher = testing::Matcher<std::vector<const bookmarks::BookmarkNode*>>;

  SingleBookmarksModelMatcherChecker(int profile_index, const Matcher& matcher);
  ~SingleBookmarksModelMatcherChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) final;

 private:
  const Matcher matcher_;
};

// Checker used to block until the actual number of bookmarks with the given
// title match the expected count.
class BookmarksTitleChecker : public SingleBookmarkModelStatusChangeChecker {
 public:
  BookmarksTitleChecker(int profile_index,
                        const std::string& title,
                        int expected_count);

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  const int profile_index_;
  const std::string title_;
  const int expected_count_;
};

// Checker used to wait until the favicon of a bookmark has been loaded. It
// doesn't itself trigger the load of the favicon.
class BookmarkFaviconLoadedChecker
    : public SingleBookmarkModelStatusChangeChecker {
 public:
  // There must be exactly one bookmark for |page_url| in the BookmarkModel in
  // |profile_index|.
  BookmarkFaviconLoadedChecker(int profile_index, const GURL& page_url);

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  const raw_ptr<const bookmarks::BookmarkNode> bookmark_node_;
};

// Checker used to block until the bookmarks on the server match a given set of
// expected bookmarks. The |title| is comapred to both legacy and full titles.
class ServerBookmarksEqualityChecker
    : public fake_server::FakeServerMatchStatusChecker {
 public:
  struct ExpectedBookmark {
    // Used to check both legacy and full titles in specifics.
    std::string title;
    GURL url;
  };

  // If a |cryptographer| is provided (i.e. is not nullptr), it is assumed that
  // the server-side data should be encrypted, and the provided cryptographer
  // will be used to decrypt the data prior to checking for equality.
  // |fake_server| must not be nullptr and must outlive this object.
  ServerBookmarksEqualityChecker(
      std::vector<ExpectedBookmark> expected_bookmarks,
      syncer::Cryptographer* cryptographer);

  bool IsExitConditionSatisfied(std::ostream* os) override;

  ServerBookmarksEqualityChecker(const ServerBookmarksEqualityChecker&) =
      delete;
  ServerBookmarksEqualityChecker& operator=(
      const ServerBookmarksEqualityChecker&) = delete;

  ~ServerBookmarksEqualityChecker() override;

 private:
  raw_ptr<syncer::Cryptographer> cryptographer_;
  const std::vector<ExpectedBookmark> expected_bookmarks_;
};

// Checker used to block until the actual number of bookmarks with the given url
// match the expected count.
class BookmarksUrlChecker : public SingleBookmarkModelStatusChangeChecker {
 public:
  BookmarksUrlChecker(int profile, const GURL& url, int expected_count);

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  const GURL url_;
  const int expected_count_;
};

// Checker used to block until there exists a bookmark with the given UUID.
class BookmarksUuidChecker : public SingleBookmarksModelMatcherChecker {
 public:
  BookmarksUuidChecker(int profile, const base::Uuid& uuid);
  ~BookmarksUuidChecker() override;
};

// Waits until the fake server has the similar structure of bookmarks like the
// bookmark model. The checker verifies that all nodes have the same UUID,
// title, URL, parent and order. It doesn't check favicons and any other fields.
// Note that this class is not enough to verify test's result as it only waits
// for the state when the bookmark model has the same structure on the server.
// It doesn't check their content and the expected number of bookmarks. The fake
// server must have entities with unique UUIDs.
class BookmarkModelMatchesFakeServerChecker
    : public SingleClientStatusChangeChecker {
 public:
  BookmarkModelMatchesFakeServerChecker(int profile,
                                        syncer::SyncServiceImpl* service,
                                        fake_server::FakeServer* fake_server);

  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  std::map<std::string, sync_pb::SyncEntity>
  GetServerPermanentBookmarksGroupedBySyncId() const;

  // Fills in |server_bookmarks_by_uuid| with all non-permanent entities stored
  // on the server. All entities must have unique UUID in specifics. Returns
  // false if there are duplicate entities on the server.
  bool GetServerBookmarksByUniqueUuid(std::map<base::Uuid, sync_pb::SyncEntity>*
                                          server_bookmarks_by_uuid) const;

  // Check that a permanent parent node of given |node| is the same as for the
  // matching |server_entity|.
  bool CheckPermanentParentNode(const bookmarks::BookmarkNode* node,
                                const sync_pb::SyncEntity& server_entity,
                                std::ostream* os) const;

  // Check that a regular parent node of given |node| matches to the parent of
  // matching server entity.
  bool CheckParentNode(
      const bookmarks::BookmarkNode* node,
      const std::map<base::Uuid, sync_pb::SyncEntity>& server_bookmarks_by_uuid,
      std::ostream* os) const;

  // Return ordered UUIDs of server entities grouped by their parents.
  std::map<std::string, std::vector<base::Uuid>>
  GetServerUuidsGroupedByParentSyncId(
      const std::map<base::Uuid, sync_pb::SyncEntity>& server_bookmarks_by_uuid)
      const;

  const raw_ptr<fake_server::FakeServer> fake_server_;
  const int profile_index_;
};

}  // namespace bookmarks_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_BOOKMARKS_HELPER_H_
