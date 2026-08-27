// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/bookmark_management_tool.h"

#include <memory>
#include <string>

#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/bookmark_management_tool_request.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

namespace actor {

namespace {

class ActorBookmarkManagementToolBrowserTest : public ActorToolsTest {
 public:
  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model());
  }

  bookmarks::BookmarkModel* bookmark_model() {
    return BookmarkModelFactory::GetForBrowserContext(GetProfile());
  }
};

IN_PROC_BROWSER_TEST_F(ActorBookmarkManagementToolBrowserTest,
                       AddAndRemoveBookmark) {
  const GURL test_url("https://example.com/test_page");
  const std::u16string test_title = u"Example Bookmark";

  // Initially, no bookmark exists for this URL.
  EXPECT_FALSE(bookmark_model()->IsBookmarked(test_url));

  // Add the bookmark.
  {
    std::unique_ptr<ToolRequest> request =
        std::make_unique<AddBookmarkToolRequest>(test_url, test_title);
    ActResultFuture result;
    actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());
    ExpectOkResult(result);
  }

  // Verify the bookmark exists with the expected URL and title.
  EXPECT_TRUE(bookmark_model()->IsBookmarked(test_url));
  auto nodes = bookmark_model()->GetNodesByURL(test_url);
  ASSERT_EQ(nodes.size(), 1u);
  EXPECT_EQ(nodes[0]->GetTitle(), test_title);

  // Remove the bookmark.
  {
    std::unique_ptr<ToolRequest> request =
        std::make_unique<RemoveBookmarkToolRequest>(test_url);
    ActResultFuture result;
    actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());
    ExpectOkResult(result);
  }

  // Verify the bookmark was removed.
  EXPECT_FALSE(bookmark_model()->IsBookmarked(test_url));
}

IN_PROC_BROWSER_TEST_F(ActorBookmarkManagementToolBrowserTest,
                       InvalidUrlReturnsError) {
  // Empty URL should return kArgumentsInvalid.
  {
    std::unique_ptr<ToolRequest> request =
        std::make_unique<AddBookmarkToolRequest>(GURL(), u"Invalid URL");
    ActResultFuture result;
    actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());
    ExpectErrorResult(result, mojom::ActionResultCode::kArgumentsInvalid);
  }

  // Malformed URL should return kArgumentsInvalid.
  {
    std::unique_ptr<ToolRequest> request =
        std::make_unique<RemoveBookmarkToolRequest>(GURL("invalid_url_string"));
    ActResultFuture result;
    actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());
    ExpectErrorResult(result, mojom::ActionResultCode::kArgumentsInvalid);
  }
}

}  // namespace

}  // namespace actor
