// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/process_type.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/base/filename_util.h"

#if defined(OS_WIN)
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#endif

using content::BrowserThread;

namespace {

class CallbackBarrier : public base::RefCountedThreadSafe<CallbackBarrier> {
 public:
  explicit CallbackBarrier(const base::Closure& target_callback)
      : target_callback_(target_callback),
        outstanding_callbacks_(0),
        did_enable_(true) {
  }

  base::Callback<void(bool)> CreateCallback() {
    outstanding_callbacks_++;
    return base::Bind(&CallbackBarrier::MayRunTargetCallback, this);
  }

 private:
  friend class base::RefCountedThreadSafe<CallbackBarrier>;

  ~CallbackBarrier() {
    EXPECT_TRUE(target_callback_.is_null());
  }

  void MayRunTargetCallback(bool did_enable) {
    EXPECT_GT(outstanding_callbacks_, 0);
    did_enable_ = did_enable_ && did_enable;
    if (--outstanding_callbacks_ == 0) {
      EXPECT_TRUE(did_enable_);
      target_callback_.Run();
      target_callback_.Reset();
    }
  }

  base::Closure target_callback_;
  int outstanding_callbacks_;
  bool did_enable_;
};

}  // namespace

class ChromePluginTest : public InProcessBrowserTest {
 protected:
  ChromePluginTest() {}

  static GURL GetURL(const char* filename) {
    base::FilePath path;
    base::PathService::Get(content::DIR_TEST_DATA, &path);
    path = path.AppendASCII("plugin").AppendASCII(filename);
    CHECK(base::PathExists(path));
    return net::FilePathToFileURL(path);
  }

  static void LoadAndWait(Browser* window, const GURL& url, bool pass) {
    content::WebContents* web_contents =
        window->tab_strip_model()->GetActiveWebContents();
    base::string16 expected_title(
        base::ASCIIToUTF16(pass ? "OK" : "plugin_not_found"));
    content::TitleWatcher title_watcher(web_contents, expected_title);
    title_watcher.AlsoWaitForTitle(base::ASCIIToUTF16("FAIL"));
    title_watcher.AlsoWaitForTitle(base::ASCIIToUTF16(
        pass ? "plugin_not_found" : "OK"));
    ui_test_utils::NavigateToURL(window, url);
    ASSERT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }

  static void CrashFlash() {
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&CrashFlashInternal, runner->QuitClosure()));
    runner->Run();
  }

  static void GetFlashPath(std::vector<base::FilePath>* paths) {
    paths->clear();
    std::vector<content::WebPluginInfo> plugins = GetPlugins();
    for (std::vector<content::WebPluginInfo>::const_iterator it =
             plugins.begin(); it != plugins.end(); ++it) {
      if (it->name == base::ASCIIToUTF16(content::kFlashPluginName))
        paths->push_back(it->path);
    }
  }

  static std::vector<content::WebPluginInfo> GetPlugins() {
    std::vector<content::WebPluginInfo> plugins;
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;
    content::PluginService::GetInstance()->GetPlugins(
        base::Bind(&GetPluginsInfoCallback, &plugins, runner->QuitClosure()));
    runner->Run();
    return plugins;
  }

  static void EnsureFlashProcessCount(int expected) {
    int actual = 0;
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&CountPluginProcesses, &actual, runner->QuitClosure()));
    runner->Run();
    ASSERT_EQ(expected, actual);
  }

 private:
  static void CrashFlashInternal(const base::Closure& quit_task) {
    bool found = false;
    for (content::BrowserChildProcessHostIterator iter; !iter.Done(); ++iter) {
      if (iter.GetData().process_type != content::PROCESS_TYPE_PPAPI_PLUGIN)
        continue;
      base::Process process = base::Process::DeprecatedGetProcessFromHandle(
          iter.GetData().GetHandle());
      process.Terminate(0, true);
      found = true;
    }
    ASSERT_TRUE(found) << "Didn't find Flash process!";
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI}, quit_task);
  }

  static void GetPluginsInfoCallback(
      std::vector<content::WebPluginInfo>* rv,
      const base::Closure& quit_task,
      const std::vector<content::WebPluginInfo>& plugins) {
    *rv = plugins;
    quit_task.Run();
  }

  static void CountPluginProcesses(int* count, const base::Closure& quit_task) {
    for (content::BrowserChildProcessHostIterator iter; !iter.Done(); ++iter) {
      if (iter.GetData().process_type == content::PROCESS_TYPE_PPAPI_PLUGIN)
        (*count)++;
    }
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI}, quit_task);
  }
};

// Tests a bunch of basic scenarios with Flash.
// This test fails under ASan on Mac, see http://crbug.com/147004.
// It fails elsewhere, too.  See http://crbug.com/152071.
IN_PROC_BROWSER_TEST_F(ChromePluginTest, DISABLED_Flash) {
  // Official builds always have bundled Flash.
#if !defined(OFFICIAL_BUILD)
  std::vector<base::FilePath> flash_paths;
  GetFlashPath(&flash_paths);
  if (flash_paths.empty()) {
    LOG(INFO) << "Test not running because couldn't find Flash.";
    return;
  }
#endif

  GURL url = GetURL("flash.html");
  EnsureFlashProcessCount(0);

  // Try a single tab.
  ASSERT_NO_FATAL_FAILURE(LoadAndWait(browser(), url, true));
  EnsureFlashProcessCount(1);
  Profile* profile = browser()->profile();
  // Try another tab.
  ASSERT_NO_FATAL_FAILURE(LoadAndWait(CreateBrowser(profile), url, true));
  // Try an incognito window.
  ASSERT_NO_FATAL_FAILURE(LoadAndWait(CreateIncognitoBrowser(), url, true));
  EnsureFlashProcessCount(1);

  // Now kill Flash process and verify it reloads.
  CrashFlash();
  EnsureFlashProcessCount(0);

  ASSERT_NO_FATAL_FAILURE(LoadAndWait(browser(), url, true));
  EnsureFlashProcessCount(1);
}

#if defined(GOOGLE_CHROME_BUILD)
// Verify that the official builds have the known set of plugins.
IN_PROC_BROWSER_TEST_F(ChromePluginTest, InstalledPlugins) {
  const char* expected[] = {
    "Chrome PDF Viewer",
    "Shockwave Flash",
    "Native Client",
  };

  std::vector<content::WebPluginInfo> plugins = GetPlugins();
  for (size_t i = 0; i < arraysize(expected); ++i) {
    size_t j = 0;
    for (; j < plugins.size(); ++j) {
      if (plugins[j].name == base::ASCIIToUTF16(expected[i]))
        break;
    }
    ASSERT_TRUE(j != plugins.size()) << "Didn't find " << expected[i];
  }
}
#endif
