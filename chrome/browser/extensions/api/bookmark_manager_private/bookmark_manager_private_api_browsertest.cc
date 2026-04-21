// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bookmark_manager_private/bookmark_manager_private_api.h"

#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/extensions/api/bookmarks/bookmarks_api.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api_test_utils.h"
#include "ui/shell_dialogs/select_file_policy.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace extensions {

class BookmarkManagerPrivateApiBrowsertest : public InProcessBrowserTest {
 public:
  void SetUp() override { InProcessBrowserTest::SetUp(); }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    model_ = WaitForBookmarkModel();
  }

  void TearDownOnMainThread() override {
    model_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

  BookmarkModel* model() { return model_; }

 private:
  BookmarkModel* WaitForBookmarkModel() {
    BookmarkModel* model =
        BookmarkModelFactory::GetForBrowserContext(GetProfile());
    bookmarks::test::WaitForBookmarkModelToLoad(model);
    return model;
  }

  raw_ptr<BookmarkModel> model_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(BookmarkManagerPrivateApiBrowsertest,
                       OpenURLInNewWindow) {
  const BookmarkNode* node =
      model()->AddURL(model()->bookmark_bar_node(), 0, u"Settings",
                      GURL(chrome::kChromeUISettingsURL));
  std::string node_id = base::NumberToString(node->id());

  auto new_window_function =
      base::MakeRefCounted<BookmarkManagerPrivateOpenInNewWindowFunction>();
  std::string args = base::StringPrintf(R"([["%s"], false])", node_id.c_str());
  ASSERT_TRUE(api_test_utils::RunFunction(new_window_function.get(), args,
                                          GetProfile()));
}

IN_PROC_BROWSER_TEST_F(BookmarkManagerPrivateApiBrowsertest,
                       OpenURLInNewWindowIncognito) {
  const BookmarkNode* node =
      model()->AddURL(model()->bookmark_bar_node(), 0, u"Settings",
                      GURL(chrome::kChromeUIVersionURL));
  std::string node_id = base::NumberToString(node->id());

  auto new_window_function =
      base::MakeRefCounted<BookmarkManagerPrivateOpenInNewWindowFunction>();
  std::string args = base::StringPrintf(R"([["%s"], true])", node_id.c_str());
  ASSERT_TRUE(api_test_utils::RunFunction(new_window_function.get(), args,
                                          GetProfile()));
}

IN_PROC_BROWSER_TEST_F(BookmarkManagerPrivateApiBrowsertest,
                       OpenURLInNewTabGroup) {
  const BookmarkNode* node1 =
      model()->AddURL(model()->bookmark_bar_node(), 0, u"Settings",
                      GURL(chrome::kChromeUISettingsURL));
  std::string node_id1 = base::NumberToString(node1->id());

  const BookmarkNode* node2 =
      model()->AddURL(model()->bookmark_bar_node(), 1, u"Version",
                      GURL(chrome::kChromeUIVersionURL));
  std::string node_id2 = base::NumberToString(node2->id());

  auto new_tab_group_function =
      base::MakeRefCounted<BookmarkManagerPrivateOpenInNewTabGroupFunction>();
  std::string args = base::StringPrintf(R"([["%s","%s"]])", node_id1.c_str(),
                                        node_id2.c_str());
  ASSERT_TRUE(api_test_utils::RunFunction(new_tab_group_function.get(), args,
                                          GetProfile()));

  // Verify the tab group and the tabs are created.
  ASSERT_EQ(
      1u, browser()->tab_strip_model()->group_model()->ListTabGroups().size());
  ASSERT_EQ(3, browser()->tab_strip_model()->count());
  ASSERT_EQ(GURL(chrome::kChromeUISettingsURL),
            browser()->tab_strip_model()->GetWebContentsAt(1)->GetURL());
  ASSERT_EQ(GURL(chrome::kChromeUIVersionURL),
            browser()->tab_strip_model()->GetWebContentsAt(2)->GetURL());
}

// Tests that running ExtensionFunction-s on deleted bookmark node gracefully
// fails.
// Regression test for https://crbug.com/41328754.
IN_PROC_BROWSER_TEST_F(BookmarkManagerPrivateApiBrowsertest, RunOnDeletedNode) {
  GURL url("https://www.google.com");
  const BookmarkNode* node =
      model()->AddURL(model()->other_node(), 0, u"Goog", url);
  std::string node_id = base::NumberToString(node->id());

  // Remove our only bookmark node.
  auto remove_function = base::MakeRefCounted<BookmarksRemoveFunction>();
  ASSERT_TRUE(api_test_utils::RunFunction(
      remove_function.get(), base::StringPrintf("[\"%s\"]", node_id.c_str()),
      GetProfile()));

  // Call bookmarkManagerPrivate.copy() with the removed bookmark node's id.
  auto copy_function =
      base::MakeRefCounted<BookmarkManagerPrivateCopyFunction>();
  EXPECT_EQ(
      base::StringPrintf("Could not find bookmark nodes with given ids: [%s]",
                         node_id.c_str()),
      api_test_utils::RunFunctionAndReturnError(
          copy_function.get(),
          base::StringPrintf("[[\"%s\"]]", node_id.c_str()), GetProfile()));
}

// Tests that calling bookmarkManagerPrivate.cut() to cut a permanent bookmark
// node into the clipboard gracefully fails.
// Regression test for https://crbug.com/40657280.
IN_PROC_BROWSER_TEST_F(BookmarkManagerPrivateApiBrowsertest,
                       RunCutOnPermanentNode) {
  auto cut_function = base::MakeRefCounted<BookmarkManagerPrivateCutFunction>();
  std::string node_id =
      base::NumberToString(model()->bookmark_bar_node()->id());
  EXPECT_EQ(
      "Can't modify the root bookmark folders.",
      api_test_utils::RunFunctionAndReturnError(
          cut_function.get(), base::StringPrintf("[[\"%s\"]]", node_id.c_str()),
          GetProfile()));
}

IN_PROC_BROWSER_TEST_F(BookmarkManagerPrivateApiBrowsertest,
                       RunOpenInNewTabFunction) {
  // Browser starts with one tab.
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  GURL url("https://www.google.com");
  const BookmarkNode* node =
      model()->AddURL(model()->other_node(), 0, u"Goog", url);
  std::string node_id = base::NumberToString(node->id());

  auto new_tab_function =
      base::MakeRefCounted<BookmarkManagerPrivateOpenInNewTabFunction>();
  std::string args = base::StringPrintf(R"(["%s"])", node_id.c_str());
  ASSERT_TRUE(
      api_test_utils::RunFunction(new_tab_function.get(), args, GetProfile()));

  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_EQ(url, browser()->tab_strip_model()->GetWebContentsAt(1)->GetURL());
}

IN_PROC_BROWSER_TEST_F(BookmarkManagerPrivateApiBrowsertest,
                       RunOpenInNewTabFunctionFolder) {
  auto new_tab_function =
      base::MakeRefCounted<BookmarkManagerPrivateOpenInNewTabFunction>();
  std::string node_id =
      base::NumberToString(model()->bookmark_bar_node()->id());
  std::string args = base::StringPrintf(R"(["%s"])", node_id.c_str());
  EXPECT_EQ("Cannot open a folder in a new tab.",
            api_test_utils::RunFunctionAndReturnError(new_tab_function.get(),
                                                      args, GetProfile()));
}

IN_PROC_BROWSER_TEST_F(BookmarkManagerPrivateApiBrowsertest,
                       RunOpenInNewWindowFunctionFolder) {
  auto new_window_function =
      base::MakeRefCounted<BookmarkManagerPrivateOpenInNewWindowFunction>();
  std::string node_id =
      base::NumberToString(model()->bookmark_bar_node()->id());
  std::string args = base::StringPrintf(R"([["%s"], false])", node_id.c_str());
  EXPECT_EQ("Cannot open a folder in a new window.",
            api_test_utils::RunFunctionAndReturnError(new_window_function.get(),
                                                      args, GetProfile()));
}

IN_PROC_BROWSER_TEST_F(BookmarkManagerPrivateApiBrowsertest,
                       RunOpenInNewWindowFunctionIncognitoDisabled) {
  // Incognito disabled.
  IncognitoModePrefs::SetAvailability(
      GetProfile()->GetPrefs(), policy::IncognitoModeAvailability::kDisabled);

  auto new_window_function =
      base::MakeRefCounted<BookmarkManagerPrivateOpenInNewWindowFunction>();
  GURL url("https://www.google.com");
  const BookmarkNode* node =
      model()->AddURL(model()->other_node(), 0, u"Goog", url);
  std::string node_id = base::NumberToString(node->id());

  std::string args = base::StringPrintf(R"([["%s"], true])", node_id.c_str());
  EXPECT_EQ("Incognito mode is disabled.",
            api_test_utils::RunFunctionAndReturnError(new_window_function.get(),
                                                      args, GetProfile()));
}

IN_PROC_BROWSER_TEST_F(BookmarkManagerPrivateApiBrowsertest,
                       RunOpenInNewWindowFunctionIncognitoForced) {
  // Incognito forced.
  IncognitoModePrefs::SetAvailability(
      GetProfile()->GetPrefs(), policy::IncognitoModeAvailability::kForced);

  auto new_window_function =
      base::MakeRefCounted<BookmarkManagerPrivateOpenInNewWindowFunction>();
  GURL url("https://www.google.com");
  const BookmarkNode* node =
      model()->AddURL(model()->other_node(), 0, u"Goog", url);
  std::string node_id = base::NumberToString(node->id());

  std::string args = base::StringPrintf(R"([["%s"], false])", node_id.c_str());
  EXPECT_EQ("Incognito mode is forced. Cannot open normal windows.",
            api_test_utils::RunFunctionAndReturnError(new_window_function.get(),
                                                      args, GetProfile()));
}

IN_PROC_BROWSER_TEST_F(BookmarkManagerPrivateApiBrowsertest,
                       RunOpenInNewWindowFunctionIncognitoIncompatibleNode) {
  const BookmarkNode* node = model()->AddURL(
      model()->other_node(), 0, u"history", GURL("chrome://history"));
  std::string node_id = base::NumberToString(node->id());

  auto new_window_function =
      base::MakeRefCounted<BookmarkManagerPrivateOpenInNewWindowFunction>();
  std::string args = base::StringPrintf(R"([["%s"], true])", node_id.c_str());
  EXPECT_EQ("Cannot open URL \"chrome://history/\" in an incognito window.",
            api_test_utils::RunFunctionAndReturnError(new_window_function.get(),
                                                      args, GetProfile()));
}

// Mock SelectFileDialog to track ListenerDestroyed calls.
class MockSelectFileDialog : public ui::SelectFileDialog {
 public:
  explicit MockSelectFileDialog(Listener* listener)
      : ui::SelectFileDialog(listener,
                             std::unique_ptr<ui::SelectFilePolicy>()) {}

  void ListenerDestroyed() override {
    listener_destroyed_called_ = true;
    listener_ = nullptr;
  }

  bool listener_destroyed_called() const { return listener_destroyed_called_; }

 private:
  ~MockSelectFileDialog() override = default;

  bool IsRunning(gfx::NativeWindow parent_window) const override {
    return false;
  }
  void SelectFileImpl(Type type,
                      const std::u16string& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      const GURL* caller) override {}
  bool HasMultipleFileTypeChoicesImpl() override { return false; }

  bool listener_destroyed_called_ = false;
};

// Testable wrapper that exposes protected members for testing.
class TestableImportFunction : public BookmarkManagerPrivateImportFunction {
 public:
  using BookmarkManagerPrivateImportFunction::CleanupFileDialog;
  using BookmarkManagerPrivateImportFunction::select_file_dialog_;

 protected:
  ~TestableImportFunction() override = default;
};

class BookmarkManagerPrivateIOFunctionTest : public InProcessBrowserTest {
 public:
  void SetupFunctionWithMockDialog(
      scoped_refptr<TestableImportFunction>* function,
      scoped_refptr<MockSelectFileDialog>* dialog) {
    *function = base::MakeRefCounted<TestableImportFunction>();
    *dialog = base::MakeRefCounted<MockSelectFileDialog>(function->get());
    (*function)->AddRef();  // Balance Release() in CleanupFileDialog.
    (*function)->select_file_dialog_ = *dialog;
  }
};

IN_PROC_BROWSER_TEST_F(BookmarkManagerPrivateIOFunctionTest,
                       CleanupFileDialogCallsListenerDestroyed) {
  scoped_refptr<TestableImportFunction> function;
  scoped_refptr<MockSelectFileDialog> dialog;
  SetupFunctionWithMockDialog(&function, &dialog);

  EXPECT_FALSE(dialog->listener_destroyed_called());

  function->CleanupFileDialog();

  EXPECT_TRUE(dialog->listener_destroyed_called());
  EXPECT_FALSE(function->select_file_dialog_);
}

IN_PROC_BROWSER_TEST_F(BookmarkManagerPrivateIOFunctionTest,
                       FileSelectionCanceledCallsListenerDestroyed) {
  scoped_refptr<TestableImportFunction> function;
  scoped_refptr<MockSelectFileDialog> dialog;
  SetupFunctionWithMockDialog(&function, &dialog);

  EXPECT_FALSE(dialog->listener_destroyed_called());

  function->FileSelectionCanceled();

  EXPECT_TRUE(dialog->listener_destroyed_called());
  EXPECT_FALSE(function->select_file_dialog_);
}

}  // namespace extensions
