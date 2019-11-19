// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BOOKMARKS_BOOKMARKS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_BOOKMARKS_BOOKMARKS_API_H_

#include <stdint.h>

#include <list>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace base {
class FilePath;
class ListValue;
}

namespace bookmarks {
class BookmarkModel;
class ManagedBookmarkService;
}

namespace content {
class BrowserContext;
}

namespace extensions {

namespace api {
namespace bookmarks {
struct CreateDetails;
}
}

// Observes BookmarkModel and then routes the notifications as events to
// the extension system.
class BookmarkEventRouter : public bookmarks::BookmarkModelObserver {
 public:
  explicit BookmarkEventRouter(Profile* profile);
  ~BookmarkEventRouter() override;

  // bookmarks::BookmarkModelObserver:
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
  void BookmarkNodeFaviconChanged(bookmarks::BookmarkModel* model,
                                  const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeChildrenReordered(
      bookmarks::BookmarkModel* model,
      const bookmarks::BookmarkNode* node) override;
  void ExtensiveBookmarkChangesBeginning(
      bookmarks::BookmarkModel* model) override;
  void ExtensiveBookmarkChangesEnded(bookmarks::BookmarkModel* model) override;

 private:
  // Helper to actually dispatch an event to extension listeners.
  void DispatchEvent(events::HistogramValue histogram_value,
                     const std::string& event_name,
                     std::unique_ptr<base::ListValue> event_args);

  content::BrowserContext* browser_context_;
  bookmarks::BookmarkModel* model_;
  bookmarks::ManagedBookmarkService* managed_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkEventRouter);
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

  content::BrowserContext* browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "BookmarksAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;

  // Created lazily upon OnListenerAdded.
  std::unique_ptr<BookmarkEventRouter> bookmark_event_router_;
};

class BookmarksFunction : public ChromeAsyncExtensionFunction,
                          public bookmarks::BaseBookmarkModelObserver {
 public:
  // ChromeAsyncExtensionFunction:
  bool RunAsync() override;

 protected:
  ~BookmarksFunction() override {}

  // RunAsync semantic equivalent called when the bookmarks are ready.
  virtual bool RunOnReady() = 0;

  // Helper to get the BookmarkModel.
  bookmarks::BookmarkModel* GetBookmarkModel();

  // Helper to get the ManagedBookmarkService.
  bookmarks::ManagedBookmarkService* GetManagedBookmarkService();

  // Helper to get the bookmark id as int64_t from the given string id.
  // Sets error_ to an error string if the given id string can't be parsed
  // as an int64_t. In case of error, doesn't change id and returns false.
  bool GetBookmarkIdAsInt64(const std::string& id_string, int64_t* id);

  // Helper to get the bookmark node from a given string id.
  // If the given id can't be parsed or doesn't refer to a valid node, sets
  // error_ and returns NULL.
  const bookmarks::BookmarkNode* GetBookmarkNodeFromId(
      const std::string& id_string);

  // Helper to create a bookmark node from a CreateDetails object. If a node
  // can't be created based on the given details, sets error_ and returns NULL.
  const bookmarks::BookmarkNode* CreateBookmarkNode(
      bookmarks::BookmarkModel* model,
      const api::bookmarks::CreateDetails& details,
      const bookmarks::BookmarkNode::MetaInfoMap* meta_info);

  // Helper that checks if bookmark editing is enabled. If it's not, this sets
  // error_ to the appropriate error string.
  bool EditBookmarksEnabled();

  // Helper that checks if |node| can be modified. Returns false if |node|
  // is NULL, or a managed node, or the root node. In these cases the node
  // can't be edited, can't have new child nodes appended, and its direct
  // children can't be moved or reordered.
  bool CanBeModified(const bookmarks::BookmarkNode* node);

 private:
  // bookmarks::BaseBookmarkModelObserver:
  void BookmarkModelChanged() override;
  void BookmarkModelLoaded(bookmarks::BookmarkModel* model,
                           bool ids_reassigned) override;

  void RunAndSendResponse();
};

class BookmarksGetFunction : public BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.get", BOOKMARKS_GET)

 protected:
  ~BookmarksGetFunction() override {}

  // BookmarksFunction:
  bool RunOnReady() override;
};

class BookmarksGetChildrenFunction : public BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.getChildren", BOOKMARKS_GETCHILDREN)

 protected:
  ~BookmarksGetChildrenFunction() override {}

  // BookmarksFunction:
  bool RunOnReady() override;
};

class BookmarksGetRecentFunction : public BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.getRecent", BOOKMARKS_GETRECENT)

 protected:
  ~BookmarksGetRecentFunction() override {}

  // BookmarksFunction:
  bool RunOnReady() override;
};

class BookmarksGetTreeFunction : public BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.getTree", BOOKMARKS_GETTREE)

 protected:
  ~BookmarksGetTreeFunction() override {}

  // BookmarksFunction:
  bool RunOnReady() override;
};

class BookmarksGetSubTreeFunction : public BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.getSubTree", BOOKMARKS_GETSUBTREE)

 protected:
  ~BookmarksGetSubTreeFunction() override {}

  // BookmarksFunction:
  bool RunOnReady() override;
};

class BookmarksSearchFunction : public BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.search", BOOKMARKS_SEARCH)

 protected:
  ~BookmarksSearchFunction() override {}

  // BookmarksFunction:
  bool RunOnReady() override;
};

class BookmarksRemoveFunctionBase : public BookmarksFunction {
 protected:
  ~BookmarksRemoveFunctionBase() override {}

  virtual bool is_recursive() const = 0;

  // BookmarksFunction:
  bool RunOnReady() override;
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
  bool RunOnReady() override;
};

class BookmarksMoveFunction : public BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.move", BOOKMARKS_MOVE)

 protected:
  ~BookmarksMoveFunction() override {}

  // BookmarksFunction:
  bool RunOnReady() override;
};

class BookmarksUpdateFunction : public BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.update", BOOKMARKS_UPDATE)

 protected:
  ~BookmarksUpdateFunction() override {}

  // BookmarksFunction:
  bool RunOnReady() override;
};

class BookmarksIOFunction : public BookmarksFunction,
                            public ui::SelectFileDialog::Listener {
 public:
  BookmarksIOFunction();

  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override = 0;

  // ui::SelectFileDialog::Listener:
  void MultiFilesSelected(const std::vector<base::FilePath>& files,
                          void* params) override;
  void FileSelectionCanceled(void* params) override;

  void ShowSelectFileDialog(
      ui::SelectFileDialog::Type type,
      const base::FilePath& default_path);

 protected:
  ~BookmarksIOFunction() override;

  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
};

class BookmarksImportFunction : public BookmarksIOFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.import", BOOKMARKS_IMPORT)

  // BookmarkManagerIOFunction:
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;

 private:
  ~BookmarksImportFunction() override {}

  // BookmarksFunction:
  bool RunOnReady() override;
};

class BookmarksExportFunction : public BookmarksIOFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.export", BOOKMARKS_EXPORT)

  // BookmarkManagerIOFunction:
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;

 private:
  ~BookmarksExportFunction() override {}

  // BookmarksFunction:
  bool RunOnReady() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BOOKMARKS_BOOKMARKS_API_H_
