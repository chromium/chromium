// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bookmarks_core/bookmarks_function.h"

#include <string>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/extensions/api/bookmarks/bookmarks_api_watcher.h"
#include "chrome/browser/extensions/bookmarks/bookmarks_error_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using bookmarks::ManagedBookmarkService;

namespace extensions {

ExtensionFunction::ResponseAction BookmarksFunction::Run() {
  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(GetProfile());
  if (!model->loaded()) {
    // Bookmarks are not ready yet.  We'll wait.
    model->AddObserver(this);
    AddRef();  // Balanced in BookmarkModelLoaded().
    return RespondLater();
  }

  ResponseValue response = RunOnReady();
  return RespondNow(std::move(response));
}

BookmarkModel* BookmarksFunction::GetBookmarkModel() {
  return BookmarkModelFactory::GetForBrowserContext(GetProfile());
}

ManagedBookmarkService* BookmarksFunction::GetManagedBookmarkService() {
  return ManagedBookmarkServiceFactory::GetForProfile(GetProfile());
}

const BookmarkNode* BookmarksFunction::GetBookmarkNodeFromId(
    const std::string& id_string,
    std::string* error) {
  int64_t id;
  if (!base::StringToInt64(id_string, &id)) {
    *error = bookmarks_errors::kInvalidIdError;
    return nullptr;
  }

  const BookmarkNode* node = bookmarks::GetBookmarkNodeByID(
      BookmarkModelFactory::GetForBrowserContext(GetProfile()), id);
  if (!node) {
    *error = bookmarks_errors::kNoNodeError;
  }

  return node;
}

bool BookmarksFunction::EditBookmarksEnabled() {
  PrefService* prefs = user_prefs::UserPrefs::Get(GetProfile());
  return prefs->GetBoolean(bookmarks::prefs::kEditBookmarksEnabled);
}

bool BookmarksFunction::CanBeModified(const BookmarkNode* node,
                                      std::string* error) {
  if (!node) {
    *error = bookmarks_errors::kNoParentError;
    return false;
  }
  if (node->is_root()) {
    *error = bookmarks_errors::kModifySpecialError;
    return false;
  }
  ManagedBookmarkService* managed = GetManagedBookmarkService();
  if (bookmarks::IsDescendantOf(node, managed->managed_node())) {
    *error = bookmarks_errors::kModifyManagedError;
    return false;
  }
  return true;
}

Profile* BookmarksFunction::GetProfile() {
  return Profile::FromBrowserContext(browser_context());
}

void BookmarksFunction::OnResponded() {
  DCHECK(response_type());
  if (*response_type() == ExtensionFunction::SUCCEEDED) {
    BookmarksApiWatcher::GetForBrowserContext(browser_context())
        ->NotifyApiInvoked(this);
  }
}

void BookmarksFunction::BookmarkModelChanged() {}

void BookmarksFunction::BookmarkModelLoaded(bool ids_reassigned) {
  GetBookmarkModel()->RemoveObserver(this);

  ResponseValue response = RunOnReady();
  Respond(std::move(response));

  Release();  // Balanced in Run().
}

}  // namespace extensions
