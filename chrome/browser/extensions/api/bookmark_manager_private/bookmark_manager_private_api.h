// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BOOKMARK_MANAGER_PRIVATE_BOOKMARK_MANAGER_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_BOOKMARK_MANAGER_PRIVATE_BOOKMARK_MANAGER_PRIVATE_API_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/bookmarks_core/bookmarks_function.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/undo/bookmark_undo_service_factory.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/undo/bookmark_undo_service.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"
#include "ui/shell_dialogs/select_file_dialog.h"

class Profile;

namespace bookmarks {
struct BookmarkNodeData;
}

namespace content {
class BrowserContext;
class WebContents;
}

namespace extensions {

class BookmarkManagerPrivateEventRouter
    : public bookmarks::BaseBookmarkModelObserver {
 public:
  BookmarkManagerPrivateEventRouter(content::BrowserContext* browser_context,
                                    bookmarks::BookmarkModel* bookmark_model);
  ~BookmarkManagerPrivateEventRouter() override;

  // bookmarks::BaseBookmarkModelObserver:
  void BookmarkModelChanged() override;
  void BookmarkModelBeingDeleted() override;

 private:
  // Helper to actually dispatch an event to extension listeners.
  void DispatchEvent(events::HistogramValue histogram_value,
                     const std::string& event_name,
                     base::Value::List event_args);

  // Remembers the previous meta info of a node before it was changed.
  bookmarks::BookmarkNode::MetaInfoMap prev_meta_info_;

  raw_ptr<content::BrowserContext> browser_context_;
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
};

class BookmarkManagerPrivateAPI : public BrowserContextKeyedAPI,
                                  public EventRouter::Observer {
 public:
  explicit BookmarkManagerPrivateAPI(content::BrowserContext* browser_context);
  ~BookmarkManagerPrivateAPI() override;

  // BrowserContextKeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<BookmarkManagerPrivateAPI>*
      GetFactoryInstance();

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;

 private:
  friend class BrowserContextKeyedAPIFactory<BookmarkManagerPrivateAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "BookmarkManagerPrivateAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;

  raw_ptr<content::BrowserContext> browser_context_;

  // Created lazily upon OnListenerAdded.
  std::unique_ptr<BookmarkManagerPrivateEventRouter> event_router_;
};

// Class that handles the drag and drop related chrome.bookmarkManagerPrivate
// events. This class has one instance per bookmark manager tab.
class BookmarkManagerPrivateDragEventRouter
    : public BookmarkTabHelper::BookmarkDrag,
      public content::WebContentsUserData<
          BookmarkManagerPrivateDragEventRouter> {
 public:
  explicit BookmarkManagerPrivateDragEventRouter(
      content::WebContents* web_contents);
  BookmarkManagerPrivateDragEventRouter(
      const BookmarkManagerPrivateDragEventRouter&) = delete;
  BookmarkManagerPrivateDragEventRouter& operator=(
      const BookmarkManagerPrivateDragEventRouter&) = delete;
  ~BookmarkManagerPrivateDragEventRouter() override;

  // BookmarkTabHelper::BookmarkDrag interface
  void OnDragEnter(const bookmarks::BookmarkNodeData& data) override;
  void OnDragOver(const bookmarks::BookmarkNodeData& data) override;
  void OnDragLeave(const bookmarks::BookmarkNodeData& data) override;
  void OnDrop(const bookmarks::BookmarkNodeData& data) override;

  // The bookmark drag and drop data. This gets set after a drop was done on
  // the page. This returns nullptr if no data is available.
  const bookmarks::BookmarkNodeData* GetBookmarkNodeData();

  // Clears the drag and drop data.
  void ClearBookmarkNodeData();

 private:
  friend class content::WebContentsUserData<
      BookmarkManagerPrivateDragEventRouter>;
  // Helper to actually dispatch an event to extension listeners.
  void DispatchEvent(events::HistogramValue histogram_value,
                     const std::string& event_name,
                     base::Value::List args);

  raw_ptr<Profile> profile_;
  bookmarks::BookmarkNodeData bookmark_drag_data_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

class ClipboardBookmarkManagerFunction : public extensions::BookmarksFunction {
 protected:
  ~ClipboardBookmarkManagerFunction() override = default;

  ResponseValue CopyOrCut(bool cut, const std::vector<std::string>& id_list);
};

class BookmarkManagerPrivateCopyFunction
    : public ClipboardBookmarkManagerFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.copy",
                             BOOKMARKMANAGERPRIVATE_COPY)

 protected:
  ~BookmarkManagerPrivateCopyFunction() override = default;

  // BookmarksFunction:
  ResponseValue RunOnReady() override;
};

class BookmarkManagerPrivateCutFunction
    : public ClipboardBookmarkManagerFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.cut",
                             BOOKMARKMANAGERPRIVATE_CUT)

 protected:
  ~BookmarkManagerPrivateCutFunction() override = default;

  // BookmarksFunction:
  ResponseValue RunOnReady() override;
};

class BookmarkManagerPrivatePasteFunction
    : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.paste",
                             BOOKMARKMANAGERPRIVATE_PASTE)

 protected:
  ~BookmarkManagerPrivatePasteFunction() override = default;

  // BookmarksFunction:
  ResponseValue RunOnReady() override;
};

class BookmarkManagerPrivateCanPasteFunction
    : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.canPaste",
                             BOOKMARKMANAGERPRIVATE_CANPASTE)

 protected:
  ~BookmarkManagerPrivateCanPasteFunction() override = default;

  // BookmarksFunction:
  ResponseValue RunOnReady() override;
};

class BookmarkManagerPrivateSortChildrenFunction
    : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.sortChildren",
                             BOOKMARKMANAGERPRIVATE_SORTCHILDREN)

 protected:
  ~BookmarkManagerPrivateSortChildrenFunction() override = default;

  // BookmarksFunction:
  ResponseValue RunOnReady() override;
};

class BookmarkManagerPrivateStartDragFunction
    : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.startDrag",
                             BOOKMARKMANAGERPRIVATE_STARTDRAG)

 protected:
  ~BookmarkManagerPrivateStartDragFunction() override = default;

  // BookmarksFunction:
  ResponseValue RunOnReady() override;
};

class BookmarkManagerPrivateDropFunction
    : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.drop",
                             BOOKMARKMANAGERPRIVATE_DROP)

 protected:
  ~BookmarkManagerPrivateDropFunction() override = default;

  // BookmarksFunction:
  ResponseValue RunOnReady() override;
};

class BookmarkManagerPrivateGetSubtreeFunction
    : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.getSubtree",
                             BOOKMARKMANAGERPRIVATE_GETSUBTREE)

 protected:
  ~BookmarkManagerPrivateGetSubtreeFunction() override = default;

  // BookmarksFunction:
  ResponseValue RunOnReady() override;
};

class BookmarkManagerPrivateRemoveTreesFunction
    : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.removeTrees",
                             BOOKMARKMANAGERPRIVATE_REMOVETREES)

 protected:
  ~BookmarkManagerPrivateRemoveTreesFunction() override = default;

  // BookmarksFunction:
  ResponseValue RunOnReady() override;
};

class BookmarkManagerPrivateUndoFunction
    : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.undo",
                             BOOKMARKMANAGERPRIVATE_UNDO)

 protected:
  ~BookmarkManagerPrivateUndoFunction() override = default;

  // BookmarksFunction:
  ResponseValue RunOnReady() override;
};

class BookmarkManagerPrivateRedoFunction
    : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.redo",
                             BOOKMARKMANAGERPRIVATE_REDO)

 protected:
  ~BookmarkManagerPrivateRedoFunction() override = default;

  // BookmarksFunction:
  ResponseValue RunOnReady() override;
};

class BookmarkManagerPrivateOpenInNewTabFunction
    : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.openInNewTab",
                             BOOKMARKMANAGERPRIVATE_OPENINNEWTAB)

 protected:
  ~BookmarkManagerPrivateOpenInNewTabFunction() override = default;

  // BookmarksFunction:
  ResponseValue RunOnReady() override;
};

class BookmarkManagerPrivateOpenInNewWindowFunction
    : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.openInNewWindow",
                             BOOKMARKMANAGERPRIVATE_OPENINNEWWINDOW)

 protected:
  ~BookmarkManagerPrivateOpenInNewWindowFunction() override = default;

  // BookmarksFunction:
  ResponseValue RunOnReady() override;
};

class BookmarkManagerPrivateIOFunction : public BookmarksFunction,
                                         public ui::SelectFileDialog::Listener {
 public:
  BookmarkManagerPrivateIOFunction();

  // ui::SelectFileDialog::Listener:
  void FileSelected(const ui::SelectedFileInfo& file, int index) override = 0;
  void FileSelectionCanceled() override;

  void ShowSelectFileDialog(
      ui::SelectFileDialog::Type type,
      const base::FilePath& default_path);

 protected:
  ~BookmarkManagerPrivateIOFunction() override;

  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
};

class BookmarkManagerPrivateImportFunction
    : public BookmarkManagerPrivateIOFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.import",
                             BOOKMARKMANAGERPRIVATE_IMPORT)

  // BookmarkManagerIOFunction:
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;

 protected:
  ~BookmarkManagerPrivateImportFunction() override = default;

 private:
  // BookmarksFunction:
  ResponseValue RunOnReady() override;
};

class BookmarkManagerPrivateExportFunction
    : public BookmarkManagerPrivateIOFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.export",
                             BOOKMARKMANAGERPRIVATE_EXPORT)

  // BookmarkManagerIOFunction:
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;

 protected:
  ~BookmarkManagerPrivateExportFunction() override = default;

 private:
  // BookmarksFunction:
  ResponseValue RunOnReady() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BOOKMARK_MANAGER_PRIVATE_BOOKMARK_MANAGER_PRIVATE_API_H_
