// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/browser_switcher_service_win.h"

#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/syslog_logging.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/win/registry.h"
#include "chrome/browser/browser_switcher/browser_switcher_policy_migrator.h"
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#include "chrome/browser/browser_switcher/browser_switcher_sitelist.h"
#include "chrome/browser/browser_switcher/ieem_sitelist_parser.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_registry.h"

namespace browser_switcher {

namespace {

const wchar_t kIeSiteListKey[] =
    L"SOFTWARE\\Policies\\Microsoft\\Internet Explorer\\Main\\EnterpriseMode";
const wchar_t kIeSiteListValue[] = L"SiteList";

const int kCurrentFileVersion = 1;

// Returns "AppData\Local\Google\BrowserSwitcher".
base::FilePath GetCacheDir() {
  base::FilePath path;
  if (!base::PathService::Get(base::DIR_LOCAL_APP_DATA, &path))
    return path;
  path = path.AppendASCII("Google");
  path = path.AppendASCII("BrowserSwitcher");
  return path;
}

// Creates a RuleSet that is the concatenation of all 3 sources.
RuleSet GetRules(const BrowserSwitcherPrefs& prefs,
                 const BrowserSwitcherSitelist* sitelist) {
  const RuleSet* source_rulesets[] = {
      &prefs.GetRules(),
      sitelist->GetIeemSitelist(),
      sitelist->GetExternalSitelist(),
  };
  RuleSet rules;
  for (const RuleSet* source : source_rulesets) {
    rules.sitelist.insert(rules.sitelist.end(), source->sitelist.begin(),
                          source->sitelist.end());
    rules.greylist.insert(rules.greylist.end(), source->greylist.begin(),
                          source->greylist.end());
  }
  return rules;
}

// Serialize prefs to a string for writing to cache.dat.
std::string SerializeCacheFile(const BrowserSwitcherPrefs& prefs,
                               const BrowserSwitcherSitelist* sitelist) {
  std::ostringstream buffer;

  buffer << kCurrentFileVersion << std::endl;

  buffer << prefs.GetAlternativeBrowserPath() << std::endl;
  buffer << base::JoinString(prefs.GetAlternativeBrowserParameters(), " ")
         << std::endl;

  buffer << prefs.GetChromePath() << std::endl;
  buffer << base::JoinString(prefs.GetChromeParameters(), " ") << std::endl;

  const RuleSet rules = GetRules(prefs, sitelist);

  buffer << rules.sitelist.size() << std::endl;
  if (!rules.sitelist.empty())
    buffer << base::JoinString(rules.sitelist, "\n") << std::endl;

  buffer << rules.greylist.size() << std::endl;
  if (!rules.greylist.empty())
    buffer << base::JoinString(rules.greylist, "\n") << std::endl;

  return buffer.str();
}

void SaveDataToFile(const std::string& data, base::StringPiece file_name) {
  base::FilePath dir = GetCacheDir();

  if (dir.empty())
    return;

  // Ensure the directory exists.
  bool success = base::CreateDirectory(dir);
  UMA_HISTOGRAM_BOOLEAN("BrowserSwitcher.CacheFile.MkDirSuccess", success);
  if (!success) {
    LOG(ERROR) << "Could not create directory: " << dir.LossyDisplayName();
    return;
  }

  base::FilePath tmp_path;
  success = base::CreateTemporaryFileInDir(dir, &tmp_path);
  UMA_HISTOGRAM_BOOLEAN("BrowserSwitcher.CacheFile.MkTempSuccess", success);
  if (!success) {
    LOG(ERROR) << "Could not open file for writing: "
               << tmp_path.LossyDisplayName();
    return;
  }

  base::WriteFile(tmp_path, data.c_str(), data.size());

  base::FilePath dest_path = dir.AppendASCII(file_name);
  success = base::Move(tmp_path, dest_path);
  UMA_HISTOGRAM_BOOLEAN("BrowserSwitcher.CacheFile.MoveSuccess", success);
}

// Delete the file at "AppData\Local\Google\BrowserSwitcher\<file_name>".
void DoRemoveFileFromCacheDir(std::string file_name) {
  base::FilePath dir = GetCacheDir();

  if (dir.empty())
    return;

  // Ignore errors while deleting.
  base::FilePath dest_path = dir.AppendASCII(file_name);
  base::DeleteFile(dest_path, false);
}

// URL to fetch the IEEM sitelist from. Only used for testing.
base::Optional<std::string>* IeemSitelistUrlForTesting() {
  static base::NoDestructor<base::Optional<std::string>>
      ieem_sitelist_url_for_testing;
  return ieem_sitelist_url_for_testing.get();
}

bool IsLBSExtensionEnabled(Profile* profile) {
  auto* reg = extensions::ExtensionRegistry::Get(profile);
  DCHECK(reg);
  return reg->GetExtensionById(kLBSExtensionId,
                               extensions::ExtensionRegistry::ENABLED);
}

}  // namespace

BrowserSwitcherServiceWin::BrowserSwitcherServiceWin(Profile* profile)
    : BrowserSwitcherService(profile),
      sequenced_task_runner_(base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})) {
  UpdateAllCacheFiles();
}

BrowserSwitcherServiceWin::~BrowserSwitcherServiceWin() = default;

void BrowserSwitcherServiceWin::OnBrowserSwitcherPrefsChanged(
    BrowserSwitcherPrefs* prefs,
    const std::vector<std::string>& changed_prefs) {
  BrowserSwitcherService::OnBrowserSwitcherPrefsChanged(prefs, changed_prefs);

  UpdateAllCacheFiles();
}

// static
void BrowserSwitcherServiceWin::SetIeemSitelistUrlForTesting(
    const std::string& spec) {
  *IeemSitelistUrlForTesting() = spec;
}

std::vector<RulesetSource> BrowserSwitcherServiceWin::GetRulesetSources() {
  auto sources = BrowserSwitcherService::GetRulesetSources();
  GURL sitelist_url = GetIeemSitelistUrl();
  sources.emplace_back(
      prefs::kUseIeSitelist, sitelist_url, /* invert_rules */ false,
      base::BindOnce(&BrowserSwitcherServiceWin::OnIeemSitelistParsed,
                     weak_ptr_factory_.GetWeakPtr()));
  return sources;
}

void BrowserSwitcherServiceWin::LoadRulesFromPrefs() {
  BrowserSwitcherService::LoadRulesFromPrefs();
  if (prefs().UseIeSitelist())
    sitelist()->SetIeemSitelist(
        ParsedXml(prefs().GetCachedIeemSitelist(), base::nullopt));
  if (!prefs().IsEnabled())
    return;
  SavePrefsToFile();
}

void BrowserSwitcherServiceWin::OnAllRulesetsParsed() {
  BrowserSwitcherService::OnAllRulesetsParsed();

  if (!prefs().IsEnabled())
    return;

  SavePrefsToFile();
}

GURL BrowserSwitcherServiceWin::GetIeemSitelistUrl() {
  if (!prefs().UseIeSitelist())
    return GURL();

  if (*IeemSitelistUrlForTesting() != base::nullopt)
    return GURL((*IeemSitelistUrlForTesting()).value());

  base::win::RegKey key;
  if (ERROR_SUCCESS != key.Open(HKEY_LOCAL_MACHINE, kIeSiteListKey, KEY_READ) &&
      ERROR_SUCCESS != key.Open(HKEY_CURRENT_USER, kIeSiteListKey, KEY_READ)) {
    return GURL();
  }
  std::wstring url_string;
  if (ERROR_SUCCESS != key.ReadValue(kIeSiteListValue, &url_string))
    return GURL();
  return GURL(base::UTF16ToUTF8(url_string));
}

void BrowserSwitcherServiceWin::OnIeemSitelistParsed(ParsedXml xml) {
  if (xml.error) {
    SYSLOG(ERROR) << "Unable to parse IEEM SiteList: " << *xml.error;
  } else {
    VLOG(2) << "Done parsing IEEM SiteList. "
            << "Applying rules to future navigations.";

    if (prefs().UseIeSitelist())
      prefs().SetCachedIeemSitelist(xml.rules);

    sitelist()->SetIeemSitelist(std::move(xml));
  }
}

void BrowserSwitcherServiceWin::SavePrefsToFile() {
  DCHECK(prefs().IsEnabled());
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SaveDataToFile, SerializeCacheFile(prefs(), sitelist()),
                     "cache.dat"));
}

void BrowserSwitcherServiceWin::DeletePrefsFile() {
  sequenced_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DoRemoveFileFromCacheDir, "cache.dat"));
}

void BrowserSwitcherServiceWin::DeleteSitelistCacheFile() {
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DoRemoveFileFromCacheDir, "sitelistcache.dat"));
}

void BrowserSwitcherServiceWin::UpdateAllCacheFiles() {
  const bool has_extension = IsLBSExtensionEnabled(profile());

  if (prefs().IsEnabled())
    SavePrefsToFile();
  else if (!has_extension)
    DeletePrefsFile();

  // Clean up sitelistcache.dat from the extension, or from a previous Chrome
  // version.
  if (!has_extension)
    DeleteSitelistCacheFile();
}

}  // namespace browser_switcher
