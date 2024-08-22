// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_BROWSERTEST_BASE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/browsing_data/content/browsing_data_model.h"
#include "components/signin/public/base/signin_buildflags.h"

namespace network::mojom {

class NetworkContext;

}  // namespace network::mojom

class BrowsingDataRemoverBrowserTestBase : public PlatformBrowserTest {
 public:
  BrowsingDataRemoverBrowserTestBase();
  ~BrowsingDataRemoverBrowserTestBase() override;

  void InitFeatureLists(std::vector<base::test::FeatureRef> enabled_features,
                        std::vector<base::test::FeatureRef> disabled_features);

  void SetUpOnMainThread() override;
  // If `web_contents` is not specified, `GetActiveWebContents` will be used.
  void RunScriptAndCheckResult(const std::string& script,
                               const std::string& result,
                               content::WebContents* web_contents = nullptr);
  bool RunScriptAndGetBool(const std::string& script,
                           content::WebContents* web_contents = nullptr);

  // If `profile` is not specified, `GetProfile` will be used.
  void VerifyDownloadCount(size_t expected, Profile* profile = nullptr);
  void DownloadAnItem();

  // If `web_contents` is not specified, `GetActiveWebContents` will be used.
  bool HasDataForType(const std::string& type,
                      content::WebContents* web_contents = nullptr);

  // If `web_contents` is not specified, `GetActiveWebContents` will be used.
  void SetDataForType(const std::string& type,
                      content::WebContents* web_contents = nullptr);

  // If `web_contents` is not specified, `GetActiveWebContents` will be used.
  int GetSiteDataCount(content::WebContents* web_contents = nullptr);

// TODO(crbug.com/40169678): Support incognito browser tests on android.
#if BUILDFLAG(IS_ANDROID)
  bool IsIncognito() { return false; }
#else
  Browser* GetBrowser() const;
  void UseIncognitoBrowser();
  void RestartIncognitoBrowser();
  bool IsIncognito() { return incognito_browser_ != nullptr; }
#endif  // BUILDFLAG(IS_ANDROID)
  network::mojom::NetworkContext* network_context();

 protected:
  // Returns the active WebContents. On desktop this is in the first browser
  // window created by tests, more specific behaviour requires other means.
  content::WebContents* GetActiveWebContents();

#if !BUILDFLAG(IS_ANDROID)
  content::WebContents* GetActiveWebContents(Browser* browser);
#endif  // !BUILDFLAG(IS_ANDROID)

  // Returns the active Profile. On desktop this is in the first browser
  // window created by tests, more specific behaviour requires other means.
  Profile* GetProfile();

 protected:
  // Searches the user data directory for files that contain `hostname` in the
  // filename or as part of the content. Returns the number of files that
  // do not match any regex in `ignore_file_patterns`.
  // If `check_leveldb_content` is true, also tries to open LevelDB files and
  // look for the `hostname` inside them. If LevelDB files are locked and cannot
  // be opened, they are ignored.
  bool CheckUserDirectoryForString(
      const std::string& hostname,
      const std::vector<std::string>& ignore_file_patterns,
      bool check_leveldb_content);

  // Returns the browsing data model for the browser.
  std::unique_ptr<BrowsingDataModel> GetBrowsingDataModel(Profile* profile);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Sets the APISID Gaia cookie, which is monitored by the AccountReconcilor.
  bool SetGaiaCookieForProfile(Profile* profile);
#endif

 private:
  base::test::ScopedFeatureList feature_list_;
#if !BUILDFLAG(IS_ANDROID)
  raw_ptr<Browser, AcrossTasksDanglingUntriaged> incognito_browser_ = nullptr;
#endif
};

#endif  // CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_BROWSERTEST_BASE_H_
