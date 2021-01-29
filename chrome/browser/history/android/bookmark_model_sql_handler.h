// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_ANDROID_BOOKMARK_MODEL_SQL_HANDLER_H_
#define CHROME_BROWSER_HISTORY_ANDROID_BOOKMARK_MODEL_SQL_HANDLER_H_

#include <stdint.h>

#include "base/macros.h"
#include "components/history/core/browser/android/sql_handler.h"

namespace bookmarks {
class BookmarkModel;
}

namespace history {

class URLDatabase;

// The SQL handler for bookmarking_mapping table.
class BookmarkModelSQLHandler : public SQLHandler {
 public:
  explicit BookmarkModelSQLHandler(URLDatabase* url_database);

  ~BookmarkModelSQLHandler() override;

  // SQLHandler overrides:
  bool Update(const HistoryAndBookmarkRow& row,
              const TableIDRows& ids_set) override;
  bool Delete(const TableIDRows& ids_set) override;
  bool Insert(HistoryAndBookmarkRow* row) override;

 private:
  // This class helps to modify the bookmark model in UI thread.
  // The instance of this class is created in history thread and posted to
  // UI thread to access the bookmark. All method must be run in UI thread.
  class Task : public base::RefCountedThreadSafe<Task> {
   public:
    // |profile| is the profile whose BookmarkModel will be modified.
    //
    // As this class is instantiated in history thread, the |profile| will be
    // checked to see if it is still valid in ProfileManger before it used to
    // get bookmark model in UI thread; So we can make sure the bookmark model
    // we working on is still valid at the time it is used.
    Task();

    // Add the a bookmark with the given |url| to mobile folder.
    void AddBookmarkToMobileFolder(const GURL& url,
                                   const base::string16& title);

    // Adds a bookmark with the given |url|, |title| and |parent_id|.
    void AddBookmark(const GURL& url,
                     const base::string16& title,
                     int64_t parent_id);

    // Removes the bookmark with the given |url|.
    void RemoveBookmark(const GURL& url);

    // Updates the given bookmark's title.
    void UpdateBookmarkTitle(const GURL& url,
                             const base::string16& title);

   private:
    friend class base::RefCountedThreadSafe<Task>;
    ~Task();

    // Returns profile_'s BookmarkModel if the profile_ is valid.
    bookmarks::BookmarkModel* GetBookmarkModel();

    DISALLOW_COPY_AND_ASSIGN(Task);
  };

  URLDatabase* url_database_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkModelSQLHandler);
};

}  // namespace history.

#endif  // CHROME_BROWSER_HISTORY_ANDROID_BOOKMARK_MODEL_SQL_HANDLER_H_
