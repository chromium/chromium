// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/managed_bookmarks_policy_handler.h"

#include <utility>

#include "base/values.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/managed/managed_bookmarks_tracker.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/url_formatter/url_fixer.h"
#include "url/gurl.h"

using bookmarks::ManagedBookmarksTracker;

namespace policy {

ManagedBookmarksPolicyHandler::ManagedBookmarksPolicyHandler(
    Schema chrome_schema)
    : SchemaValidatingPolicyHandler(
          key::kManagedBookmarks,
          chrome_schema.GetKnownProperty(key::kManagedBookmarks),
          SCHEMA_ALLOW_INVALID) {}

ManagedBookmarksPolicyHandler::~ManagedBookmarksPolicyHandler() {}

void ManagedBookmarksPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  std::unique_ptr<base::Value> value;
  if (!CheckAndGetValue(policies, NULL, &value))
    return;

  base::ListValue* list = NULL;
  if (!value || !value->GetAsList(&list))
    return;

  prefs->SetString(bookmarks::prefs::kManagedBookmarksFolderName,
                   GetFolderName(*list));
  FilterBookmarks(list);
  prefs->SetValue(bookmarks::prefs::kManagedBookmarks, std::move(value));
}

std::string
ManagedBookmarksPolicyHandler::GetFolderName(const base::ListValue& list) {
  // Iterate over the list, and try to find the FolderName.
  for (const auto& el : list) {
    const base::DictionaryValue* dict = NULL;
    if (!el.GetAsDictionary(&dict))
      continue;

    std::string name;
    if (dict->GetString(ManagedBookmarksTracker::kFolderName, &name)) {
      return name;
    }
  }

  // FolderName not present.
  return std::string();
}

void ManagedBookmarksPolicyHandler::FilterBookmarks(base::ListValue* list) {
  // Remove any non-conforming values found.
  auto it = list->begin();
  while (it != list->end()) {
    base::DictionaryValue* dict = NULL;
    if (!it->GetAsDictionary(&dict)) {
      it = list->Erase(it, NULL);
      continue;
    }

    std::string name;
    std::string url;
    base::ListValue* children = NULL;
    // Every bookmark must have a name, and then either a URL of a list of
    // child bookmarks.
    if (!dict->GetString(ManagedBookmarksTracker::kName, &name) ||
        (!dict->GetList(ManagedBookmarksTracker::kChildren, &children) &&
         !dict->GetString(ManagedBookmarksTracker::kUrl, &url))) {
      it = list->Erase(it, NULL);
      continue;
    }

    if (children) {
      // Ignore the URL if this bookmark has child nodes.
      dict->Remove(ManagedBookmarksTracker::kUrl, NULL);
      FilterBookmarks(children);
    } else {
      // Make sure the URL is valid before passing a bookmark to the pref.
      dict->Remove(ManagedBookmarksTracker::kChildren, NULL);
      GURL gurl = url_formatter::FixupURL(url, std::string());
      if (!gurl.is_valid()) {
        it = list->Erase(it, NULL);
        continue;
      }
      dict->SetString(ManagedBookmarksTracker::kUrl, gurl.spec());
    }

    ++it;
  }
}

}  // namespace policy
