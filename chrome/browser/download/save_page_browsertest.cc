// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/repeating_test_future.h"
#include "base/test/test_file_util.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_history.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/save_package_file_picker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/download/public/common/download_item.h"
#include "components/history/core/browser/download_constants.h"
#include "components/history/core/browser/download_row.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/security_state/core/security_state.h"
#include "components/services/quarantine/test_support.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/fake_select_file_dialog.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/dbus/dlp/dlp_client.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

using content::BrowserContext;
using content::BrowserThread;
using content::DownloadManager;
using content::RenderFrameHost;
using content::RenderProcessHost;
using content::WebContents;
using download::DownloadItem;
using testing::_;
using testing::ContainsRegex;
using testing::HasSubstr;
using ui::FakeSelectFileDialog;

namespace {

// Returns file contents with each continuous run of whitespace replaced by a
// single space.
std::string ReadFileAndCollapseWhitespace(const base::FilePath& file_path) {
  std::string file_contents;
  if (!base::ReadFileToString(file_path, &file_contents)) {
    ADD_FAILURE() << "Failed to read \"" << file_path.value() << "\" file.";
    return std::string();
  }

  return base::CollapseWhitespaceASCII(file_contents, false);
}

// Takes a string with "url=(%04d)%s", and replaces that with the length and
// contents of the path the response was saved from, |url|, to match output by
// the SavePageAs logic.
std::string WriteSavedFromPath(const std::string& file_contents,
                               const GURL& url) {
  return base::StringPrintfNonConstexpr(
      file_contents.c_str(), url.spec().length(), url.spec().c_str());
}

// Waits for an item record in the downloads database to match |filter|. See
// DownloadStoredProperly() below for an example filter.
class DownloadPersistedObserver : public DownloadHistory::Observer {
 public:
  typedef base::RepeatingCallback<bool(DownloadItem* item,
                                       const history::DownloadRow&)>
      PersistedFilter;

  DownloadPersistedObserver(Profile* profile, const PersistedFilter& filter)
      : profile_(profile),
        filter_(filter),
        persisted_(false) {
    DownloadCoreServiceFactory::GetForBrowserContext(profile_)
        ->GetDownloadHistory()
        ->AddObserver(this);
  }

  DownloadPersistedObserver(const DownloadPersistedObserver&) = delete;
  DownloadPersistedObserver& operator=(const DownloadPersistedObserver&) =
      delete;

  ~DownloadPersistedObserver() override {
    DownloadCoreService* service =
        DownloadCoreServiceFactory::GetForBrowserContext(profile_);
    if (service && service->GetDownloadHistory())
      service->GetDownloadHistory()->RemoveObserver(this);
  }

  bool WaitForPersisted() {
    if (persisted_)
      return true;
    base::RunLoop run_loop;
    quit_waiting_callback_ = run_loop.QuitClosure();
    run_loop.Run();
    return persisted_;
  }

  void OnDownloadStored(DownloadItem* item,
                        const history::DownloadRow& info) override {
    persisted_ = persisted_ || filter_.Run(item, info);
    if (persisted_ && quit_waiting_callback_)
      std::move(quit_waiting_callback_).Run();
  }

 private:
  raw_ptr<Profile> profile_;
  PersistedFilter filter_;
  base::OnceClosure quit_waiting_callback_;
  bool persisted_;
};

// Waits for an item record to be removed from the downloads database.
class DownloadRemovedObserver : public DownloadPersistedObserver {
 public:
  DownloadRemovedObserver(Profile* profile, int32_t download_id)
      : DownloadPersistedObserver(profile, PersistedFilter()),
        removed_(false),
        download_id_(download_id) {}

  DownloadRemovedObserver(const DownloadRemovedObserver&) = delete;
  DownloadRemovedObserver& operator=(const DownloadRemovedObserver&) = delete;

  ~DownloadRemovedObserver() override {}

  bool WaitForRemoved() {
    if (removed_)
      return true;
    base::RunLoop run_loop;
    quit_waiting_callback_ = run_loop.QuitClosure();
    run_loop.Run();
    return removed_;
  }

  void OnDownloadStored(DownloadItem* item,
                        const history::DownloadRow& info) override {}

  void OnDownloadsRemoved(const DownloadHistory::IdSet& ids) override {
    removed_ = ids.find(download_id_) != ids.end();
    if (removed_ && quit_waiting_callback_)
      std::move(quit_waiting_callback_).Run();
  }

 private:
  bool removed_;
  base::OnceClosure quit_waiting_callback_;
  int32_t download_id_;
};

bool DownloadStoredProperly(const GURL& expected_url,
                            const base::FilePath& expected_path,
                            int64_t num_files,
                            history::DownloadState expected_state,
                            DownloadItem* item,
                            const history::DownloadRow& info) {
  // This function may be called multiple times for a given test. Returning
  // false doesn't necessarily mean that the test has failed or will fail, it
  // might just mean that the test hasn't passed yet.
  if (!expected_path.empty() && info.target_path != expected_path) {
    DVLOG(20) << __FUNCTION__ << " " << info.target_path.value()
              << " != " << expected_path.value();
    return false;
  }
  if (info.url_chain.size() != 1u) {
    DVLOG(20) << __FUNCTION__ << " " << info.url_chain.size() << " != 1";
    return false;
  }
  if (info.url_chain[0] != expected_url) {
    DVLOG(20) << __FUNCTION__ << " " << info.url_chain[0].spec()
              << " != " << expected_url.spec();
    return false;
  }
  if ((num_files >= 0) && (info.received_bytes != num_files)) {
    DVLOG(20) << __FUNCTION__ << " " << num_files
              << " != " << info.received_bytes;
    return false;
  }
  if (info.state != expected_state) {
    DVLOG(20) << __FUNCTION__ << " " << info.state << " != " << expected_state;
    return false;
  }
  return true;
}

static const char kAppendedExtension[] = ".html";

// Loosely based on logic in DownloadTestObserver.
class DownloadItemCreatedObserver : public DownloadManager::Observer {
 public:
  explicit DownloadItemCreatedObserver(DownloadManager* manager)
      : manager_(manager) {
    manager->AddObserver(this);
  }

  DownloadItemCreatedObserver(const DownloadItemCreatedObserver&) = delete;
  DownloadItemCreatedObserver& operator=(const DownloadItemCreatedObserver&) =
      delete;

  ~DownloadItemCreatedObserver() override {
    if (manager_)
      manager_->RemoveObserver(this);
  }

  // Wait for the first download item created after object creation.
  // Note that this class provides no protection against the download
  // being destroyed between creation and return of WaitForNewDownloadItem();
  // the caller must guarantee that in some other fashion.
  void WaitForDownloadItem(
      std::vector<raw_ptr<DownloadItem, VectorExperimental>>* items_seen) {
    if (!manager_) {
      // The manager went away before we were asked to wait; return
      // what we have, even if it's null.
      *items_seen = items_seen_;
      return;
    }

    if (items_seen_.empty()) {
      base::RunLoop run_loop;
      quit_waiting_callback_ = run_loop.QuitClosure();
      run_loop.Run();
    }

    *items_seen = items_seen_;
    return;
  }

 private:
  // DownloadManager::Observer
  void OnDownloadCreated(DownloadManager* manager,
                         DownloadItem* item) override {
    DCHECK_EQ(manager, manager_);
    items_seen_.push_back(item);

    if (quit_waiting_callback_)
      std::move(quit_waiting_callback_).Run();
  }

  void ManagerGoingDown(DownloadManager* manager) override {
    manager_->RemoveObserver(this);
    manager_ = nullptr;
    if (quit_waiting_callback_)
      std::move(quit_waiting_callback_).Run();
  }

  base::OnceClosure quit_waiting_callback_;
  raw_ptr<DownloadManager> manager_;
  std::vector<raw_ptr<DownloadItem, VectorExperimental>> items_seen_;
};

class SavePageBrowserTest : public InProcessBrowserTest {
 public:
  SavePageBrowserTest() {}

  SavePageBrowserTest(const SavePageBrowserTest&) = delete;
  SavePageBrowserTest& operator=(const SavePageBrowserTest&) = delete;

 protected:
  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    content::SetupCrossSiteRedirector(embedded_test_server());
    embedded_test_server()->StartAcceptingConnections();

    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_dir_));
    InProcessBrowserTest::SetUp();
  }

  GURL NavigateToMockURL(const std::string& prefix) {
    GURL url = embedded_test_server()->GetURL("/save_page/" + prefix + ".htm");
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    return url;
  }

  // Returns full paths of destination file and directory.
  void GetDestinationPaths(const std::string& prefix,
                           base::FilePath* full_file_name,
                           base::FilePath* dir,
                           content::SavePageType save_page_type =
                               content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML) {
    std::string extension =
        (save_page_type == content::SAVE_PAGE_TYPE_AS_MHTML) ? ".mht" : ".htm";
    *full_file_name = GetSaveDir().AppendASCII(prefix + extension);
    *dir = GetSaveDir().AppendASCII(prefix + "_files");
  }

  WebContents* GetCurrentTab(Browser* browser) const {
    WebContents* current_tab =
        browser->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(current_tab);
    return current_tab;
  }

  // Returns true if and when there was a single download created, and its url
  // is |expected_url|.
  bool VerifySavePackageExpectations(
      Browser* browser,
      const GURL& expected_url) const {
    // Generally, there should only be one download item created
    // in all of these tests.  If it's already here, grab it; if not,
    // wait for it to show up.
    std::vector<raw_ptr<DownloadItem, VectorExperimental>> items;
    DownloadManager* manager = browser->profile()->GetDownloadManager();
    manager->GetAllDownloads(&items);
    if (items.empty())
      DownloadItemCreatedObserver(manager).WaitForDownloadItem(&items);

    EXPECT_EQ(1u, items.size());
    if (1u != items.size())
      return false;
    DownloadItem* download_item(items[0]);

    return (expected_url == download_item->GetOriginalUrl());
  }

  void SaveCurrentTab(const GURL& url,
                      content::SavePageType save_page_type,
                      const std::string& prefix_for_output_files,
                      int expected_number_of_files,
                      base::FilePath* output_dir,
                      base::FilePath* main_file_name) {
    GetDestinationPaths(prefix_for_output_files, main_file_name, output_dir,
                        save_page_type);
    DownloadPersistedObserver persisted(
        browser()->profile(),
        base::BindRepeating(&DownloadStoredProperly, url, *main_file_name,
                            expected_number_of_files,
                            history::DownloadState::COMPLETE));
    base::RunLoop run_loop;
    content::SavePackageFinishedObserver observer(
        browser()->profile()->GetDownloadManager(), run_loop.QuitClosure());
    ASSERT_TRUE(GetCurrentTab(browser())
                    ->SavePage(*main_file_name, *output_dir, save_page_type));

    run_loop.Run();
    ASSERT_TRUE(VerifySavePackageExpectations(browser(), url));
    persisted.WaitForPersisted();
  }

  // Note on synchronization:
  //
  // For each Save Page As operation, we create a corresponding shell
  // DownloadItem to display progress to the user.  That DownloadItem goes
  // through its own state transitions, including being persisted out to the
  // history database, and the download shelf is not shown until after the
  // persistence occurs.  Save Package completion (and marking the DownloadItem
  // as completed) occurs asynchronously from persistence.  Thus if we want to
  // examine either UI state or DB state, we need to wait until both the save
  // package operation is complete and the relevant download item has been
  // persisted.

  DownloadManager* GetDownloadManager() const {
    DownloadManager* download_manager =
        browser()->profile()->GetDownloadManager();
    EXPECT_TRUE(download_manager);
    return download_manager;
  }

  // Returns full path to a file in chrome/test/data/save_page directory.
  base::FilePath GetTestDirFile(const std::string& file_name) {
    const base::FilePath::CharType kTestDir[] = FILE_PATH_LITERAL("save_page");
    return test_dir_.Append(base::FilePath(kTestDir)).AppendASCII(file_name);
  }

  base::FilePath GetSaveDir() {
    return DownloadPrefs(browser()->profile()).DownloadPath();
  }

  // Path to directory containing test data.
  base::FilePath test_dir_;
};

IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, SaveHTMLOnly) {
  GURL url = NavigateToMockURL("a");

  base::FilePath full_file_name, dir;
  SaveCurrentTab(url, content::SAVE_PAGE_TYPE_AS_ONLY_HTML, "a", 1, &dir,
                 &full_file_name);
  ASSERT_FALSE(HasFailure());

  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::PathExists(full_file_name));
  EXPECT_FALSE(base::PathExists(dir));
  EXPECT_TRUE(base::ContentsEqual(GetTestDirFile("a.htm"), full_file_name));
}

IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, SaveFileURL) {
  GURL url = net::FilePathToFileURL(GetTestDirFile("text.txt"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  base::FilePath full_file_name, dir;
  SaveCurrentTab(url, content::SAVE_PAGE_TYPE_AS_ONLY_HTML, "test", 1, &dir,
                 &full_file_name);
  ASSERT_FALSE(HasFailure());

  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::PathExists(full_file_name));
  EXPECT_FALSE(base::PathExists(dir));
  EXPECT_TRUE(base::ContentsEqual(GetTestDirFile("text.txt"), full_file_name));
#if BUILDFLAG(IS_WIN)
  // Local file URL will not be quarantined.
  EXPECT_FALSE(quarantine::IsFileQuarantined(full_file_name, GURL(), GURL()));
#endif
}

IN_PROC_BROWSER_TEST_F(SavePageBrowserTest,
                       SaveHTMLOnly_CrossOriginReadPolicy) {
  GURL url = embedded_test_server()->GetURL(
      "/downloads/cross-origin-resource-policy-resource.txt");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  base::FilePath full_file_name, dir;
  SaveCurrentTab(url, content::SAVE_PAGE_TYPE_AS_ONLY_HTML, "a", 1, &dir,
                 &full_file_name);
  ASSERT_FALSE(HasFailure());

  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::PathExists(full_file_name));
  EXPECT_FALSE(base::PathExists(dir));

  const base::FilePath::CharType kTestDir[] = FILE_PATH_LITERAL("downloads");
  const base::FilePath kTestFile =
      test_dir_.Append(base::FilePath(kTestDir))
          .AppendASCII("cross-origin-resource-policy-resource.txt");
  EXPECT_TRUE(base::ContentsEqual(kTestFile, full_file_name));
}

// TODO(crbug.com/40805571): Flaky on mac arm64.
#if BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64)
#define MAYBE_SaveHTMLOnlyCancel DISABLED_SaveHTMLOnlyCancel
#else
#define MAYBE_SaveHTMLOnlyCancel SaveHTMLOnlyCancel
#endif
IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, MAYBE_SaveHTMLOnlyCancel) {
  GURL url = NavigateToMockURL("a");
  DownloadManager* manager = GetDownloadManager();
  std::vector<raw_ptr<DownloadItem, VectorExperimental>> downloads;
  manager->GetAllDownloads(&downloads);
  ASSERT_EQ(0u, downloads.size());

  base::FilePath full_file_name, dir;
  GetDestinationPaths("a", &full_file_name, &dir);
  DownloadItemCreatedObserver creation_observer(manager);
  DownloadPersistedObserver persisted(
      browser()->profile(),
      base::BindRepeating(&DownloadStoredProperly, url, full_file_name, -1,
                          history::DownloadState::CANCELLED));
  // -1 to disable number of files check; we don't update after cancel, and
  // we don't know when the single file completed in relationship to
  // the cancel.

  ASSERT_TRUE(GetCurrentTab(browser())->SavePage(full_file_name, dir,
                                        content::SAVE_PAGE_TYPE_AS_ONLY_HTML));
  std::vector<raw_ptr<DownloadItem, VectorExperimental>> items;
  creation_observer.WaitForDownloadItem(&items);
  ASSERT_EQ(1UL, items.size());
  ASSERT_EQ(url.spec(), items[0]->GetOriginalUrl().spec());
  items[0]->Cancel(true);
  // TODO(rdsmith): Fix DII::Cancel() to actually cancel the save package.
  // Currently it's ignored.

  persisted.WaitForPersisted();

  // TODO(benjhayden): Figure out how to safely wait for SavePackage's finished
  // notification, then expect the contents of the downloaded file.
}

// Test that saving an HTML file with long (i.e. > 65536 bytes) text content
// does not crash the browser despite the renderer requiring more than one
// "pass" to serialize the HTML content (see crash from crbug.com/1085721).
IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, SaveHTMLWithLongTextContent) {
  GURL url =
      embedded_test_server()->GetURL("/save_page/long-text-content.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  base::FilePath full_file_name, dir;
  SaveCurrentTab(url, content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML,
                 "long-text-content", 1, &dir, &full_file_name);

  ASSERT_FALSE(HasFailure());

  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::PathExists(full_file_name));
  EXPECT_FALSE(base::PathExists(dir));

  // Besides checking that the renderer didn't crash, test also that the HTML
  // content saved is the expected one (i.e. the whole HTML, no truncation).
  EXPECT_EQ(ReadFileAndCollapseWhitespace(full_file_name),
            WriteSavedFromPath(ReadFileAndCollapseWhitespace(GetTestDirFile(
                                   "long-text-content.saved.html")),
                               url));
}

class DelayingDownloadManagerDelegate : public ChromeDownloadManagerDelegate {
 public:
  explicit DelayingDownloadManagerDelegate(Profile* profile)
    : ChromeDownloadManagerDelegate(profile) {
  }

  DelayingDownloadManagerDelegate(const DelayingDownloadManagerDelegate&) =
      delete;
  DelayingDownloadManagerDelegate& operator=(
      const DelayingDownloadManagerDelegate&) = delete;

  ~DelayingDownloadManagerDelegate() override {}

  bool ShouldCompleteDownload(
      download::DownloadItem* item,
      base::OnceClosure user_complete_callback) override {
    return false;
  }
};

// Disabled on multiple platforms due to flakiness. crbug.com/580766
IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, DISABLED_SaveHTMLOnlyTabDestroy) {
  GURL url = NavigateToMockURL("a");
  std::unique_ptr<DelayingDownloadManagerDelegate> delaying_delegate(
      new DelayingDownloadManagerDelegate(browser()->profile()));
  delaying_delegate->GetDownloadIdReceiverCallback().Run(
      download::DownloadItem::kInvalidId + 1);
  DownloadCoreServiceFactory::GetForBrowserContext(browser()->profile())
      ->SetDownloadManagerDelegateForTesting(std::move(delaying_delegate));
  DownloadManager* manager = GetDownloadManager();
  std::vector<raw_ptr<DownloadItem, VectorExperimental>> downloads;
  manager->GetAllDownloads(&downloads);
  ASSERT_EQ(0u, downloads.size());

  base::FilePath full_file_name, dir;
  GetDestinationPaths("a", &full_file_name, &dir);
  DownloadItemCreatedObserver creation_observer(manager);
  ASSERT_TRUE(GetCurrentTab(browser())->SavePage(full_file_name, dir,
                                        content::SAVE_PAGE_TYPE_AS_ONLY_HTML));
  std::vector<raw_ptr<DownloadItem, VectorExperimental>> items;
  creation_observer.WaitForDownloadItem(&items);
  ASSERT_EQ(1u, items.size());

  // Close the tab; does this cancel the download?
  GetCurrentTab(browser())->Close();
  EXPECT_EQ(DownloadItem::CANCELLED, items[0]->GetState());

  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_FALSE(base::PathExists(full_file_name));
  EXPECT_FALSE(base::PathExists(dir));
}

IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, SaveViewSourceHTMLOnly) {
  // TODO(lukasza): https://crbug.com/971811: Disallow renderer crashes once the
  // bug is fixed.
  content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;

  GURL mock_url = embedded_test_server()->GetURL("/save_page/a.htm");
  GURL view_source_url =
      GURL(content::kViewSourceScheme + std::string(":") + mock_url.spec());
  GURL actual_page_url = embedded_test_server()->GetURL("/save_page/a.htm");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), view_source_url));

  base::FilePath full_file_name, dir;
  SaveCurrentTab(actual_page_url, content::SAVE_PAGE_TYPE_AS_ONLY_HTML, "a", 1,
                 &dir, &full_file_name);
  ASSERT_FALSE(HasFailure());

  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::PathExists(full_file_name));
  EXPECT_FALSE(base::PathExists(dir));
  EXPECT_TRUE(base::ContentsEqual(GetTestDirFile("a.htm"), full_file_name));
}

// Regression test for https://crbug.com/974312 (saving a page that was served
// with `Cross-Origin-Resource-Policy: same-origin` http response header).
IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, SaveCompleteHTML) {
  GURL url = NavigateToMockURL("b");

  base::FilePath full_file_name, dir;
  SaveCurrentTab(url, content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML, "b", 3, &dir,
                 &full_file_name);
  ASSERT_FALSE(HasFailure());

  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::PathExists(full_file_name));
  EXPECT_TRUE(base::PathExists(dir));

  EXPECT_EQ(
      ReadFileAndCollapseWhitespace(full_file_name),
      WriteSavedFromPath(
          ReadFileAndCollapseWhitespace(GetTestDirFile("b.saved1.htm")), url));
  EXPECT_TRUE(
      base::ContentsEqual(GetTestDirFile("1.png"), dir.AppendASCII("1.png")));
  EXPECT_EQ(ReadFileAndCollapseWhitespace(dir.AppendASCII("1.css")),
            ReadFileAndCollapseWhitespace(GetTestDirFile("1.css")));
}

IN_PROC_BROWSER_TEST_F(SavePageBrowserTest,
                       SaveDuringInitialNavigationIncognito) {
  // Open an Incognito window.
  Browser* incognito = CreateIncognitoBrowser();  // Waits.
  ASSERT_TRUE(incognito);

  // Create a download item creation waiter on that window.
  DownloadItemCreatedObserver creation_observer(
      incognito->profile()->GetDownloadManager());

  // Navigate, unblocking with new tab.
  GURL url = embedded_test_server()->GetURL("/save_page/b.htm");
  NavigateToURLWithDisposition(incognito, url,
                               WindowOpenDisposition::NEW_FOREGROUND_TAB,
                               ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  // Save the page before completion.
  base::FilePath full_file_name, dir;
  GetDestinationPaths("b", &full_file_name, &dir);

  base::RunLoop run_loop;
  content::SavePackageFinishedObserver observer(
      incognito->profile()->GetDownloadManager(), run_loop.QuitClosure());
  ASSERT_TRUE(GetCurrentTab(incognito)->SavePage(
      full_file_name, dir, content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML));

  run_loop.Run();
  ASSERT_TRUE(VerifySavePackageExpectations(incognito, url));

  // We can't check more than this because SavePackage is racing with
  // the page load.  If the page load won the race, then SavePackage
  // might have completed. If the page load lost the race, then
  // SavePackage will cancel because there aren't any resources to
  // save.
}

IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, NoSave) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  EXPECT_FALSE(chrome::CanSavePage(browser()));
}

IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, FileNameFromPageTitle) {
  GURL url = NavigateToMockURL("b");

  base::FilePath full_file_name = GetSaveDir().AppendASCII(
      std::string("Test page for saving page feature") + kAppendedExtension);
  base::FilePath dir =
      GetSaveDir().AppendASCII("Test page for saving page feature_files");
  DownloadPersistedObserver persisted(
      browser()->profile(),
      base::BindRepeating(&DownloadStoredProperly, url, full_file_name, 3,
                          history::DownloadState::COMPLETE));
  base::RunLoop run_loop;
  content::SavePackageFinishedObserver observer(
      browser()->profile()->GetDownloadManager(), run_loop.QuitClosure());
  ASSERT_TRUE(GetCurrentTab(browser())->SavePage(
      full_file_name, dir, content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML));

  run_loop.Run();
  ASSERT_TRUE(VerifySavePackageExpectations(browser(), url));
  persisted.WaitForPersisted();

  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::PathExists(full_file_name));
  EXPECT_TRUE(base::PathExists(dir));

  EXPECT_EQ(
      ReadFileAndCollapseWhitespace(full_file_name),
      WriteSavedFromPath(
          ReadFileAndCollapseWhitespace(GetTestDirFile("b.saved2.htm")), url));
  EXPECT_TRUE(
      base::ContentsEqual(GetTestDirFile("1.png"), dir.AppendASCII("1.png")));
  EXPECT_EQ(ReadFileAndCollapseWhitespace(dir.AppendASCII("1.css")),
            ReadFileAndCollapseWhitespace(GetTestDirFile("1.css")));
}

IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, RemoveFromList) {
  GURL url = NavigateToMockURL("a");

  base::FilePath full_file_name, dir;
  SaveCurrentTab(url, content::SAVE_PAGE_TYPE_AS_ONLY_HTML, "a", 1, &dir,
                 &full_file_name);
  ASSERT_FALSE(HasFailure());

  DownloadManager* manager = GetDownloadManager();
  std::vector<raw_ptr<DownloadItem, VectorExperimental>> downloads;
  manager->GetAllDownloads(&downloads);
  ASSERT_EQ(1UL, downloads.size());

  DownloadRemovedObserver removed(browser()->profile(), downloads[0]->GetId());
  downloads[0]->Remove();
  removed.WaitForRemoved();

  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::PathExists(full_file_name));
  EXPECT_FALSE(base::PathExists(dir));
  EXPECT_TRUE(base::ContentsEqual(GetTestDirFile("a.htm"), full_file_name));
}

// This tests that a webpage with the title "test.exe" is saved as
// "test.exe.htm".
// We probably don't care to handle this on Linux or Mac.
#if BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, CleanFilenameFromPageTitle) {
  base::FilePath download_dir =
      DownloadPrefs::FromDownloadManager(GetDownloadManager())->
          DownloadPath();
  base::FilePath full_file_name =
      download_dir.AppendASCII(std::string("test.exe") + kAppendedExtension);
  base::FilePath dir = download_dir.AppendASCII("test.exe_files");

  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_FALSE(base::PathExists(full_file_name));
  GURL url = embedded_test_server()->GetURL("/save_page/c.htm");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  SavePackageFilePicker::SetShouldPromptUser(false);
  base::RunLoop run_loop;
  content::SavePackageFinishedObserver observer(
      browser()->profile()->GetDownloadManager(), run_loop.QuitClosure());
  chrome::SavePage(browser());
  run_loop.Run();

  EXPECT_TRUE(base::PathExists(full_file_name));

  EXPECT_TRUE(base::DieFileDie(full_file_name, false));
  EXPECT_TRUE(base::DieFileDie(dir, true));
}
#endif

// Tests that the SecurityLevel histograms are logged for save page downloads.
IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, SecurityLevelHistogram) {
  base::HistogramTester histogram_tester;
  GURL url = NavigateToMockURL("a");
  base::FilePath full_file_name, dir;
  SaveCurrentTab(url, content::SAVE_PAGE_TYPE_AS_ONLY_HTML, "a", 1, &dir,
                 &full_file_name);
  histogram_tester.ExpectUniqueSample("Security.SecurityLevel.DownloadStarted",
                                      security_state::NONE, 1);
}

// Tests that a page can be saved as MHTML.
// Flaky on Windows, crbug.com/1048100
#if BUILDFLAG(IS_WIN)
#define MAYBE_SavePageAsMHTML DISABLED_SavePageAsMHTML
#else
#define MAYBE_SavePageAsMHTML SavePageAsMHTML
#endif
IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, MAYBE_SavePageAsMHTML) {
  static const int64_t kFileSizeMin = 2758;
  GURL url = NavigateToMockURL("b");
  base::FilePath download_dir = DownloadPrefs::FromDownloadManager(
      GetDownloadManager())->DownloadPath();
  base::FilePath full_file_name = download_dir.AppendASCII(std::string(
      "Test page for saving page feature.mhtml"));

  SavePackageFilePicker::SetShouldPromptUser(true);
  DownloadPersistedObserver persisted(
      browser()->profile(),
      base::BindRepeating(&DownloadStoredProperly, url, full_file_name, -1,
                          history::DownloadState::COMPLETE));

  FakeSelectFileDialog::Factory* select_file_dialog_factory =
      FakeSelectFileDialog::RegisterFactory();
  // Save page and run until the fake select file dialog opens.
  {
    base::RunLoop run_loop;
    select_file_dialog_factory->SetOpenCallback(run_loop.QuitClosure());
    chrome::SavePage(browser());
    run_loop.Run();
  }

// On ChromeOS, the default should be MHTML.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ASSERT_EQ("mhtml",
            select_file_dialog_factory->GetLastDialog()->default_extension());
#else
  ASSERT_EQ("html",
            select_file_dialog_factory->GetLastDialog()->default_extension());
#endif

  // Save the file as MHTML. Run until save completes.
  base::RunLoop run_loop;
  content::SavePackageFinishedObserver observer(
      browser()->profile()->GetDownloadManager(), run_loop.QuitClosure());
  ASSERT_TRUE(select_file_dialog_factory->GetLastDialog()->CallFileSelected(
      full_file_name, "mhtml"));
  run_loop.Run();

  ASSERT_TRUE(VerifySavePackageExpectations(browser(), url));
  persisted.WaitForPersisted();

  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(base::PathExists(full_file_name));
  int64_t actual_file_size = -1;
  EXPECT_TRUE(base::GetFileSize(full_file_name, &actual_file_size));
  EXPECT_LE(kFileSizeMin, actual_file_size);

  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(full_file_name, &contents));
  // Test for a CSS encoded character.  This used to use HTML encoding.
  EXPECT_THAT(contents, HasSubstr("content: \"\\e003 \\e004 b\""));
}

// Tests that if we default our file picker to MHTML due to user preference we
// update the suggested file name to end with .mhtml.
IN_PROC_BROWSER_TEST_F(SavePageBrowserTest,
                       SavePageAsMHTMLByPrefUpdatesExtension) {
  SavePackageFilePicker::SetShouldPromptUser(false);
  DownloadPrefs* download_prefs =
      DownloadPrefs::FromDownloadManager(GetDownloadManager());
  base::FilePath download_dir = download_prefs->DownloadPath();
  base::FilePath full_file_name = download_dir.AppendASCII("test_page");
  download_prefs->SetSaveFileType(content::SAVE_PAGE_TYPE_AS_MHTML);

  content::SavePackagePathPickedParams received_params;
  content::SavePackagePathPickedCallback callback = base::BindOnce(
      [](content::SavePackagePathPickedParams* received_params,
         content::SavePackagePathPickedParams params,
         content::SavePackageDownloadCreatedCallback cb) {
        *received_params = params;
      },
      &received_params);

  // Deletes itself.
  new SavePackageFilePicker(
      /*web_contents=*/GetCurrentTab(browser()),
      /*suggested_path=*/full_file_name,
      /*default_extension=*/FILE_PATH_LITERAL(".html"),
      /*can_save_as_complete=*/true,
      /*download_prefs=*/download_prefs,
      /*callback=*/std::move(callback));

  EXPECT_TRUE(
      received_params.file_path.MatchesExtension(FILE_PATH_LITERAL(".mhtml")));
  EXPECT_EQ(received_params.save_type, content::SAVE_PAGE_TYPE_AS_MHTML);
}

// Flaky on Windows: https://crbug.com/1247404.
#if BUILDFLAG(IS_WIN)
#define MAYBE_SavePageBrowserTest_NonMHTML DISABLED_SavePageBrowserTest_NonMHTML
#else
#define MAYBE_SavePageBrowserTest_NonMHTML SavePageBrowserTest_NonMHTML
#endif
IN_PROC_BROWSER_TEST_F(SavePageBrowserTest,
                       MAYBE_SavePageBrowserTest_NonMHTML) {
  SavePackageFilePicker::SetShouldPromptUser(false);
  GURL url("data:text/plain,foo");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  base::RunLoop run_loop;
  content::SavePackageFinishedObserver observer(
      browser()->profile()->GetDownloadManager(), run_loop.QuitClosure());
  chrome::SavePage(browser());
  run_loop.Run();
  base::FilePath download_dir = DownloadPrefs::FromDownloadManager(
      GetDownloadManager())->DownloadPath();
  base::FilePath filename = download_dir.AppendASCII("dataurl.txt");
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(base::PathExists(filename));
  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(filename, &contents));
  EXPECT_EQ("foo", contents);
}

// If a save-page-complete operation results in creating subresources that would
// otherwise be considered dangerous, such files should get a .download
// extension appended so that they won't be accidentally executed by the user.
IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, DangerousSubresources) {
  GURL url =
      embedded_test_server()->GetURL("/save_page/dubious-subresources.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  base::FilePath full_file_name, dir;
  SaveCurrentTab(url, content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML,
                 "dubious-subresources", 2, &dir, &full_file_name);
  ASSERT_FALSE(HasFailure());

  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::PathExists(full_file_name));
  EXPECT_TRUE(base::PathExists(dir.AppendASCII("not-a-crx.crx.download")));
}

// Test that we don't crash when the page contains an iframe that
// was handled as a download (http://crbug.com/42212).
IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, SaveDownloadableIFrame) {
  GURL url =
      embedded_test_server()->GetURL("/downloads/iframe-src-is-a-download.htm");

  // Wait for and then dismiss the non-save-page-as-related download item
  // (the one associated with downloading of "thisdayinhistory.xls" file).
  {
    GURL download_url =
        embedded_test_server()->GetURL("/downloads/thisdayinhistory.xls");
    DownloadPersistedObserver persisted(
        browser()->profile(),
        base::BindRepeating(&DownloadStoredProperly, download_url,
                            base::FilePath(), -1,
                            history::DownloadState::COMPLETE));

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    ASSERT_TRUE(VerifySavePackageExpectations(browser(), download_url));
    persisted.WaitForPersisted();
    std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
    GetDownloadManager()->GetAllDownloads(&downloads);
    for (download::DownloadItem* download : downloads) {
      download->Remove();
    }
  }

  base::FilePath full_file_name, dir;
  SaveCurrentTab(url, content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML,
                 "iframe-src-is-a-download", 3, &dir, &full_file_name);
  ASSERT_FALSE(HasFailure());

  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::PathExists(full_file_name));
  EXPECT_TRUE(base::PathExists(dir.AppendASCII("thisdayinhistory.html")));
  EXPECT_TRUE(base::PathExists(dir.AppendASCII("no-such-file.html")));
}

// Test that file: URI won't be saved when referred to from an HTTP page.
// See also https://crbug.com/616429.
IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, SaveUnauthorizedResource) {
  GURL url = NavigateToMockURL("unauthorized-access");

  // Create a test file (that the web page should not have access to).
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir2;
  ASSERT_TRUE(temp_dir2.CreateUniqueTempDir());
  base::FilePath file_path =
      temp_dir2.GetPath().Append(FILE_PATH_LITERAL("should-not-save.jpg"));
  std::string file_content("fake-jpg");
  ASSERT_TRUE(base::WriteFile(file_path, file_content));

  // Refer to the test file from the test page.
  GURL file_url = net::FilePathToFileURL(file_path);
  ASSERT_TRUE(ExecJs(
      browser()->tab_strip_model()->GetWebContentsAt(0),
      base::StringPrintf("document.getElementById('resource1').src = '%s';",
                         file_url.spec().data())));

  // Save the current page.
  base::FilePath full_file_name, dir;
  SaveCurrentTab(url, content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML,
                 "unauthorized-access", 2, &dir, &full_file_name);

  // We should not save resource that the web page didn't have access to.
  // (because executing a resource request can have side effects - for example
  // after https://crbug.com/590714 a website from the internet should not be
  // able to issue a resource request to an intranet website and trigger
  // server-side actions in the internet;  this test uses a file: URI as a
  // canary for detecting whether a website can access restricted resources).
  EXPECT_FALSE(base::PathExists(dir.AppendASCII("should-not-save.jpg")));
}

#if BUILDFLAG(IS_WIN)
// Save a file and confirm that the file is correctly quarantined.
IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, SaveURLQuarantine) {
  GURL url = embedded_test_server()->GetURL("/save_page/text.txt");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  base::FilePath full_file_name, dir;
  SaveCurrentTab(url, content::SAVE_PAGE_TYPE_AS_ONLY_HTML, "test", 1, &dir,
                 &full_file_name);
  ASSERT_FALSE(HasFailure());

  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::PathExists(full_file_name));
  EXPECT_FALSE(base::PathExists(dir));
  EXPECT_TRUE(base::ContentsEqual(GetTestDirFile("text.txt"), full_file_name));
  EXPECT_TRUE(quarantine::IsFileQuarantined(full_file_name, url, GURL()));
}
#endif

// Test suite that allows testing --site-per-process against cross-site frames.
// See http://dev.chromium.org/developers/design-documents/site-isolation.
class SavePageSitePerProcessBrowserTest : public SavePageBrowserTest {
 public:
  SavePageSitePerProcessBrowserTest() {}

  SavePageSitePerProcessBrowserTest(const SavePageSitePerProcessBrowserTest&) =
      delete;
  SavePageSitePerProcessBrowserTest& operator=(
      const SavePageSitePerProcessBrowserTest&) = delete;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SavePageBrowserTest::SetUpCommandLine(command_line);

    // Append --site-per-process flag.
    content::IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    SavePageBrowserTest::SetUpOnMainThread();

    // Used by the BrokenImage test which depends on *.no.such.host not
    // resolving to 127.0.0.1
    host_resolver()->AddRule("no.such.host", "128.0.0.1");
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

// Test for crbug.com/526786.
IN_PROC_BROWSER_TEST_F(SavePageSitePerProcessBrowserTest, SaveAsCompleteHtml) {
  GURL url(
      embedded_test_server()->GetURL("a.com", "/save_page/frames-xsite.htm"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  base::FilePath full_file_name, dir;
  SaveCurrentTab(url, content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML,
                 "frames-xsite-complete-html", 5, &dir, &full_file_name);
  ASSERT_FALSE(HasFailure());

  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::DirectoryExists(dir));
  base::FilePath expected_files[] = {
      full_file_name,
      dir.AppendASCII("a.html"),  // From iframes.htm
      dir.AppendASCII("b.html"),  // From iframes.htm
      dir.AppendASCII("1.css"),   // From b.htm
      dir.AppendASCII("1.png"),   // Deduplicated from iframes.htm and b.htm.
  };
  for (auto file_path : expected_files) {
    EXPECT_TRUE(base::PathExists(file_path)) << "Does " << file_path.value()
                                             << " exist?";
    int64_t actual_file_size = 0;
    EXPECT_TRUE(base::GetFileSize(file_path, &actual_file_size));
    EXPECT_NE(0, actual_file_size) << "Is " << file_path.value()
                                   << " non-empty?";
  }

  // Verify that local links got correctly replaced with local paths
  // (most importantly for iframe elements, which are only exercised
  // by this particular test).
  std::string main_contents;
  ASSERT_TRUE(base::ReadFileToString(full_file_name, &main_contents));
  EXPECT_THAT(
      main_contents,
      HasSubstr("<iframe "
                "src=\"./frames-xsite-complete-html_files/a.html\"></iframe>"));
  EXPECT_THAT(
      main_contents,
      HasSubstr("<iframe "
                "src=\"./frames-xsite-complete-html_files/b.html\"></iframe>"));
  EXPECT_THAT(
      main_contents,
      HasSubstr("<img src=\"./frames-xsite-complete-html_files/1.png\">"));

  // Verification of html contents.
  EXPECT_THAT(
      main_contents,
      HasSubstr("frames-xsite.htm: 896fd88d-a77a-4f46-afd8-24db7d5af9c2"));
  std::string a_contents;
  ASSERT_TRUE(base::ReadFileToString(dir.AppendASCII("a.html"), &a_contents));
  EXPECT_THAT(a_contents,
              HasSubstr("a.htm: 1b8aae2b-e164-462f-bd5b-98aa366205f2"));
  std::string b_contents;
  ASSERT_TRUE(base::ReadFileToString(dir.AppendASCII("b.html"), &b_contents));
  EXPECT_THAT(b_contents,
              HasSubstr("b.htm: 3a35f7fa-96a9-4487-9f18-4470263907fa"));
}

// Test for crbug.com/538766.
// Disabled on Mac due to excessive flakiness. https://crbug.com/1271741
#if BUILDFLAG(IS_MAC)
#define MAYBE_SaveAsMHTML DISABLED_SaveAsMHTML
#else
#define MAYBE_SaveAsMHTML SaveAsMHTML
#endif
IN_PROC_BROWSER_TEST_F(SavePageSitePerProcessBrowserTest,
                       MAYBE_SaveAsMHTML) {
  GURL url(
      embedded_test_server()->GetURL("a.com", "/save_page/frames-xsite.htm"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  base::FilePath full_file_name, dir;
  SaveCurrentTab(url, content::SAVE_PAGE_TYPE_AS_MHTML, "frames-xsite-mhtml",
                 -1, &dir, &full_file_name);
  ASSERT_FALSE(HasFailure());

  std::string mhtml;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::ReadFileToString(full_file_name, &mhtml));
  }

  // Verify content of main frame, subframes and some savable resources.
  EXPECT_THAT(
      mhtml,
      HasSubstr("frames-xsite.htm: 896fd88d-a77a-4f46-afd8-24db7d5af9c2"));
  EXPECT_THAT(mhtml, HasSubstr("a.htm: 1b8aae2b-e164-462f-bd5b-98aa366205f2"));
  EXPECT_THAT(mhtml, HasSubstr("b.htm: 3a35f7fa-96a9-4487-9f18-4470263907fa"));
  EXPECT_THAT(mhtml, HasSubstr("font-size: 20px;"))
      << "Verifying if content from 1.css is present";

  // Verify presence of URLs associated with main frame, subframes and some
  // savable resources.
  // (note that these are single-line regexes).
  EXPECT_THAT(mhtml,
              ContainsRegex("Content-Location.*/save_page/frames-xsite.htm"));
  EXPECT_THAT(mhtml, ContainsRegex("Content-Location.*/save_page/a.htm"));
  EXPECT_THAT(mhtml, ContainsRegex("Content-Location.*/save_page/b.htm"));
  EXPECT_THAT(mhtml, ContainsRegex("Content-Location.*/save_page/1.css"));
  EXPECT_THAT(mhtml, ContainsRegex("Content-Location.*/save_page/1.png"));

  // Verify that 1.png appears in the output only once (despite being referred
  // to twice - from iframes.htm and from b.htm).
  int count = 0;
  size_t pos = 0;
  for (;;) {
    pos = mhtml.find("Content-Type: image/png", pos);
    if (pos == std::string::npos)
      break;
    count++;
    pos++;
  }
  EXPECT_EQ(1, count) << "Verify number of image/png parts in the mhtml output";
}

// Test for crbug.com/541342 - handling of dead renderer processes.
IN_PROC_BROWSER_TEST_F(SavePageSitePerProcessBrowserTest,
                       CompleteHtmlWhenRendererIsDead) {
  GURL url(
      embedded_test_server()->GetURL("a.com", "/save_page/frames-xsite.htm"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Kill one of renderer processes (this is the essence of this test).
  WebContents* web_contents = GetCurrentTab(browser());
  bool did_kill_a_process = false;
  web_contents->GetPrimaryMainFrame()
      ->ForEachRenderFrameHostWithAction(
          [web_contents, &did_kill_a_process](RenderFrameHost* frame) {
            if (frame->GetLastCommittedURL().host() == "bar.com") {
              RenderProcessHost* process_to_kill = frame->GetProcess();
              EXPECT_NE(
                  web_contents->GetPrimaryMainFrame()->GetProcess()->GetID(),
                  process_to_kill->GetID())
                  << "a.com and bar.com should be in different processes.";

              EXPECT_TRUE(process_to_kill->FastShutdownIfPossible());
              EXPECT_FALSE(process_to_kill->IsInitializedAndNotDead());
              did_kill_a_process = true;
              return content::RenderFrameHost::FrameIterationAction::kStop;
            }
            return content::RenderFrameHost::FrameIterationAction::kContinue;
          });
  EXPECT_TRUE(did_kill_a_process);

  // Main verification is that we don't hang and time out when saving.
  base::FilePath full_file_name, dir;
  SaveCurrentTab(url, content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML,
                 "frames-xsite-complete-html", 5, &dir, &full_file_name);
  ASSERT_FALSE(HasFailure());
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::DirectoryExists(dir));
  EXPECT_TRUE(base::PathExists(full_file_name));
}

// Test suite that verifies that the frame tree "looks" the same before
// and after a save-page-as.
class SavePageOriginalVsSavedComparisonTest
    : public SavePageSitePerProcessBrowserTest,
      public ::testing::WithParamInterface<content::SavePageType> {
 protected:
  void TestOriginalVsSavedPage(
      content::SavePageType save_page_type,
      const GURL& url,
      int expected_number_of_frames_in_original_page,
      int expected_number_of_frames_in_mhtml_page,
      const std::vector<std::string>& expected_substrings) {
    // Navigate to the test page and verify if test expectations
    // are met (this is mostly a sanity check - a failure to meet
    // expectations would probably mean that there is a test bug
    // (i.e. that we got called with wrong expected_foo argument).
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    DLOG(INFO) << "Verifying test expectations for original page... : "
               << GetCurrentTab(browser())->GetLastCommittedURL();
    AssertExpectationsAboutCurrentTab(
        expected_number_of_frames_in_original_page, expected_substrings,
        save_page_type);

    // Save the page.
    base::FilePath full_file_name, dir;
    SaveCurrentTab(url, save_page_type, "save_result", -1, &dir,
                   &full_file_name);
    ASSERT_FALSE(HasFailure());

    // Stop the test server (to make sure the locally saved page
    // is self-contained / won't try to open original resources).
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

    // Open the saved page and verify if test expectations are
    // met (i.e. if the same expectations are met for "after"
    // [saved version of the page] as for the "before"
    // [the original version of the page].
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GURL(net::FilePathToFileURL(full_file_name))));
    DLOG(INFO) << "Verifying test expectations for saved page... : "
               << GetCurrentTab(browser())->GetLastCommittedURL();
    // Hidden elements, i.e., hidden frames, will be removed only from MHTML
    // page. They're still kept in other types of serialization, like saving
    // as a complete html page.
    int expected_number_of_frames_in_saved_page =
        (save_page_type == content::SAVE_PAGE_TYPE_AS_MHTML) ?
        expected_number_of_frames_in_mhtml_page :
        expected_number_of_frames_in_original_page;
    AssertExpectationsAboutCurrentTab(expected_number_of_frames_in_saved_page,
                                      expected_substrings, save_page_type);

    if (GetParam() == content::SAVE_PAGE_TYPE_AS_MHTML) {
      std::set<url::Origin> origins;
      GetCurrentTab(browser())->GetPrimaryMainFrame()->ForEachRenderFrameHost(
          [&origins](content::RenderFrameHost* host) {
            CheckFrameForMHTML(host, origins);
          });
      int unique_origins = origins.size();
      EXPECT_EQ(expected_number_of_frames_in_saved_page, unique_origins)
          << "All origins should be unique";
    }

    // Check that we're able to navigate away and come back, as well.
    // See https://crbug.com/948246.
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL("data:text/html,foo")));
    chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
    EXPECT_TRUE(content::WaitForLoadStop(GetCurrentTab(browser())));
    DLOG(INFO) << "Verifying test expectations after history navigation...";
    AssertExpectationsAboutCurrentTab(expected_number_of_frames_in_saved_page,
                                      expected_substrings, save_page_type);
  }

  // Helper method to deduplicate some code across 2 tests.
  void RunObjectElementsTest(GURL url) {
    content::SavePageType save_page_type = GetParam();

    // The |expected_number_of_frames| comes from:
    // - main frame (frames-objects.htm)
    // - object with frame-nested.htm + 2 subframes (frames-nested2.htm + b.htm)
    // - iframe with a.htm
    // - object with svg.svg
    // - object with text.txt
    // - object with pdf.pdf is responsible for presence of 2 extra frames
    //   (about:blank + one frame for the actual pdf.pdf).  These frames are an
    //   implementation detail and are not web-exposed (e.g. via window.frames).
    int expected_number_of_frames = 9;

    std::vector<std::string> expected_substrings = {
        "frames-objects.htm: 8da13db4-a512-4d9b-b1c5-dc1c134234b9",
        "a.htm: 1b8aae2b-e164-462f-bd5b-98aa366205f2",
        "b.htm: 3a35f7fa-96a9-4487-9f18-4470263907fa",
        "frames-nested.htm: 4388232f-8d45-4d2e-9807-721b381be153",
        "frames-nested2.htm: 6d23dc47-f283-4977-96ec-66bcf72301a4",
        "text-object.txt: ae52dd09-9746-4b7e-86a6-6ada5e2680c2",
        "svg: 0875fd06-131d-4708-95e1-861853c6b8dc",

        // TODO(lukasza): Consider also verifying presence of "PDF test file"
        // from <object data="pdf.pdf">.  This requires ensuring that the PDF is
        // loaded before continuing with the test.
    };

    // TODO(lukasza): crbug.com/553478: Enable <object> testing of MHTML.
    if (save_page_type == content::SAVE_PAGE_TYPE_AS_MHTML)
      return;

    TestOriginalVsSavedPage(save_page_type, url, expected_number_of_frames,
                            expected_number_of_frames, expected_substrings);
  }

 private:
  void AssertExpectationsAboutCurrentTab(
      int expected_number_of_frames,
      const std::vector<std::string>& expected_substrings,
      content::SavePageType save_page_type) {
    int actual_number_of_frames =
        CollectAllRenderFrameHosts(GetCurrentTab(browser())->GetPrimaryPage())
            .size();
    EXPECT_EQ(expected_number_of_frames, actual_number_of_frames);

    for (const auto& expected_substring : expected_substrings) {
      int actual_number_of_matches = ui_test_utils::FindInPage(
          GetCurrentTab(browser()), base::UTF8ToUTF16(expected_substring),
          true,   // |forward|
          false,  // |case_sensitive|
          nullptr, nullptr);

      EXPECT_EQ(1, actual_number_of_matches)
          << "Verifying that \"" << expected_substring << "\" appears "
          << "exactly once in the text of web contents";

      // TODO(lukasza): https://crbug.com/1070597 and https://crbug.com/1070886:
      // Remove the extra test assertions below (and maybe also the
      // |save_page_type| parameter) after we get a better understanding of the
      // root cause of test flakiness.
      if (expected_substring == "a.htm: 1b8aae2b-e164-462f-bd5b-98aa366205f2" &&
          save_page_type == content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML) {
        DLOG(INFO) << "Verifying that a.htm frame has fully loaded...";
        std::vector<std::string> frame_names;
        GetCurrentTab(browser())->GetPrimaryMainFrame()->ForEachRenderFrameHost(
            [&frame_names](content::RenderFrameHost* frame) {
              frame_names.push_back(frame->GetFrameName());
            });

        EXPECT_THAT(frame_names, testing::Contains("Frame name of a.htm"));
      }
    }

    std::string forbidden_substrings[] = {
        "head",  // Html markup should not be visible.
        "err",   // "err" is a prefix of error messages + is strategically
                 // included in some tests in contents that should not render
                 // (i.e. inside of an object element and/or inside of a frame
                 // that should be hidden).
    };
    for (const auto& forbidden_substring : forbidden_substrings) {
      int actual_number_of_matches = ui_test_utils::FindInPage(
          GetCurrentTab(browser()), base::UTF8ToUTF16(forbidden_substring),
          true,   // |forward|
          false,  // |case_sensitive|
          nullptr, nullptr);
      EXPECT_EQ(0, actual_number_of_matches)
          << "Verifying that \"" << forbidden_substring << "\" doesn't "
          << "appear in the text of web contents";
    }
  }

  static void CheckFrameForMHTML(content::RenderFrameHost* host,
                                 std::set<url::Origin>& origins) {
    // See RFC n°2557, section-8.3: "Use of the Content-ID header and CID URLs".
    const char kContentIdScheme[] = "cid";
    origins.insert(host->GetLastCommittedOrigin());
    EXPECT_TRUE(host->GetLastCommittedOrigin().opaque());
    if (!host->GetParent())
      EXPECT_TRUE(host->GetLastCommittedURL().SchemeIsFile());
    else
      EXPECT_TRUE(host->GetLastCommittedURL().SchemeIs(kContentIdScheme));
  }
};

// Test coverage for:
// - crbug.com/526786: OOPIFs support for CompleteHtml
// - crbug.com/538766: OOPIFs support for MHTML
// - crbug.com/539936: Subframe gets redirected.
// Test compares original-vs-saved for a page with cross-site frames
// (subframes get redirected to a different domain - see frames-xsite.htm).
IN_PROC_BROWSER_TEST_P(SavePageOriginalVsSavedComparisonTest, CrossSite) {
  content::SavePageType save_page_type = GetParam();

  std::vector<std::string> expected_substrings = {
      "frames-xsite.htm: 896fd88d-a77a-4f46-afd8-24db7d5af9c2",
      "a.htm: 1b8aae2b-e164-462f-bd5b-98aa366205f2",
      "b.htm: 3a35f7fa-96a9-4487-9f18-4470263907fa",
  };

  GURL url(
      embedded_test_server()->GetURL("a.com", "/save_page/frames-xsite.htm"));

  TestOriginalVsSavedPage(save_page_type, url, 3, 3, expected_substrings);
}

// Test compares original-vs-saved for a page with <object> elements.
// (see crbug.com/553478).
// crbug.com/1070886: disabled because of flakiness.
IN_PROC_BROWSER_TEST_P(SavePageOriginalVsSavedComparisonTest,
                       DISABLED_ObjectElementsViaHttp) {
  GURL url(
      embedded_test_server()->GetURL("a.com", "/save_page/frames-objects.htm"));

  RunObjectElementsTest(url);
}

// Tests that saving a page from file: URI works.
// TODO(lukasza): https://crbug.com/964364: Re-enable the test.
IN_PROC_BROWSER_TEST_P(SavePageOriginalVsSavedComparisonTest,
                       DISABLED_ObjectElementsViaFile) {
  base::FilePath test_data_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
  GURL url(net::FilePathToFileURL(
      test_data_dir.Append(FILE_PATH_LITERAL("save_page/frames-objects.htm"))));
  EXPECT_TRUE(url.SchemeIsFile());

  RunObjectElementsTest(url);
}

// Test compares original-vs-saved for a page with frames at about:blank uri.
// This tests handling of iframe elements without src attribute (only with
// srcdoc attribute) and how they get saved / cross-referenced.
#if BUILDFLAG(IS_MAC)
// TODO(crbug.com/40202613): Fails on dcheck-enabled builds on 11.0.
#define MAYBE_AboutBlank DISABLED_AboutBlank
#else
#define MAYBE_AboutBlank AboutBlank
#endif
IN_PROC_BROWSER_TEST_P(SavePageOriginalVsSavedComparisonTest,
                       MAYBE_AboutBlank) {
  content::SavePageType save_page_type = GetParam();

  std::vector<std::string> expected_substrings = {
      "main: acb0609d-eb10-4c26-83e2-ad8afb7b0ff3",
      "sub1: b124df3a-d39f-47a1-ae04-5bb5d0bf549e",
      "sub2: 07014068-604d-45ae-884f-a068cfe7bc0a",
      "sub3: 06cc8fcc-c692-4a1a-a10f-1645b746e8f4",
  };

  GURL url(embedded_test_server()->GetURL("a.com",
                                          "/save_page/frames-about-blank.htm"));

  TestOriginalVsSavedPage(save_page_type, url, 4, 4, expected_substrings);
}

// Test compares original-vs-saved for a page with nested frames.
// Two levels of nesting are especially good for verifying correct
// link rewriting for subframes-vs-main-frame (see crbug.com/554666).
IN_PROC_BROWSER_TEST_P(SavePageOriginalVsSavedComparisonTest, NestedFrames) {
  content::SavePageType save_page_type = GetParam();

  std::vector<std::string> expected_substrings = {
      "frames-nested.htm: 4388232f-8d45-4d2e-9807-721b381be153",
      "frames-nested2.htm: 6d23dc47-f283-4977-96ec-66bcf72301a4",
      "b.htm: 3a35f7fa-96a9-4487-9f18-4470263907fa",
  };

  GURL url(
      embedded_test_server()->GetURL("a.com", "/save_page/frames-nested.htm"));

  TestOriginalVsSavedPage(save_page_type, url, 3, 3, expected_substrings);
}

// Test for crbug.com/106364 and crbug.com/538188.
// Test frames have the same uri ...
//   subframe1 and subframe2 - both have src=b.htm
//   subframe3 and subframe4 - about:blank (no src, only srcdoc attribute).
// ... but different content (generated by main frame's javascript).
#if BUILDFLAG(IS_MAC)
// TODO(crbug.com/40202613): Fails on dcheck-enabled builds on 11.0.
#define MAYBE_RuntimeChanges DISABLED_RuntimeChanges
#else
#define MAYBE_RuntimeChanges RuntimeChanges
#endif
IN_PROC_BROWSER_TEST_P(SavePageOriginalVsSavedComparisonTest,
                       MAYBE_RuntimeChanges) {
  content::SavePageType save_page_type = GetParam();

  std::vector<std::string> expected_substrings = {
      "frames-runtime-changes.htm: 4388232f-8d45-4d2e-9807-721b381be153",
      "subframe1: 21595339-61fc-4854-b6df-0668328ea263",
      "subframe2: adf55719-15e7-45be-9eda-d12fe782a1bd",
      "subframe3: 50e294bf-3a5b-499d-8772-651ead26952f",
      "subframe4: e0ea9289-7467-4d32-ba5c-c604e8d84cb7",
  };

  GURL url(embedded_test_server()->GetURL(
      "a.com", "/save_page/frames-runtime-changes.htm?do_runtime_changes=1"));

  TestOriginalVsSavedPage(save_page_type, url, 5, 5, expected_substrings);
}

// Test for saving frames with various encodings:
// - iso-8859-2: encoding declared via <meta> element
// - utf16-le-bom.htm, utf16-be-bom.htm: encoding detected via BOM
// - utf16-le-nobom.htm, utf16-le-nobom.htm - encoding declared via
//                                            mocked http headers
IN_PROC_BROWSER_TEST_P(SavePageOriginalVsSavedComparisonTest, Encoding) {
  content::SavePageType save_page_type = GetParam();

  std::vector<std::string> expected_substrings = {
      "frames-encodings.htm: f53295dd-a95b-4b32-85f5-b6e15377fb20",
      "iso-8859-2.htm: Za\xc5\xbc\xc3\xb3\xc5\x82\xc4\x87 "
      "g\xc4\x99\xc5\x9bl\xc4\x85 ja\xc5\xba\xc5\x84",
      "utf16-le-nobom.htm: Za\xc5\xbc\xc3\xb3\xc5\x82\xc4\x87 "
      "g\xc4\x99\xc5\x9bl\xc4\x85 ja\xc5\xba\xc5\x84",
      "utf16-le-bom.htm: Za\xc5\xbc\xc3\xb3\xc5\x82\xc4\x87 "
      "g\xc4\x99\xc5\x9bl\xc4\x85 ja\xc5\xba\xc5\x84",
      "utf16-be-nobom.htm: Za\xc5\xbc\xc3\xb3\xc5\x82\xc4\x87 "
      "g\xc4\x99\xc5\x9bl\xc4\x85 ja\xc5\xba\xc5\x84",
      "utf16-be-bom.htm: Za\xc5\xbc\xc3\xb3\xc5\x82\xc4\x87 "
      "g\xc4\x99\xc5\x9bl\xc4\x85 ja\xc5\xba\xc5\x84"};

  GURL url(embedded_test_server()->GetURL("a.com",
                                          "/save_page/frames-encodings.htm"));

  // TODO(lukasza): crbug.com/541699: MHTML needs to handle multi-byte encodings
  // by either:
  // 1. Continuing to preserve the original encoding, but starting to round-trip
  //    the encoding declaration (in Content-Type MIME/MHTML header?)
  // 2. Saving html docs in UTF8.
  // 3. Saving the BOM (not sure if this will help for all cases though).
  if (save_page_type == content::SAVE_PAGE_TYPE_AS_MHTML)
    return;

  TestOriginalVsSavedPage(save_page_type, url, 6, 6, expected_substrings);
}

// Test for saving style element and attribute (see also crbug.com/568293).
#if BUILDFLAG(IS_MAC)
// TODO(crbug.com/40202613): Fails on dcheck-enabled builds on 11.0.
#define MAYBE_Style DISABLED_Style
#else
#define MAYBE_Style Style
#endif
IN_PROC_BROWSER_TEST_P(SavePageOriginalVsSavedComparisonTest, MAYBE_Style) {
  content::SavePageType save_page_type = GetParam();

  std::vector<std::string> expected_substrings = {
      "style.htm: af84c3ca-0fc6-4b0d-bf7a-5ac18a4dab62",
      "frameF: c9539ccd-47b0-47cf-a03b-734614865872",
  };

  GURL url(embedded_test_server()->GetURL("a.com", "/save_page/style.htm"));

  // The original page has 7 iframes. One of them that contains hidden attribute
  // will be excluded from the saved page.
  TestOriginalVsSavedPage(save_page_type, url, 7, 6, expected_substrings);
}

// Test for saving a page with broken subresources:
// - Broken, undecodable image (see also https://crbug.com/586680)
// - Broken link, to unresolvable host (see also https://crbug.com/594219)
IN_PROC_BROWSER_TEST_P(SavePageOriginalVsSavedComparisonTest, BrokenImage) {
  content::SavePageType save_page_type = GetParam();

  std::vector<std::string> expected_substrings = {
      "broken-image.htm: 1e846775-b3ed-4d9c-a124-029554a1eb9d",
  };

  GURL url(embedded_test_server()->GetURL("127.0.0.1",
                                          "/save_page/broken-image.htm"));

  TestOriginalVsSavedPage(save_page_type, url, 1, 1, expected_substrings);
}

// Test for saving a page with a cross-site <object> element.
// Disabled on Windows due to flakiness. crbug.com/1070597.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_CrossSiteObject DISABLED_CrossSiteObject
#else
#define MAYBE_CrossSiteObject CrossSiteObject
#endif
IN_PROC_BROWSER_TEST_P(SavePageOriginalVsSavedComparisonTest,
                       MAYBE_CrossSiteObject) {
  content::SavePageType save_page_type = GetParam();

  std::vector<std::string> expected_substrings = {
      "cross-site-object.htm: f727dd87-2048-44cf-beee-19fa9863f046",
      "a.htm: 1b8aae2b-e164-462f-bd5b-98aa366205f2",
      "svg: 0875fd06-131d-4708-95e1-861853c6b8dc",
  };

  GURL url(embedded_test_server()->GetURL("a.com",
                                          "/save_page/cross-site-object.htm"));

  TestOriginalVsSavedPage(save_page_type, url, 4, 4, expected_substrings);
}

INSTANTIATE_TEST_SUITE_P(
    SaveAsCompleteHtml,
    SavePageOriginalVsSavedComparisonTest,
    ::testing::Values(content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML));
INSTANTIATE_TEST_SUITE_P(SaveAsMhtml,
                         SavePageOriginalVsSavedComparisonTest,
                         ::testing::Values(content::SAVE_PAGE_TYPE_AS_MHTML));

class BlockingDownloadManagerDelegate : public ChromeDownloadManagerDelegate {
 public:
  explicit BlockingDownloadManagerDelegate(Profile* profile)
      : ChromeDownloadManagerDelegate(profile) {}
  ~BlockingDownloadManagerDelegate() override = default;

  void CheckSavePackageAllowed(
      download::DownloadItem* download_item,
      base::flat_map<base::FilePath, base::FilePath> save_package_files,
      content::SavePackageAllowedCallback callback) override {
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      for (const auto& tmp_path_and_final_path : save_package_files) {
        // Every intermediate path in `save_package_files` should exist when
        // this function is called.
        EXPECT_TRUE(base::PathExists(tmp_path_and_final_path.first));

        // We don't know what exact temporary path the file has, but it
        // shouldn't be the same as its final one.
        EXPECT_NE(tmp_path_and_final_path.first,
                  tmp_path_and_final_path.second);

        save_package_final_paths_.insert(tmp_path_and_final_path.second);
      }
    }

    std::move(callback).Run(false);
  }

  void ValidateSavePackageFiles(
      const base::flat_set<base::FilePath>& expected_paths) {
    EXPECT_EQ(expected_paths.size(), save_package_final_paths_.size());
    for (const base::FilePath& expected_path : expected_paths) {
      EXPECT_TRUE(save_package_final_paths_.contains(expected_path));
    }
  }

 private:
  base::flat_set<base::FilePath> save_package_final_paths_;
};

IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, SaveOnlyHTMLBlocked) {
  GURL url = NavigateToMockURL("a");

  auto blocking_delegate =
      std::make_unique<BlockingDownloadManagerDelegate>(browser()->profile());
  blocking_delegate->GetDownloadIdReceiverCallback().Run(
      download::DownloadItem::kInvalidId + 1);
  DownloadCoreServiceFactory::GetForBrowserContext(browser()->profile())
      ->SetDownloadManagerDelegateForTesting(std::move(blocking_delegate));
  auto* delegate = static_cast<BlockingDownloadManagerDelegate*>(
      DownloadCoreServiceFactory::GetForBrowserContext(browser()->profile())
          ->GetDownloadManagerDelegate());

  base::FilePath full_file_name, dir;
  GetDestinationPaths("a", &full_file_name, &dir,
                      content::SAVE_PAGE_TYPE_AS_ONLY_HTML);
  base::RunLoop run_loop;
  content::SavePackageFinishedObserver observer(
      browser()->profile()->GetDownloadManager(), run_loop.QuitClosure());
  ASSERT_TRUE(GetCurrentTab(browser())->SavePage(
      full_file_name, dir, content::SAVE_PAGE_TYPE_AS_ONLY_HTML));

  run_loop.Run();
  ASSERT_FALSE(HasFailure());

  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_FALSE(base::PathExists(full_file_name));
  EXPECT_FALSE(base::PathExists(dir));

  delegate->ValidateSavePackageFiles({full_file_name});
}

IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, SaveCompleteHTMLBlocked) {
  GURL url = NavigateToMockURL("b");

  auto blocking_delegate =
      std::make_unique<BlockingDownloadManagerDelegate>(browser()->profile());
  blocking_delegate->GetDownloadIdReceiverCallback().Run(
      download::DownloadItem::kInvalidId + 1);
  DownloadCoreServiceFactory::GetForBrowserContext(browser()->profile())
      ->SetDownloadManagerDelegateForTesting(std::move(blocking_delegate));
  auto* delegate = static_cast<BlockingDownloadManagerDelegate*>(
      DownloadCoreServiceFactory::GetForBrowserContext(browser()->profile())
          ->GetDownloadManagerDelegate());

  base::FilePath full_file_name, dir;
  GetDestinationPaths("b", &full_file_name, &dir,
                      content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML);
  base::RunLoop run_loop;
  content::SavePackageFinishedObserver observer(
      browser()->profile()->GetDownloadManager(), run_loop.QuitClosure());
  ASSERT_TRUE(GetCurrentTab(browser())->SavePage(
      full_file_name, dir, content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML));

  run_loop.Run();
  ASSERT_FALSE(HasFailure());

  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_FALSE(base::PathExists(full_file_name));
  EXPECT_FALSE(base::PathExists(dir));

  delegate->ValidateSavePackageFiles({
      full_file_name,
      dir.AppendASCII("1.png"),
      dir.AppendASCII("1.css"),
  });
}

#if BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, SaveHTMLWithDlp) {
  base::FilePath full_file_name, dir;
  GURL url;

  chromeos::DlpClient::Shutdown();
  chromeos::DlpClient::InitializeFake();
  base::test::RepeatingTestFuture<
      dlp::AddFilesRequest, base::OnceCallback<void(dlp::AddFilesResponse)>>
      add_file_cb;
  chromeos::DlpClient::Get()->GetTestInterface()->SetAddFilesMock(
      add_file_cb.GetCallback());

  url = NavigateToMockURL("a");

  SaveCurrentTab(url, content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML, "a", 1, &dir,
                 &full_file_name);

  ASSERT_FALSE(HasFailure());

  auto request = std::get<0>(add_file_cb.Take());
  EXPECT_EQ(1, request.add_file_requests().size());
  EXPECT_EQ(full_file_name.value(), request.add_file_requests(0).file_path());
  EXPECT_EQ(request.add_file_requests(0).source_url(), url.spec());

  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::PathExists(full_file_name));
  EXPECT_FALSE(base::PathExists(dir));
}

IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, SaveMHTMLWithDlp) {
  base::FilePath full_file_name, dir;
  GURL url;

  chromeos::DlpClient::Shutdown();
  chromeos::DlpClient::InitializeFake();
  base::test::RepeatingTestFuture<
      dlp::AddFilesRequest, base::OnceCallback<void(dlp::AddFilesResponse)>>
      add_file_cb;
  chromeos::DlpClient::Get()->GetTestInterface()->SetAddFilesMock(
      add_file_cb.GetCallback());

  url = NavigateToMockURL("a");

  SaveCurrentTab(url, content::SAVE_PAGE_TYPE_AS_MHTML, "a", -1, &dir,
                 &full_file_name);

  ASSERT_FALSE(HasFailure());

  auto request = std::get<0>(add_file_cb.Take());
  EXPECT_EQ(1, request.add_file_requests().size());
  EXPECT_EQ(full_file_name.value(), request.add_file_requests(0).file_path());
  EXPECT_EQ(request.add_file_requests(0).source_url(), url.spec());

  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::PathExists(full_file_name));
  EXPECT_FALSE(base::PathExists(dir));
}

#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace
