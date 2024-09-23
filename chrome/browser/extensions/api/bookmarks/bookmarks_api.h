// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BOOKMARKS_BOOKMARKS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_BOOKMARKS_BOOKMARKS_API_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/bookmarks_core/bookmarks_function.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"
#include "ui/shell_dialogs/select_file_dialog.h"

class Profile;

namespace base {
class FilePath;
}

namespace bookmarks {
class BookmarkNode;
class BookmarkModel;
class ManagedBookmarkService;
}

namespace content {
class BrowserContext;
}

namespace extensions {

namespace api::bookmarks {
struct CreateDetails;
}  // namespace api::bookmarks

// Observes BookmarkModel and then routes the notifications as events to
// the extension system.
class BookmarkEventRouter : public bookmarks::BookmarkModelObserver {
 public:
  explicit BookmarkEventRouter(Profile* profile);
  BookmarkEventRouter(const BookmarkEventRouter&) = delete;
  BookmarkEventRouter& operator=(const BookmarkEventRouter&) = delete;
  ~BookmarkEventRouter() override;

  // bookmarks::BookmarkModelObserver:
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
  void BookmarkNodeFaviconChanged(const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeChildrenReordered(
      const bookmarks::BookmarkNode* node) override;
  void ExtensiveBookmarkChangesBeginning() override;
  void ExtensiveBookmarkChangesEnded() override;

 private:
  // Helper to actually dispatch an event to extension listeners.
  void DispatchEvent(events::HistogramValue histogram_value,
                     const std::string& event_name,
                     base::Value::List event_args);

  raw_ptr<content::BrowserContext> browser_context_;
  raw_ptr<bookmarks::BookmarkModel> model_;
  raw_ptr<bookmarks::ManagedBookmarkService> managed_;
};

class BookmarksAPI : public BrowserContextKeyedAPI,
                     public EventRouter::Observer {
 public:
  explicit BookmarksAPI(content::BrowserContext* context);
  ~BookmarksAPI() override;

  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<BookmarksAPI>* GetFactoryInstance();

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;

 private:
  friend class BrowserContextKeyedAPIFactory<BookmarksAPI>;

  raw_ptr<content::BrowserContext> browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "BookmarksAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;

  // Created lazily upon OnListenerAdded.
  std::unique_ptr<BookmarkEventRouter> bookmark_event_router_;
};

class BookmarksGetFunction : public BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.get", BOOKMARKS_GET)

 protected:
  ~BookmarksGetFunction() override {}

  // BookmarksFunction:
  ResponseValue RunOnReady() override;
};

class BookmarksGetChildrenFunction : public BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.getChildren", BOOKMARKS_GETCHILDREN)

 protected:
  ~BookmarksGetChildrenFunction() override {}

  // BookmarksFunction:
  ResponseValue RunOnReady() override;
};

class BookmarksGetRecentFunction : public BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.getRecent", BOOKMARKS_GETRECENT)

 protected:
  ~BookmarksGetRecentFunction() override {}

  // BookmarksFunction:
  ResponseValue RunOnReady() override;
};

class BookmarksGetTreeFunction : public BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.getTree", BOOKMARKS_GETTREE)

 protected:
  ~BookmarksGetTreeFunction() override {}

  // BookmarksFunction:
  ResponseValue RunOnReady() override;
};

class BookmarksGetSubTreeFunction : public BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.getSubTree", BOOKMARKS_GETSUBTREE)

 protected:
  ~BookmarksGetSubTreeFunction() override {}

  // BookmarksFunction:
  ResponseValue RunOnReady() override;
};

class BookmarksSearchFunction : public BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.search", BOOKMARKS_SEARCH)

 protected:
  ~BookmarksSearchFunction() override {}

  // BookmarksFunction:
  ResponseValue RunOnReady() override;
};

class BookmarksRemoveFunctionBase : public BookmarksFunction {
 protected:
  ~BookmarksRemoveFunctionBase() override {}

  virtual bool is_recursive() const = 0;

  // BookmarksFunction:
  ResponseValue RunOnReady() override;
};

class BookmarksRemoveFunction : public BookmarksRemoveFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.remove", BOOKMARKS_REMOVE)

 protected:
  ~BookmarksRemoveFunction() override {}

  // BookmarksRemoveFunctionBase:
  bool is_recursive() const override;
};

class BookmarksRemoveTreeFunction : public BookmarksRemoveFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.removeTree", BOOKMARKS_REMOVETREE)

 protected:
  ~BookmarksRemoveTreeFunction() override {}

  // BookmarksRemoveFunctionBase:
  bool is_recursive() const override;
};

class BookmarksCreateFunction : public BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.create", BOOKMARKS_CREATE)

 protected:
  ~BookmarksCreateFunction() override {}

  // BookmarksFunction:
  ResponseValue RunOnReady() override;

 private:
  // Helper to create a bookmark node from a CreateDetails object. If a node
  // can't be created based on the given details, sets |error| and returns
  // nullptr.
  const bookmarks::BookmarkNode* CreateBookmarkNode(
      bookmarks::BookmarkModel* model,
      const api::bookmarks::CreateDetails& details,
      std::string* error);
};

class BookmarksMoveFunction : public BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.move", BOOKMARKS_MOVE)

 protected:
  ~BookmarksMoveFunction() override {}

  // BookmarksFunction:
  ResponseValue RunOnReady() override;
};

class BookmarksUpdateFunction : public BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.update", BOOKMARKS_UPDATE)

 protected:
  ~BookmarksUpdateFunction() override {}

  // BookmarksFunction:
  ResponseValue RunOnReady() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BOOKMARKS_BOOKMARKS_API_H_
