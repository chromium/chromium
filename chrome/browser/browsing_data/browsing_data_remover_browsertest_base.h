// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_BROWSERTEST_BASE_H_

#include <string>
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/signin/public/base/signin_buildflags.h"

class BrowsingDataRemoverBrowserTestBase : public InProcessBrowserTest {
 public:
  BrowsingDataRemoverBrowserTestBase();
  ~BrowsingDataRemoverBrowserTestBase() override;

  void InitFeatureList(std::vector<base::Feature> enabled_features);

  Browser* GetBrowser() const;
  void SetUpOnMainThread() override;
  // If |browser| is not specified, |GetBrowser| will be used.
  void RunScriptAndCheckResult(const std::string& script,
                               const std::string& result,
                               Browser* browser = nullptr);
  bool RunScriptAndGetBool(const std::string& script, Browser* browser);

  // If |browser| is not specified, |GetBrowser| will be used.
  void VerifyDownloadCount(size_t expected, Browser* browser = nullptr);
  void DownloadAnItem();

  // If |browser| is not specified, |GetBrowser| will be used.
  bool HasDataForType(const std::string& type, Browser* browser = nullptr);

  // If |browser| is not specified, |GetBrowser| will be used.
  void SetDataForType(const std::string& type, Browser* browser = nullptr);

  // If |browser| is not specified, |GetBrowser| will be used.
  int GetSiteDataCount(Browser* browser = nullptr);

  void UseIncognitoBrowser();
  bool IsIncognito() { return incognito_browser_ != nullptr; }
  void RestartIncognitoBrowser();

  network::mojom::NetworkContext* network_context() const;

 protected:
  // Searches the user data directory for files that contain |hostname| in the
  // filename or as part of the content. Returns the number of files that
  // do not match any regex in |ignore_file_patterns|.
  // If |check_leveldb_content| is true, also tries to open LevelDB files and
  // look for the |hostname| inside them. If LevelDB files are locked and cannot
  // be opened, they are ignored.
  bool CheckUserDirectoryForString(
      const std::string& hostname,
      const std::vector<std::string>& ignore_file_patterns,
      bool check_leveldb_content);

  // Returns the cookie tree model for the browser.
  std::unique_ptr<CookiesTreeModel> GetCookiesTreeModel(Browser* browser);

  // Returns the sum of the number of datatypes per host.
  int GetCookiesTreeModelCount(const CookieTreeNode* root);

  // Returns a string with information about the content of the
  // cookie tree model.
  std::string GetCookiesTreeModelInfo(const CookieTreeNode* root);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Sets the APISID Gaia cookie, which is monitored by the AccountReconcilor.
  bool SetGaiaCookieForProfile(Profile* profile);
#endif

 private:
  base::test::ScopedFeatureList feature_list_;
  Browser* incognito_browser_ = nullptr;
};

#endif  // CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_BROWSERTEST_BASE_H_
