// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BOOKMARKS_TEST_BOOKMARKS_API_MATCHERS_H_
#define CHROME_BROWSER_EXTENSIONS_API_BOOKMARKS_TEST_BOOKMARKS_API_MATCHERS_H_

#include "base/strings/utf_string_conversions.h"
#include "chrome/common/extensions/api/bookmarks.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"

using ::testing::Eq;
using ::testing::ExplainMatchResult;
using ::testing::Pointwise;

namespace extensions {

// Matches a `BookmarkTreeNode` against a `BookmarkNode`.
//
// const extensions::api::bookmarks::BookmarkTreeNode& arg
// const bookmarks::BookmarkNode* bookmark_node
MATCHER_P(MatchesBookmarkNode, bookmark_node, "") {
  return ExplainMatchResult(Eq(base::NumberToString(bookmark_node->id())),
                            arg.id, result_listener) &&
         ExplainMatchResult(
             Eq(base::NumberToString(bookmark_node->parent()->id())),
             arg.parent_id, result_listener) &&
         ExplainMatchResult(Eq(bookmark_node->GetTitle()),
                            base::UTF8ToUTF16(arg.title), result_listener) &&
         ExplainMatchResult(Eq(bookmark_node->url().spec()),
                            arg.url.value_or(""), result_listener);
}

// Matches a `BookmarkTreeNode` against a `BookmarkNode`.
MATCHER(MatchesBookmarkNode, "") {
  const extensions::api::bookmarks::BookmarkTreeNode& bookmark_tree_node =
      std::get<0>(arg);
  const bookmarks::BookmarkNode& bookmark_node = *std::get<1>(arg);

  return ExplainMatchResult(Eq(base::NumberToString(bookmark_node.id())),
                            bookmark_tree_node.id, result_listener) &&
         ExplainMatchResult(
             Eq(base::NumberToString(bookmark_node.parent()->id())),
             bookmark_tree_node.parent_id, result_listener) &&
         ExplainMatchResult(Eq(bookmark_node.GetTitle()),
                            base::UTF8ToUTF16(bookmark_tree_node.title),
                            result_listener) &&
         ExplainMatchResult(Eq(bookmark_node.url().spec()),
                            bookmark_tree_node.url.value_or(""),
                            result_listener);
}

// Matches a `base::Value::List` of `BookmarkTreeNode`s against the provided
// `BookmarkNode`s.
MATCHER_P(ResultMatchesNodes, nodes, "") {
  std::vector<extensions::api::bookmarks::BookmarkTreeNode>
      result_bookmark_tree_nodes;
  std::ranges::transform(
      arg, std::back_inserter(result_bookmark_tree_nodes),
      [](const base::Value& value) {
        return extensions::api::bookmarks::BookmarkTreeNode::FromValue(value)
            .value();
      });
  return ExplainMatchResult(Pointwise(MatchesBookmarkNode(), nodes),
                            result_bookmark_tree_nodes, result_listener);
}

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BOOKMARKS_TEST_BOOKMARKS_API_MATCHERS_H_
