// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BOOKMARK_MANAGER_PRIVATE_BOOKMARK_MANAGER_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_BOOKMARK_MANAGER_PRIVATE_BOOKMARK_MANAGER_PRIVATE_API_H_

#include <memory>

#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/bookmarks/bookmarks_api.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/undo/bookmark_undo_service_factory.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/undo/bookmark_undo_service.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"

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
  void BookmarkModelBeingDeleted(bookmarks::BookmarkModel* model) override;

 private:
  // Helper to actually dispatch an event to extension listeners.
  void DispatchEvent(events::HistogramValue histogram_value,
                     const std::string& event_name,
                     std::unique_ptr<base::ListValue> event_args);

  // Remembers the previous meta info of a node before it was changed.
  bookmarks::BookmarkNode::MetaInfoMap prev_meta_info_;

  content::BrowserContext* browser_context_;
  bookmarks::BookmarkModel* bookmark_model_;
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

  content::BrowserContext* browser_context_;

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
  ~BookmarkManagerPrivateDragEventRouter() override;

  // BookmarkTabHelper::BookmarkDrag interface
  void OnDragEnter(const bookmarks::BookmarkNodeData& data) override;
  void OnDragOver(const bookmarks::BookmarkNodeData& data) override;
  void OnDragLeave(const bookmarks::BookmarkNodeData& data) override;
  void OnDrop(const bookmarks::BookmarkNodeData& data) override;

  // The bookmark drag and drop data. This gets set after a drop was done on
  // the page. This returns NULL if no data is available.
  const bookmarks::BookmarkNodeData* GetBookmarkNodeData();

  // Clears the drag and drop data.
  void ClearBookmarkNodeData();

 private:
  friend class content::WebContentsUserData<
      BookmarkManagerPrivateDragEventRouter>;
  // Helper to actually dispatch an event to extension listeners.
  void DispatchEvent(events::HistogramValue histogram_value,
                     const std::string& event_name,
                     std::unique_ptr<base::ListValue> args);

  content::WebContents* web_contents_;
  Profile* profile_;
  bookmarks::BookmarkNodeData bookmark_drag_data_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(BookmarkManagerPrivateDragEventRouter);
};

class ClipboardBookmarkManagerFunction : public extensions::BookmarksFunction {
 protected:
  ~ClipboardBookmarkManagerFunction() override {}

  bool CopyOrCut(bool cut, const std::vector<std::string>& id_list);
};

class BookmarkManagerPrivateCopyFunction
    : public ClipboardBookmarkManagerFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.copy",
                             BOOKMARKMANAGERPRIVATE_COPY)

 protected:
  ~BookmarkManagerPrivateCopyFunction() override {}

  // ExtensionFunction:
  bool RunOnReady() override;
};

class BookmarkManagerPrivateCutFunction
    : public ClipboardBookmarkManagerFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.cut",
                             BOOKMARKMANAGERPRIVATE_CUT)

 protected:
  ~BookmarkManagerPrivateCutFunction() override {}

  // ExtensionFunction:
  bool RunOnReady() override;
};

class BookmarkManagerPrivatePasteFunction
    : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.paste",
                             BOOKMARKMANAGERPRIVATE_PASTE)

 protected:
  ~BookmarkManagerPrivatePasteFunction() override {}

  // ExtensionFunction:
  bool RunOnReady() override;
};

class BookmarkManagerPrivateCanPasteFunction
    : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.canPaste",
                             BOOKMARKMANAGERPRIVATE_CANPASTE)

 protected:
  ~BookmarkManagerPrivateCanPasteFunction() override {}

  // ExtensionFunction:
  bool RunOnReady() override;
};

class BookmarkManagerPrivateSortChildrenFunction
    : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.sortChildren",
                             BOOKMARKMANAGERPRIVATE_SORTCHILDREN)

 protected:
  ~BookmarkManagerPrivateSortChildrenFunction() override {}

  // ExtensionFunction:
  bool RunOnReady() override;
};

class BookmarkManagerPrivateStartDragFunction
    : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.startDrag",
                             BOOKMARKMANAGERPRIVATE_STARTDRAG)

 protected:
  ~BookmarkManagerPrivateStartDragFunction() override {}

  // ExtensionFunction:
  bool RunOnReady() override;
};

class BookmarkManagerPrivateDropFunction
    : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.drop",
                             BOOKMARKMANAGERPRIVATE_DROP)

 protected:
  ~BookmarkManagerPrivateDropFunction() override {}

  // ExtensionFunction:
  bool RunOnReady() override;
};

class BookmarkManagerPrivateGetSubtreeFunction
    : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.getSubtree",
                             BOOKMARKMANAGERPRIVATE_GETSUBTREE)

 protected:
  ~BookmarkManagerPrivateGetSubtreeFunction() override {}

  // ExtensionFunction:
  bool RunOnReady() override;
};

class BookmarkManagerPrivateRemoveTreesFunction
    : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.removeTrees",
                             BOOKMARKMANAGERPRIVATE_REMOVETREES)

 protected:
  ~BookmarkManagerPrivateRemoveTreesFunction() override {}

  // ExtensionFunction:
  bool RunOnReady() override;
};

class BookmarkManagerPrivateUndoFunction
    : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.undo",
                             BOOKMARKMANAGERPRIVATE_UNDO)

 protected:
  ~BookmarkManagerPrivateUndoFunction() override {}

  // ExtensionFunction:
  bool RunOnReady() override;
};

class BookmarkManagerPrivateRedoFunction
    : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.redo",
                             BOOKMARKMANAGERPRIVATE_REDO)

 protected:
  ~BookmarkManagerPrivateRedoFunction() override {}

  // ExtensionFunction:
  bool RunOnReady() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BOOKMARK_MANAGER_PRIVATE_BOOKMARK_MANAGER_PRIVATE_API_H_
