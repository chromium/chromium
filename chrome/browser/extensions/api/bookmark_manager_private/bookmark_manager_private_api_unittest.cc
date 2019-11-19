// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bookmark_manager_private/bookmark_manager_private_api.h"

#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "extensions/browser/api_test_utils.h"

namespace extensions {

class BookmarkManagerPrivateApiUnitTest : public ExtensionServiceTestBase {
 public:
  BookmarkManagerPrivateApiUnitTest() {}

  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();
    profile_->CreateBookmarkModel(false);
    model_ = BookmarkModelFactory::GetForBrowserContext(profile());
    bookmarks::test::WaitForBookmarkModelToLoad(model_);

    const bookmarks::BookmarkNode* node =
        model_->AddURL(model_->other_node(), 0, base::ASCIIToUTF16("Goog"),
                       GURL("https://www.google.com"));
    // Store node->id() as we will delete |node| in RunOnDeletedNode().
    node_id_ = base::NumberToString(node->id());
  }

  const bookmarks::BookmarkModel* model() const { return model_; }
  std::string node_id() const { return node_id_; }

 private:
  bookmarks::BookmarkModel* model_ = nullptr;
  std::string node_id_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkManagerPrivateApiUnitTest);
};

// Tests that running ExtensionFunction-s on deleted bookmark node gracefully
// fails.
// Regression test for https://crbug.com/739260.
TEST_F(BookmarkManagerPrivateApiUnitTest, RunOnDeletedNode) {
  // Remove our only bookmark node.
  auto remove_function = base::MakeRefCounted<BookmarksRemoveFunction>();
  api_test_utils::RunFunction(remove_function.get(),
                              base::StringPrintf("[\"%s\"]", node_id().c_str()),
                              profile());

  // Call bookmarkManagerPrivate.copy() with the removed bookmark node's id.
  auto copy_function =
      base::MakeRefCounted<BookmarkManagerPrivateCopyFunction>();
  EXPECT_EQ(
      "Could not find bookmark nodes with given ids.",
      api_test_utils::RunFunctionAndReturnError(
          copy_function.get(),
          base::StringPrintf("[[\"%s\"]]", node_id().c_str()), profile()));
}

// Tests that calling bookmarkManagerPrivate.cut() to cut a permanent bookmark
// node into the clipboard gracefully fails.
// Regression test for https://crbug.com/1021829.
TEST_F(BookmarkManagerPrivateApiUnitTest, RunCutOnPermanentNode) {
  auto cut_function = base::MakeRefCounted<BookmarkManagerPrivateCutFunction>();
  std::string node_id =
      base::NumberToString(model()->bookmark_bar_node()->id());
  EXPECT_EQ("Can't modify the root bookmark folders.",
            api_test_utils::RunFunctionAndReturnError(
                cut_function.get(),
                base::StringPrintf("[[\"%s\"]]", node_id.c_str()), profile()));
}

}  // namespace extensions
