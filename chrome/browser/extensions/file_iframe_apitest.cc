// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "net/base/filename_util.h"

class FileIFrameAPITest : public extensions::ExtensionBrowserTest {
 public:
  FileIFrameAPITest() {}

  FileIFrameAPITest(const FileIFrameAPITest&) = delete;
  FileIFrameAPITest& operator=(const FileIFrameAPITest&) = delete;

  void set_has_all_urls(bool val) { has_all_urls_ = val; }
  void set_has_file_access(bool val) { has_file_access_ = val; }

  // Loads the extension and determines if the background page was able to load
  // a file iframe.
  void RunTest(bool expect_will_load_file_iframe) {
    WriteManifest();
    WriteBackgroundPage();

    ExtensionTestMessageListener listener;
    ASSERT_TRUE(LoadExtension(extension_dir_.UnpackedPath(),
                              {.allow_file_access = has_file_access_}));
    EXPECT_TRUE(listener.WaitUntilSatisfied());

    EXPECT_TRUE(listener.message() == "allowed" ||
                listener.message() == "denied")
        << "Unexpected message " << listener.message();
    bool allowed = listener.message() == "allowed";
    EXPECT_EQ(expect_will_load_file_iframe, allowed);

    // Sanity check the last committed url on the |file_iframe|.
    extensions::ExtensionHost* background_host =
        extensions::ProcessManager::Get(profile())
            ->GetBackgroundHostForExtension(last_loaded_extension_id());
    ASSERT_TRUE(background_host);
    content::RenderFrameHost* file_iframe = content::FrameMatchingPredicate(
        background_host->host_contents()->GetPrimaryPage(),
        base::BindRepeating(&content::FrameMatchesName, "file_iframe"));
    bool is_file_url = file_iframe->GetLastCommittedURL() == GURL("file:///");
    EXPECT_EQ(expect_will_load_file_iframe, is_file_url)
        << "Unexpected committed url: "
        << file_iframe->GetLastCommittedURL().spec();
  }

 private:
  void WriteManifest() {
    constexpr char manifest[] = R"(
      {
         "name": "Test extension",
         "version": "1",
         "manifest_version": 2,
         "permissions" : [%s],
         "background" : {
            "page" : "background.html"
         }
      }
    )";
    extension_dir_.WriteManifest(
        base::StringPrintf(manifest, has_all_urls_ ? "\"<all_urls>\"" : ""));
  }

  void WriteBackgroundPage() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    CHECK(base::CopyDirectory(test_data_dir_.AppendASCII("file_iframe"),
                              extension_dir_.UnpackedPath(),
                              false /*recursive*/));
  }

  bool has_all_urls_ = false;
  bool has_file_access_ = false;
  extensions::TestExtensionDir extension_dir_;
};

// Tests that an extension frame can embed a file iframe if it has file access
// and "<all_urls>" host permissions.
IN_PROC_BROWSER_TEST_F(FileIFrameAPITest, FileAccessAllURLs) {
  set_has_all_urls(true);
  set_has_file_access(true);
  RunTest(true);
}

// Tests that an extension frame can't embed a file iframe if it has no file
// access even with the "<all_urls>" host permissions.
IN_PROC_BROWSER_TEST_F(FileIFrameAPITest, NoFileAccessAllURLs) {
  set_has_all_urls(true);
  set_has_file_access(false);
  RunTest(false);
}

// Tests that an extension frame can't embed a file iframe if it does not have
// host permissions to the file scheme even though it has file access.
IN_PROC_BROWSER_TEST_F(FileIFrameAPITest, FileAccessNoHosts) {
  set_has_all_urls(false);
  set_has_file_access(true);
  RunTest(false);
}
