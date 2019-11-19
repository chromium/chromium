// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/chrome_history_client.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "chrome/browser/history/chrome_history_backend_client.h"
#include "chrome/browser/history/history_utils.h"
#include "chrome/browser/profiles/sql_init_error_message_ids.h"
#include "chrome/browser/ui/profile_error_dialog.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/history/core/browser/history_service.h"

ChromeHistoryClient::ChromeHistoryClient(
    bookmarks::BookmarkModel* bookmark_model)
    : bookmark_model_(bookmark_model) {
  if (bookmark_model_)
    bookmark_model_->AddObserver(this);
}

ChromeHistoryClient::~ChromeHistoryClient() {
  StopObservingBookmarkModel();
}

void ChromeHistoryClient::OnHistoryServiceCreated(
    history::HistoryService* history_service) {
  if (bookmark_model_) {
    on_bookmarks_removed_ =
        base::Bind(&history::HistoryService::URLsNoLongerBookmarked,
                   base::Unretained(history_service));
    favicons_changed_subscription_ =
        history_service->AddFaviconsChangedCallback(
            base::Bind(&bookmarks::BookmarkModel::OnFaviconsChanged,
                       base::Unretained(bookmark_model_)));
  }
}

void ChromeHistoryClient::Shutdown() {
  favicons_changed_subscription_.reset();
  StopObservingBookmarkModel();
}

bool ChromeHistoryClient::CanAddURL(const GURL& url) {
  return CanAddURLToHistory(url);
}

void ChromeHistoryClient::NotifyProfileError(sql::InitStatus init_status,
                                             const std::string& diagnostics) {
  ShowProfileErrorDialog(ProfileErrorType::HISTORY,
                         SqlInitStatusToMessageId(init_status), diagnostics);
}

std::unique_ptr<history::HistoryBackendClient>
ChromeHistoryClient::CreateBackendClient() {
  return std::make_unique<ChromeHistoryBackendClient>(
      bookmark_model_ ? bookmark_model_->model_loader() : nullptr);
}

void ChromeHistoryClient::StopObservingBookmarkModel() {
  if (!bookmark_model_)
    return;
  bookmark_model_->RemoveObserver(this);
  bookmark_model_ = nullptr;
}

void ChromeHistoryClient::BookmarkModelChanged() {
}

void ChromeHistoryClient::BookmarkModelBeingDeleted(
    bookmarks::BookmarkModel* model) {
  DCHECK_EQ(model, bookmark_model_);
  StopObservingBookmarkModel();
}

void ChromeHistoryClient::BookmarkNodeRemoved(
    bookmarks::BookmarkModel* bookmark_model,
    const bookmarks::BookmarkNode* parent,
    size_t old_index,
    const bookmarks::BookmarkNode* node,
    const std::set<GURL>& removed_urls) {
  BaseBookmarkModelObserver::BookmarkNodeRemoved(bookmark_model, parent,
                                                 old_index, node, removed_urls);
  if (on_bookmarks_removed_)
    on_bookmarks_removed_.Run(removed_urls);
}

void ChromeHistoryClient::BookmarkAllUserNodesRemoved(
    bookmarks::BookmarkModel* bookmark_model,
    const std::set<GURL>& removed_urls) {
  BaseBookmarkModelObserver::BookmarkAllUserNodesRemoved(bookmark_model,
                                                         removed_urls);
  if (on_bookmarks_removed_)
    on_bookmarks_removed_.Run(removed_urls);
}
