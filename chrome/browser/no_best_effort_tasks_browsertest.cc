// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/unpacked_installer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#endif

namespace {

class RunLoopUntilLoadedAndPainted : public content::WebContentsObserver {
 public:
  explicit RunLoopUntilLoadedAndPainted(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  ~RunLoopUntilLoadedAndPainted() override = default;

  // Runs a RunLoop on the main thread until the first non-empty frame is
  // painted and the load is complete for the WebContents provided to the
  // constructor.
  void Run() {
    if (LoadedAndPainted())
      return;

    run_loop_.Run();
  }

 private:
  bool LoadedAndPainted() {
    return web_contents()->CompletedFirstVisuallyNonEmptyPaint() &&
           !web_contents()->IsLoading();
  }

  // content::WebContentsObserver:
  void DidFirstVisuallyNonEmptyPaint() override {
    if (LoadedAndPainted())
      run_loop_.Quit();
  }
  void DidStopLoading() override {
    if (LoadedAndPainted())
      run_loop_.Quit();
  }

  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(RunLoopUntilLoadedAndPainted);
};

class NoBestEffortTasksTest : public InProcessBrowserTest {
 protected:
  NoBestEffortTasksTest() = default;
  ~NoBestEffortTasksTest() override = default;

 private:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kDisableBestEffortTasks);
    InProcessBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    // Redirect all DNS requests back to localhost (to the embedded test
    // server).
    host_resolver()->AddRule("*", "127.0.0.1");
    InProcessBrowserTest::SetUpOnMainThread();
  }

  DISALLOW_COPY_AND_ASSIGN(NoBestEffortTasksTest);
};

#if BUILDFLAG(ENABLE_EXTENSIONS)
constexpr base::StringPiece kExtensionId = "ddchlicdkolnonkihahngkmmmjnjlkkf";
constexpr base::TimeDelta kSendMessageRetryPeriod =
    base::TimeDelta::FromMilliseconds(250);
#endif

}  // namespace

// Verify that it is possible to load and paint the initial about:blank page
// without running BEST_EFFORT tasks.
IN_PROC_BROWSER_TEST_F(NoBestEffortTasksTest, LoadAndPaintAboutBlank) {
  content::WebContents* const web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(web_contents->GetLastCommittedURL().IsAboutBlank());

  RunLoopUntilLoadedAndPainted run_until_loaded_and_painted(web_contents);
  run_until_loaded_and_painted.Run();
}

// Verify that it is possible to load and paint a page from the network without
// running BEST_EFFORT tasks.
//
// This test has more dependencies than LoadAndPaintAboutBlank, including
// loading cookies.
IN_PROC_BROWSER_TEST_F(NoBestEffortTasksTest, LoadAndPaintFromNetwork) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::OpenURLParams open(
      embedded_test_server()->GetURL("a.com", "/empty.html"),
      content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_TYPED, false);
  content::WebContents* const web_contents = browser()->OpenURL(open);
  EXPECT_TRUE(web_contents->IsLoading());

  RunLoopUntilLoadedAndPainted run_until_loaded_and_painted(web_contents);
  run_until_loaded_and_painted.Run();
}

// Verify that it is possible to load and paint a file:// URL without running
// BEST_EFFORT tasks. Regression test for https://crbug.com/973244.
IN_PROC_BROWSER_TEST_F(NoBestEffortTasksTest, LoadAndPaintFileScheme) {
  constexpr base::FilePath::CharType kFile[] = FILE_PATH_LITERAL("links.html");
  GURL file_url(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kFile)));
  ASSERT_TRUE(file_url.SchemeIs(url::kFileScheme));

  content::OpenURLParams open(file_url, content::Referrer(),
                              WindowOpenDisposition::NEW_FOREGROUND_TAB,
                              ui::PAGE_TRANSITION_TYPED, false);
  content::WebContents* const web_contents = browser()->OpenURL(open);
  EXPECT_TRUE(web_contents->IsLoading());

  RunLoopUntilLoadedAndPainted run_until_loaded_and_painted(web_contents);
  run_until_loaded_and_painted.Run();
}

// Verify that an extension can be loaded and perform basic messaging without
// running BEST_EFFORT tasks. Regression test for http://crbug.com/177163#c112.
//
// NOTE: If this test times out, it might help to look at how
// http://crbug.com/924416 was resolved.
#if BUILDFLAG(ENABLE_EXTENSIONS)
IN_PROC_BROWSER_TEST_F(NoBestEffortTasksTest, LoadExtensionAndSendMessages) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load the extension, waiting until the ExtensionRegistry reports that its
  // renderer has been started.
  base::FilePath extension_dir;
  const bool have_test_data_dir =
      base::PathService::Get(chrome::DIR_TEST_DATA, &extension_dir);
  ASSERT_TRUE(have_test_data_dir);
  extension_dir = extension_dir.AppendASCII("extensions")
                      .AppendASCII("no_best_effort_tasks_test_extension");
  extensions::UnpackedInstaller::Create(
      extensions::ExtensionSystem::Get(browser()->profile())
          ->extension_service())
      ->Load(extension_dir);
  auto* const extension =
      extensions::TestExtensionRegistryObserver(
          extensions::ExtensionRegistry::Get(browser()->profile()))
          .WaitForExtensionReady();
  ASSERT_TRUE(extension);
  ASSERT_EQ(kExtensionId, extension->id());

  // Navigate to a test page, waiting until complete. Note that the hostname
  // here must match the pattern found in the extension's manifest file, or it
  // will not be able to send/receive messaging from the test web page (due to
  // extension permissions).
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("fake.chromium.org", "/empty.html"));

  // Execute JavaScript in the test page, to send a ping message to the
  // extension and await the reply. The chrome.runtime.sendMessage() operation
  // can fail if the extension's background page hasn't finished running yet
  // (i.e., there is no message listener yet). Thus, use a retry loop.
  const std::string request_reply_javascript = base::StringPrintf(
      "new Promise((resolve, reject) => {\n"
      "  chrome.runtime.sendMessage(\n"
      "      '%s',\n"
      "      {ping: true},\n"
      "      response => {\n"
      "        if (response) {\n"
      "          resolve(response);\n"
      "        } else {\n"
      "          reject(chrome.runtime.lastError.message);\n"
      "        }\n"
      "      });\n"
      "})",
      extension->id().c_str());
  for (;;) {
    const auto result =
        content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                        request_reply_javascript);
    if (result.error.empty()) {
      LOG(INFO) << "Got a response from the extension.";
      EXPECT_TRUE(result.value.FindBoolKey("pong").value_or(false));
      break;
    }
    // An error indicates the extension's message listener isn't up yet. Wait a
    // little before trying again.
    LOG(INFO) << "Waiting for the extension's message listener...";
    base::RunLoop run_loop;
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), kSendMessageRetryPeriod);
    run_loop.Run();
  }
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

// Verify that Blob XMLHttpRequest finishes without running BEST_EFFORT tasks.
// Regression test for https://crbug.com/989868.
IN_PROC_BROWSER_TEST_F(NoBestEffortTasksTest, BlobXMLHttpRequest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/empty.html"));
  const char kScript[] = R"(
      new Promise(function (resolve, reject) {
        const xhr = new XMLHttpRequest();
        xhr.open("GET", "./empty.html?", true);
        xhr.responseType = "blob";
        xhr.onload = () => {
          resolve('DONE');
        };
        xhr.send();
      })
  )";
  EXPECT_EQ("DONE",
            content::EvalJs(
                browser()->tab_strip_model()->GetActiveWebContents(), kScript));
}

// A test specialization for verifying quota storage related operations do not
// use BEST_EFFORT tasks.
class NoBestEffortTasksTestWithQuota : public NoBestEffortTasksTest {
 protected:
  std::unique_ptr<storage::QuotaSettings> CreateQuotaSettings() override {
    // Return nullptr to use the real quota subsystem.
    return nullptr;
  }
};

// Verify that cache_storage finishes without running BEST_EFFORT tasks.
// Regression test for https://crbug.com/1006546.
IN_PROC_BROWSER_TEST_F(NoBestEffortTasksTestWithQuota, CacheStorage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/empty.html"));
  const char kScript[] = R"(
      (async function() {
        const name = 'foo';
        const url = '/';
        const body = 'hello world';
        let c = await caches.open(name);
        await c.put(url, new Response(body));
        let r = await c.match(url);
        await r.text();
        return 'DONE';
      })();
  )";
  EXPECT_EQ("DONE",
            content::EvalJs(
                browser()->tab_strip_model()->GetActiveWebContents(), kScript));
}

// Verify that quota estimate() finishes without running BEST_EFFORT tasks.
// Regression test for https://crbug.com/1006546.
IN_PROC_BROWSER_TEST_F(NoBestEffortTasksTestWithQuota, QuotaEstimate) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/empty.html"));
  const char kScript[] = R"(
      (async function() {
        await navigator.storage.estimate();
        return 'DONE';
      })();
  )";
  EXPECT_EQ("DONE",
            content::EvalJs(
                browser()->tab_strip_model()->GetActiveWebContents(), kScript));
}
