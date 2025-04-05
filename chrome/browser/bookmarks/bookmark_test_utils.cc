// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_test_utils.h"

#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"

namespace {

base::Value::List ConstructManagedBookmarks(size_t managed_bookmarks_size) {
  const GURL url("http://google.com/");
  base::Value::List bookmarks_list;
  for (size_t i = 0; i < managed_bookmarks_size; ++i) {
    base::Value::List folder_items;
    folder_items.Append(
        base::Value::Dict().Set("name", "Google").Set("url", url.spec()));
    bookmarks_list.Append(
        base::Value::Dict()
            .Set("name", "Bookmark folder " + base::NumberToString(i))
            .Set("children", std::move(folder_items)));
  }
  return bookmarks_list;
}

}  // namespace

std::unique_ptr<bookmarks::ManagedBookmarkService> CreateManagedBookmarkService(
    sync_preferences::TestingPrefServiceSyncable* prefs,
    size_t managed_bookmarks_size) {
  prefs->registry()->RegisterListPref(bookmarks::prefs::kManagedBookmarks);
  prefs->registry()->RegisterStringPref(
      bookmarks::prefs::kManagedBookmarksFolderName, std::string());

  prefs->SetString(bookmarks::prefs::kManagedBookmarksFolderName, "Managed");
  prefs->SetManagedPref(bookmarks::prefs::kManagedBookmarks,
                        ConstructManagedBookmarks(managed_bookmarks_size));

  return std::make_unique<bookmarks::ManagedBookmarkService>(
      prefs,
      base::BindRepeating([]() -> std::string { return "managedDomain.com"; }));
}

TestBookmarkClientWithManagedService::TestBookmarkClientWithManagedService(
    bookmarks::ManagedBookmarkService* managed_bookmark_service)
    : managed_bookmark_service_(managed_bookmark_service) {
  CHECK(managed_bookmark_service);
}

void TestBookmarkClientWithManagedService::Init(
    bookmarks::BookmarkModel* model) {
  managed_bookmark_service_->BookmarkModelCreated(model);
}
bookmarks::LoadManagedNodeCallback
TestBookmarkClientWithManagedService::GetLoadManagedNodeCallback() {
  return managed_bookmark_service_->GetLoadManagedNodeCallback();
}
bool TestBookmarkClientWithManagedService::CanSetPermanentNodeTitle(
    const bookmarks::BookmarkNode* permanent_node) {
  return managed_bookmark_service_->CanSetPermanentNodeTitle(permanent_node);
}
bool TestBookmarkClientWithManagedService::IsNodeManaged(
    const bookmarks::BookmarkNode* node) {
  return managed_bookmark_service_->IsNodeManaged(node);
}
