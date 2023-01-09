// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_SERVICE_WIN_H_
#define CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_SERVICE_WIN_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/browser_switcher/browser_switcher_service.h"

namespace browser_switcher {

// Windows-specific extension of BrowserSwitcherService.
class BrowserSwitcherServiceWin : public BrowserSwitcherService {
 public:
  BrowserSwitcherServiceWin() = delete;

  explicit BrowserSwitcherServiceWin(
      Profile* profile,
      base::FilePath cache_dir_for_testing = base::FilePath());

  BrowserSwitcherServiceWin(const BrowserSwitcherServiceWin&) = delete;
  BrowserSwitcherServiceWin& operator=(const BrowserSwitcherServiceWin&) =
      delete;

  ~BrowserSwitcherServiceWin() override;

  void Init() override;

  static void SetIeemSitelistUrlForTesting(const std::string& url);

  // BrowserSwitcherService:
  std::vector<RulesetSource> GetRulesetSources() override;

  void LoadRulesFromPrefs() override;

  void OnCacheFileUpdatedForTesting(base::OnceClosure cb);
  void OnSitelistCacheFileUpdatedForTesting(base::OnceClosure cb);

 protected:
  // BrowserSwitcherService:
  void OnAllRulesetsParsed() override;

  void OnBrowserSwitcherPrefsChanged(
      BrowserSwitcherPrefs* prefs,
      const std::vector<std::string>& changed_prefs) override;

 private:
  // Returns "AppData\Local\Google\BrowserSwitcher", in official builds.
  base::FilePath GetCacheDir();

  // Returns the URL to fetch to get Internet Explorer's Enterprise Mode
  // sitelist, based on policy. Returns an empty (invalid) URL if IE's SiteList
  // policy is unset, or if |use_ie_sitelist| is false.
  GURL GetIeemSitelistUrl();

  void OnIeemSitelistParsed(ParsedXml xml);

  // Save the current prefs' state to the "cache.dat" file, to be read & used by
  // the Internet Explorer BHO. This call does not block, it only posts a task
  // to a worker thread.
  void SavePrefsToFile();
  // Delete the "cache.dat" file created by |SavePrefsToFile()|. This call does
  // not block, it only posts a task to a worker thread.
  void DeletePrefsFile();
  // Delete the "sitelistcache.dat" file that might be left from the LBS
  // extension, or from a previous Chrome version. Called during initialization.
  void DeleteSitelistCacheFile();

  void PrefsFileDeleted(bool success);
  void CacheFileUpdated();
  void SitelistCacheFileUpdated();

  // Updates or cleans up cache.dat and sitelistcache.dat, based on whether
  // BrowserSwitcher is enabled or disabled.
  void UpdateAllCacheFiles();

  base::FilePath cache_dir_for_testing_;
  base::OnceClosure cache_file_updated_callback_for_testing_;
  base::OnceClosure sitelist_cache_file_updated_callback_for_testing_;

  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;

  base::WeakPtrFactory<BrowserSwitcherServiceWin> weak_ptr_factory_{this};
};

}  // namespace browser_switcher

#endif  // CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_SERVICE_WIN_H_
