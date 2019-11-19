// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_BOOKMARKS_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_BOOKMARKS_HELPER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "chrome/browser/sync/test/integration/await_match_status_change_checker.h"
#include "chrome/browser/sync/test/integration/multi_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "components/sync/engine_impl/loopback_server/loopback_server_entity.h"
#include "components/sync/nigori/cryptographer.h"
#include "components/sync/test/fake_server/fake_server.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

class GURL;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace gfx {
class Image;
}  // namespace gfx

namespace bookmarks_helper {

// Used to access the bookmark model within a particular sync profile.
bookmarks::BookmarkModel* GetBookmarkModel(int index) WARN_UNUSED_RESULT;

// Used to access the bookmark bar within a particular sync profile.
const bookmarks::BookmarkNode* GetBookmarkBarNode(int index) WARN_UNUSED_RESULT;

// Used to access the "other bookmarks" node within a particular sync profile.
const bookmarks::BookmarkNode* GetOtherNode(int index) WARN_UNUSED_RESULT;

// Used to access the "Synced Bookmarks" node within a particular sync profile.
const bookmarks::BookmarkNode* GetSyncedBookmarksNode(int index)
    WARN_UNUSED_RESULT;

// Used to access the "Managed Bookmarks" node for the given profile.
const bookmarks::BookmarkNode* GetManagedNode(int index) WARN_UNUSED_RESULT;

// Used to access the bookmarks within the verifier sync profile.
bookmarks::BookmarkModel* GetVerifierBookmarkModel() WARN_UNUSED_RESULT;

// Adds a URL with address |url| and title |title| to the bookmark bar of
// profile |profile|. Returns a pointer to the node that was added.
const bookmarks::BookmarkNode* AddURL(int profile,
                                      const std::string& title,
                                      const GURL& url) WARN_UNUSED_RESULT;

// Adds a URL with address |url| and title |title| to the bookmark bar of
// profile |profile| at position |index|. Returns a pointer to the node that
// was added.
const bookmarks::BookmarkNode* AddURL(int profile,
                                      size_t index,
                                      const std::string& title,
                                      const GURL& url) WARN_UNUSED_RESULT;

// Adds a URL with address |url| and title |title| under the node |parent| of
// profile |profile| at position |index|. Returns a pointer to the node that
// was added.
const bookmarks::BookmarkNode* AddURL(int profile,
                                      const bookmarks::BookmarkNode* parent,
                                      size_t index,
                                      const std::string& title,
                                      const GURL& url) WARN_UNUSED_RESULT;

// Adds a folder named |title| to the bookmark bar of profile |profile|.
// Returns a pointer to the folder that was added.
const bookmarks::BookmarkNode* AddFolder(int profile, const std::string& title)
    WARN_UNUSED_RESULT;

// Adds a folder named |title| to the bookmark bar of profile |profile| at
// position |index|. Returns a pointer to the folder that was added.
const bookmarks::BookmarkNode* AddFolder(int profile,
                                         size_t index,
                                         const std::string& title)
    WARN_UNUSED_RESULT;

// Adds a folder named |title| to the node |parent| in the bookmark model of
// profile |profile| at position |index|. Returns a pointer to the node that
// was added.
const bookmarks::BookmarkNode* AddFolder(int profile,
                                         const bookmarks::BookmarkNode* parent,
                                         size_t index,
                                         const std::string& title)
    WARN_UNUSED_RESULT;

// Changes the title of the node |node| in the bookmark model of profile
// |profile| to |new_title|.
void SetTitle(int profile,
              const bookmarks::BookmarkNode* node,
              const std::string& new_title);

// The source of the favicon.
enum FaviconSource {
  FROM_UI,
  FROM_SYNC
};

// Sets the |icon_url| and |image| data for the favicon for |node| in the
// bookmark model for |profile|.
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
                                      const GURL& new_url) WARN_UNUSED_RESULT;

// Moves the node |node| in the bookmark model of profile |profile| so it ends
// up under the node |new_parent| at position |index|.
void Move(int profile,
          const bookmarks::BookmarkNode* node,
          const bookmarks::BookmarkNode* new_parent,
          size_t index);

// Removes the node in the bookmark model of profile |profile| under the node
// |parent| at position |index|.
void Remove(int profile, const bookmarks::BookmarkNode* parent, size_t index);

// Removes all non-permanent nodes in the bookmark model of profile |profile|.
void RemoveAll(int profile);

// Sorts the children of the node |parent| in the bookmark model of profile
// |profile|.
void SortChildren(int profile, const bookmarks::BookmarkNode* parent);

// Reverses the order of the children of the node |parent| in the bookmark
// model of profile |profile|.
void ReverseChildOrder(int profile, const bookmarks::BookmarkNode* parent);

// Checks if the bookmark model of profile |profile| matches the verifier
// bookmark model. Returns true if they match.
bool ModelMatchesVerifier(int profile) WARN_UNUSED_RESULT;

// Checks if the bookmark models of all sync profiles match the verifier
// bookmark model. Returns true if they match.
bool AllModelsMatchVerifier() WARN_UNUSED_RESULT;

// Checks if the bookmark models of |profile_a| and |profile_b| match each
// other. Returns true if they match.
bool ModelsMatch(int profile_a, int profile_b) WARN_UNUSED_RESULT;

// Checks if the bookmark models of all sync profiles match each other. Does
// not compare them with the verifier bookmark model. Returns true if they
// match.
bool AllModelsMatch() WARN_UNUSED_RESULT;

// Checks if the bookmark model of profile |profile| contains any instances of
// two bookmarks with the same URL under the same parent folder. Returns true
// if even one instance is found.
bool ContainsDuplicateBookmarks(int profile);

// Returns whether a node exists with the specified url.
bool HasNodeWithURL(int profile, const GURL& url);

// Gets the node in the bookmark model of profile |profile| that has the url
// |url|. Note: Only one instance of |url| is assumed to be present.
const bookmarks::BookmarkNode* GetUniqueNodeByURL(int profile, const GURL& url)
    WARN_UNUSED_RESULT;

// Returns the number of bookmarks in bookmark model of profile |profile|.
size_t CountAllBookmarks(int profile) WARN_UNUSED_RESULT;

// Returns the number of bookmarks in bookmark model of profile |profile|
// whose titles match the string |title|.
size_t CountBookmarksWithTitlesMatching(int profile, const std::string& title)
    WARN_UNUSED_RESULT;

// Returns the number of bookmarks in bookmark model of profile |profile|
// whose URLs match the |url|.
size_t CountBookmarksWithUrlsMatching(int profile,
                                      const GURL& url) WARN_UNUSED_RESULT;

// Returns the number of bookmark folders in the bookmark model of profile
// |profile| whose titles contain the query string |title|.
size_t CountFoldersWithTitlesMatching(int profile, const std::string& title)
    WARN_UNUSED_RESULT;

// Returns whether there exists a BookmarkNode in the bookmark model of
// profile |profile| whose GUID matches the string |guid|.
bool ContainsBookmarkNodeWithGUID(int profile, const std::string& guid);

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

// Checker used to block until bookmarks match on all clients.
class BookmarksMatchChecker : public MultiClientStatusChangeChecker {
 public:
  BookmarksMatchChecker();

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;
};

// Checker used to block until bookmarks match the verifier bookmark model.
class BookmarksMatchVerifierChecker : public MultiClientStatusChangeChecker {
 public:
  BookmarksMatchVerifierChecker();

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;
};

// Checker used to block until the actual number of bookmarks with the given
// title match the expected count.
// TODO(pvalenzuela): Remove this class and instead use
// AwaitMatchStatusChangeChecker.
class BookmarksTitleChecker : public SingleClientStatusChangeChecker {
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

// Checker used to block until the bookmarks on the server match a given set of
// expected bookmarks.
class ServerBookmarksEqualityChecker : public SingleClientStatusChangeChecker {
 public:
  struct ExpectedBookmark {
    std::string title;
    GURL url;
  };

  // If a |cryptographer| is provided (i.e. is not nullptr), it is assumed that
  // the server-side data should be encrypted, and the provided cryptographer
  // will be used to decrypt the data prior to checking for equality.
  ServerBookmarksEqualityChecker(
      syncer::ProfileSyncService* service,
      fake_server::FakeServer* fake_server,
      const std::vector<ExpectedBookmark>& expected_bookmarks,
      syncer::Cryptographer* cryptographer);

  bool IsExitConditionSatisfied(std::ostream* os) override;

  ~ServerBookmarksEqualityChecker() override;

 private:
  fake_server::FakeServer* fake_server_;
  syncer::Cryptographer* cryptographer_;
  const std::vector<ExpectedBookmark> expected_bookmarks_;

  DISALLOW_COPY_AND_ASSIGN(ServerBookmarksEqualityChecker);
};

// Checker used to block until the actual number of bookmarks with the given url
// match the expected count.
class BookmarksUrlChecker : public AwaitMatchStatusChangeChecker {
 public:
  BookmarksUrlChecker(int profile, const GURL& url, int expected_count);
};

// Checker used to block until there exists a bookmark with the given GUID.
class BookmarksGUIDChecker : public AwaitMatchStatusChangeChecker {
 public:
  BookmarksGUIDChecker(int profile_index, const std::string& guid);
};

}  // namespace bookmarks_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_BOOKMARKS_HELPER_H_
