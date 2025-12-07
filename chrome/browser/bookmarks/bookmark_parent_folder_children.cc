// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_parent_folder_children.h"

#include <cstddef>

#include "base/check_op.h"
#include "chrome/browser/bookmarks/permanent_folder_ordering_tracker.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

using bookmarks::BookmarkNode;

BookmarkParentFolderChildren::BookmarkParentFolderChildren(
    const BookmarkNode* node)
    : children_provider_(node) {
  CHECK(node);
  // For all permanent node types except the managed node,
  // `PermanentFolderOrderingTracker` is required.
  CHECK_NE(node->type(), BookmarkNode::Type::BOOKMARK_BAR);
  CHECK_NE(node->type(), BookmarkNode::Type::OTHER_NODE);
  CHECK_NE(node->type(), BookmarkNode::Type::MOBILE);
}

BookmarkParentFolderChildren::BookmarkParentFolderChildren(
    const PermanentFolderOrderingTracker* tracker)
    : children_provider_(tracker) {
  CHECK(tracker);
}

BookmarkParentFolderChildren::~BookmarkParentFolderChildren() = default;

BookmarkParentFolderChildren::Iterator BookmarkParentFolderChildren::begin()
    const {
  return Iterator(this, 0);
}

BookmarkParentFolderChildren::Iterator BookmarkParentFolderChildren::end()
    const {
  return Iterator(this, size());
}

const BookmarkNode* BookmarkParentFolderChildren::operator[](
    size_t index) const {
  CHECK_LT(index, size());
  return std::visit(
      absl::Overload{
          [index](const BookmarkNode* parent) -> const BookmarkNode* {
            return parent->children()[index].get();
          },
          [index](const PermanentFolderOrderingTracker* tracker) {
            return tracker->GetNodeAtIndex(index);
          }},
      children_provider_);
}

size_t BookmarkParentFolderChildren::size() const {
  return std::visit(
      absl::Overload{
          [](const BookmarkNode* parent) { return parent->children().size(); },
          [](const PermanentFolderOrderingTracker* tracker) {
            return tracker->GetChildrenCount();
          }},
      children_provider_);
}

BookmarkParentFolderChildren::Iterator::Iterator(
    const BookmarkParentFolderChildren* parent,
    size_t index)
    : parent_(parent), index_(index) {}

BookmarkParentFolderChildren::Iterator&
BookmarkParentFolderChildren::Iterator::operator++() {
  ++index_;
  return *this;
}

BookmarkParentFolderChildren::Iterator
BookmarkParentFolderChildren::Iterator::operator+(int offset) const {
  Iterator result = *this;
  result.index_ += offset;
  return result;
}

const BookmarkNode* BookmarkParentFolderChildren::Iterator::operator*() const {
  return (*parent_)[index_];
}
