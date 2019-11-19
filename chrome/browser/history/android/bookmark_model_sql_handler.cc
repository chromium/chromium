// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/android/bookmark_model_sql_handler.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/history/core/browser/url_database.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using base::Time;
using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using content::BrowserThread;

namespace history {

namespace {

// The interesting columns of this handler.
const HistoryAndBookmarkRow::ColumnID kInterestingColumns[] = {
    HistoryAndBookmarkRow::BOOKMARK, HistoryAndBookmarkRow::TITLE };

} // namespace

BookmarkModelSQLHandler::Task::Task() {
}

void BookmarkModelSQLHandler::Task::AddBookmarkToMobileFolder(
    const GURL& url,
    const base::string16& title) {
  BookmarkModel* bookmark_model = GetBookmarkModel();
  if (!bookmark_model)
    return;
  const BookmarkNode* mobile_node = bookmark_model->mobile_node();
  if (mobile_node)
    bookmark_model->AddURL(mobile_node, 0, title, url);
}

void BookmarkModelSQLHandler::Task::AddBookmark(const GURL& url,
                                                const base::string16& title,
                                                int64_t parent_id) {
  BookmarkModel* bookmark_model = GetBookmarkModel();
  if (!bookmark_model)
    return;
  const BookmarkNode* parent =
      bookmarks::GetBookmarkNodeByID(bookmark_model, parent_id);
  if (parent)
    bookmark_model->AddURL(parent, 0, title, url);
}

void BookmarkModelSQLHandler::Task::RemoveBookmark(const GURL& url) {
  BookmarkModel* bookmark_model = GetBookmarkModel();
  if (!bookmark_model)
    return;
  std::vector<const BookmarkNode*> nodes;
  bookmark_model->GetNodesByURL(url, &nodes);
  for (std::vector<const BookmarkNode*>::iterator i = nodes.begin();
       i != nodes.end(); ++i) {
    bookmark_model->Remove(*i);
  }
}

void BookmarkModelSQLHandler::Task::UpdateBookmarkTitle(
    const GURL& url,
    const base::string16& title) {
  BookmarkModel* bookmark_model = GetBookmarkModel();
  if (!bookmark_model)
    return;
  std::vector<const BookmarkNode*> nodes;
  bookmark_model->GetNodesByURL(url, &nodes);
  for (std::vector<const BookmarkNode*>::iterator i = nodes.begin();
       i != nodes.end(); ++i) {
    bookmark_model->SetTitle(*i, title);
  }
}


BookmarkModelSQLHandler::Task::~Task() {
}

BookmarkModel* BookmarkModelSQLHandler::Task::GetBookmarkModel() {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  if (!profile)
    return NULL;
  return BookmarkModelFactory::GetForBrowserContext(profile);
}

BookmarkModelSQLHandler::BookmarkModelSQLHandler(URLDatabase* url_database)
    : SQLHandler(kInterestingColumns, base::size(kInterestingColumns)),
      url_database_(url_database) {}

BookmarkModelSQLHandler::~BookmarkModelSQLHandler() {
}

bool BookmarkModelSQLHandler::Update(const HistoryAndBookmarkRow& row,
                                     const TableIDRows& ids_set) {
  for (TableIDRows::const_iterator i = ids_set.begin();
      i != ids_set.end(); ++i) {
    if (row.is_value_set_explicitly(HistoryAndBookmarkRow::BOOKMARK)) {
      if (row.is_bookmark()) {
        URLRow url_row;
        if (!url_database_->GetURLRow(i->url_id, &url_row))
          return false;
        if (row.is_value_set_explicitly(HistoryAndBookmarkRow::PARENT_ID)) {
          base::PostTask(
              FROM_HERE, {BrowserThread::UI},
              base::BindOnce(&BookmarkModelSQLHandler::Task::AddBookmark,
                             scoped_refptr<BookmarkModelSQLHandler::Task>(
                                 new BookmarkModelSQLHandler::Task()),
                             i->url, url_row.title(), row.parent_id()));
        } else {
          base::PostTask(
              FROM_HERE, {BrowserThread::UI},
              base::BindOnce(
                  &BookmarkModelSQLHandler::Task::AddBookmarkToMobileFolder,
                  scoped_refptr<BookmarkModelSQLHandler::Task>(
                      new BookmarkModelSQLHandler::Task()),
                  i->url, url_row.title()));
        }
      } else {
        base::PostTask(
            FROM_HERE, {BrowserThread::UI},
            base::BindOnce(&BookmarkModelSQLHandler::Task::RemoveBookmark,
                           scoped_refptr<BookmarkModelSQLHandler::Task>(
                               new BookmarkModelSQLHandler::Task()),
                           i->url));
      }
    } else if (row.is_value_set_explicitly(HistoryAndBookmarkRow::TITLE)) {
      base::PostTask(
          FROM_HERE, {BrowserThread::UI},
          base::BindOnce(&BookmarkModelSQLHandler::Task::UpdateBookmarkTitle,
                         scoped_refptr<BookmarkModelSQLHandler::Task>(
                             new BookmarkModelSQLHandler::Task()),
                         i->url, row.title()));
    }
  }
  return true;
}

bool BookmarkModelSQLHandler::Delete(const TableIDRows& ids_set) {
  for (TableIDRows::const_iterator i = ids_set.begin();
       i != ids_set.end(); ++i) {
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&BookmarkModelSQLHandler::Task::RemoveBookmark,
                       scoped_refptr<BookmarkModelSQLHandler::Task>(
                           new BookmarkModelSQLHandler::Task()),
                       i->url));
  }
  return true;
}

bool BookmarkModelSQLHandler::Insert(HistoryAndBookmarkRow* row) {
  DCHECK(row->is_value_set_explicitly(HistoryAndBookmarkRow::URL));
  if (!row->is_value_set_explicitly(HistoryAndBookmarkRow::BOOKMARK) ||
      !row->is_bookmark())
    return true;
  if (row->is_value_set_explicitly(HistoryAndBookmarkRow::PARENT_ID)) {
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(&BookmarkModelSQLHandler::Task::AddBookmark,
                                  scoped_refptr<BookmarkModelSQLHandler::Task>(
                                      new BookmarkModelSQLHandler::Task()),
                                  row->url(), row->title(), row->parent_id()));
  } else {
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(
            &BookmarkModelSQLHandler::Task::AddBookmarkToMobileFolder,
            scoped_refptr<BookmarkModelSQLHandler::Task>(
                new BookmarkModelSQLHandler::Task()),
            row->url(), row->title()));
  }
  return true;
}

}  // namespace history
