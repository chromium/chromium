// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BOOKMARKS_CORE_BOOKMARKS_FUNCTION_H_
#define CHROME_BROWSER_EXTENSIONS_API_BOOKMARKS_CORE_BOOKMARKS_FUNCTION_H_

#include <string>

#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "extensions/browser/extension_function.h"

class Profile;

namespace bookmarks {
class BookmarkNode;
class BookmarkModel;
class ManagedBookmarkService;
}  // namespace bookmarks

namespace extensions {

class BookmarksFunction : public ExtensionFunction,
                          public bookmarks::BaseBookmarkModelObserver {
 public:
  // ExtensionFunction:
  ResponseAction Run() override;

 protected:
  ~BookmarksFunction() override = default;

  // Run semantic equivalent called when the bookmarks are ready.
  // Overrides can return nullptr to further delay responding (a.k.a.
  // RespondLater()).
  virtual ResponseValue RunOnReady() = 0;

  // Helper to get the BookmarkModel.
  bookmarks::BookmarkModel* GetBookmarkModel();

  // Helper to get the ManagedBookmarkService.
  bookmarks::ManagedBookmarkService* GetManagedBookmarkService();

  // Helper to get the bookmark node from a given string id.
  // If the given id can't be parsed or doesn't refer to a valid node, sets
  // |error| and returns nullptr.
  const bookmarks::BookmarkNode* GetBookmarkNodeFromId(
      const std::string& id_string,
      std::string* error);

  // Helper that checks if bookmark editing is enabled.
  bool EditBookmarksEnabled();

  // Helper that checks if |node| can be modified. Returns false if |node|
  // is nullptr, or a managed node, or the root node. In these cases the node
  // can't be edited, can't have new child nodes appended, and its direct
  // children can't be moved or reordered.
  bool CanBeModified(const bookmarks::BookmarkNode* node, std::string* error);

  Profile* GetProfile();

 private:
  // bookmarks::BaseBookmarkModelObserver:
  void BookmarkModelChanged() override;
  void BookmarkModelLoaded(bool ids_reassigned) override;

  // ExtensionFunction:
  void OnResponded() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BOOKMARKS_CORE_BOOKMARKS_FUNCTION_H_
