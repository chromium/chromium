// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POWER_BOOKMARKS_POWER_BOOKMARK_UTILS_H_
#define CHROME_BROWSER_POWER_BOOKMARKS_POWER_BOOKMARK_UTILS_H_

#include <memory>
#include <vector>

#include "components/bookmarks/browser/bookmark_utils.h"

class GURL;

namespace base {
class GUID;
class Time;
}  // namespace base

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace content {
class WebContents;
}

namespace power_bookmarks {

class PowerBookmarkMeta;

struct PowerBookmarkQueryFields : bookmarks::QueryFields {
  PowerBookmarkQueryFields();
  ~PowerBookmarkQueryFields();

  std::vector<std::u16string> tags;

  // If his field is left null, the root of the bookmark model will be searched.
  const bookmarks::BookmarkNode* folder{nullptr};
};

// This is the key for the storage of PowerBookmarkMeta in bookmarks' meta_info
// map.
extern const char kPowerBookmarkMetaKey[];

// Add a bookmark to the provided model. This version is similar to the method
// on BookmarkModel with the addition of polling extra metadata providers and
// attaching it to the node prior to returning it to the client.
const bookmarks::BookmarkNode* AddURL(
    content::WebContents* web_contents,
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* parent,
    size_t index,
    const std::u16string& title,
    const GURL& url,
    bookmarks::BookmarkNode::MetaInfoMap* meta_info = nullptr,
    absl::optional<base::Time> creation_time = absl::nullopt,
    absl::optional<base::GUID> guid = absl::nullopt);

// Get the PowerBookmarkMeta for a node. The ownership of the returned object
// is transferred to the caller and a new instance is created each time this is
// called. If the node has no meta, nullprt is returned.
std::unique_ptr<PowerBookmarkMeta> GetNodePowerBookmarkMeta(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* node);

// Set or overwrite the PowerBookmarkMeta for a node.
void SetNodePowerBookmarkMeta(bookmarks::BookmarkModel* model,
                              const bookmarks::BookmarkNode* node,
                              std::unique_ptr<PowerBookmarkMeta> meta);

// Remove the PowerBookmarkMeta from a node.
void DeleteNodePowerBookmarkMeta(bookmarks::BookmarkModel* model,
                                 const bookmarks::BookmarkNode* node);

// Largely copied from bookmark_utils, this function finds up to \max_count\
// bookmarks in \model\ matching the properties provided in |query\. Unlike its
// counterpart in bookmark_utils, this method is capable of searching and
// filtering on tags. A list of tags can be provided that will produce
// bookmarks that at least have those tags. The bookmark's tags will also be
// tested against the text search query. Output is put into \nodes\.
void GetBookmarksMatchingProperties(
    bookmarks::BookmarkModel* model,
    const PowerBookmarkQueryFields& query,
    size_t max_count,
    std::vector<const bookmarks::BookmarkNode*>* nodes);

// Encode the provided metadata into |out| so it can be safely stored as JSON
// in the persistence layer.
void EncodeMetaForStorage(const PowerBookmarkMeta& meta, std::string* out);

// Decode metadata into |out| and return whether the operation was successful.
bool DecodeMetaFromStorage(const std::string& data, PowerBookmarkMeta* out);

}  // namespace power_bookmarks

#endif  // CHROME_BROWSER_POWER_BOOKMARKS_POWER_BOOKMARK_UTILS_H_
