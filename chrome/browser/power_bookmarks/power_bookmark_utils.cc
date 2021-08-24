// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/power_bookmarks/power_bookmark_utils.h"

#include <string>

#include "chrome/browser/power_bookmarks/proto/power_bookmark_meta.pb.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"

namespace power_bookmarks {

const char kPowerBookmarkMetaKey[] = "power_bookmark_meta";

std::unique_ptr<PowerBookmarkMeta> GetNodePowerBookmarkMeta(
    const bookmarks::BookmarkNode* node) {
  std::string proto_string;
  if (!node || !node->GetMetaInfo(kPowerBookmarkMetaKey, &proto_string))
    return nullptr;

  std::unique_ptr<PowerBookmarkMeta> meta =
      std::make_unique<PowerBookmarkMeta>();
  if (!meta->ParseFromString(proto_string))
    meta.reset();

  return meta;
}

void SetNodePowerBookmarkMeta(bookmarks::BookmarkModel* model,
                              const bookmarks::BookmarkNode* node,
                              std::unique_ptr<PowerBookmarkMeta> meta) {
  if (!model || !node)
    return;

  CHECK(meta);

  std::string proto_string;
  meta->SerializeToString(&proto_string);
  model->SetNodeMetaInfo(node, kPowerBookmarkMetaKey, proto_string);
}

void DeleteNodePowerBookmarkMeta(bookmarks::BookmarkModel* model,
                                 const bookmarks::BookmarkNode* node) {
  if (model && node)
    model->DeleteNodeMetaInfo(node, kPowerBookmarkMetaKey);
}

}  // namespace power_bookmarks
