// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_BROWSERTEST_BASE_H_

#include <string>
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/in_process_browser_test.h"

class BrowsingDataRemoverBrowserTestBase : public InProcessBrowserTest {
 public:
  BrowsingDataRemoverBrowserTestBase();
  ~BrowsingDataRemoverBrowserTestBase() override;

  void InitFeatureList(std::vector<base::Feature> enabled_features);

  Browser* GetBrowser() const;
  void SetUpOnMainThread() override;
  void RunScriptAndCheckResult(const std::string& script,
                               const std::string& result);
  bool RunScriptAndGetBool(const std::string& script);
  void VerifyDownloadCount(size_t expected);
  void DownloadAnItem();
  bool HasDataForType(const std::string& type);

  void SetDataForType(const std::string& type);
  int GetSiteDataCount();

  void UseIncognitoBrowser();
  bool IsIncognito() { return incognito_browser_ != nullptr; }

  network::mojom::NetworkContext* network_context() const;

 private:
  base::test::ScopedFeatureList feature_list_;
  Browser* incognito_browser_ = nullptr;
};

#endif  // CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_BROWSERTEST_BASE_H_
