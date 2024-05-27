// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/browser_switcher_service_win.h"

#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/syslog_logging.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/win/registry.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/browser_switcher/browser_switcher_policy_migrator.h"
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#include "chrome/browser/browser_switcher/browser_switcher_sitelist.h"
#include "chrome/browser/browser_switcher/ieem_sitelist_parser.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry.h"

namespace browser_switcher {

namespace {

const wchar_t kIeSiteListKey[] =
    L"SOFTWARE\\Policies\\Microsoft\\Internet Explorer\\Main\\EnterpriseMode";
const wchar_t kIeSiteListValue[] = L"SiteList";

const int kCurrentFileVersion = 1;

// Rule sets after merging from the 3 sources (XML sitelist, EMIE sitelist, and
// policies). This stores the rules as raw-pointers rather than unique-pointers,
// to avoid copying/moving them from their original source.
struct MergedRuleSet {
  std::vector<raw_ptr<Rule, VectorExperimental>> sitelist;
  std::vector<raw_ptr<Rule, VectorExperimental>> greylist;
};

// Creates a RuleSet that is the concatenation of all 3 sources.
MergedRuleSet GetRules(const BrowserSwitcherPrefs& prefs,
                       const BrowserSwitcherSitelist* sitelist) {
  const RuleSet* source_rulesets[] = {
      &prefs.GetRules(),
      sitelist->GetIeemSitelist(),
      sitelist->GetExternalSitelist(),
      sitelist->GetExternalGreylist(),
  };
  MergedRuleSet rules;
  for (const RuleSet* source : source_rulesets) {
    for (const auto& rule : source->sitelist)
      rules.sitelist.push_back(rule.get());
    for (const auto& rule : source->greylist)
      rules.greylist.push_back(rule.get());
  }
  return rules;
}

// Convert a ParsingMode enum value to a string, for writing to cache.dat.
std::string ParsingModeToString(ParsingMode parsing_mode) {
  switch (parsing_mode) {
    case ParsingMode::kDefault:
      return "default";
    case ParsingMode::kIESiteListMode:
      return "ie_sitelist";
    default:
      // BrowserSwitcherPrefs should've sanitized the value for us.
      NOTREACHED_IN_MIGRATION();
  }
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
  std::vector<std::string> chrome_params = prefs.GetChromeParameters();
  // Always include "--from-browser-switcher", to record the
  // Windows.Launch.FromBrowserSwitcher histogram when we come back.
  chrome_params.push_back(std::string("--") + switches::kFromBrowserSwitcher);
  buffer << base::JoinString(chrome_params, " ") << std::endl;

  const auto rules = GetRules(prefs, sitelist);

  buffer << rules.sitelist.size() << std::endl;
  for (const Rule* rule : rules.sitelist)
    buffer << rule->ToString() << std::endl;

  buffer << rules.greylist.size() << std::endl;
  for (const Rule* rule : rules.greylist)
    buffer << rule->ToString() << std::endl;

  buffer << ParsingModeToString(prefs.GetParsingMode()) << std::endl;

  return buffer.str();
}

void SaveDataToFile(const std::string& data, base::FilePath path) {
  base::FilePath dir = path.DirName();
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

  base::WriteFile(tmp_path, data);

  success = base::Move(tmp_path, path);
  UMA_HISTOGRAM_BOOLEAN("BrowserSwitcher.CacheFile.MoveSuccess", success);
}

// URL to fetch the IEEM sitelist from. Only used for testing.
std::optional<std::string>* IeemSitelistUrlForTesting() {
  static base::NoDestructor<std::optional<std::string>>
      ieem_sitelist_url_for_testing;
  return ieem_sitelist_url_for_testing.get();
}

bool IsLBSExtensionEnabled(Profile* profile) {
  auto* reg = extensions::ExtensionRegistry::Get(profile);
  DCHECK(reg);
  return reg->enabled_extensions().Contains(kLBSExtensionId);
}

}  // namespace

BrowserSwitcherServiceWin::BrowserSwitcherServiceWin(
    Profile* profile,
    base::FilePath cache_dir_for_testing)
    : BrowserSwitcherService(profile),
      cache_dir_for_testing_(std::move(cache_dir_for_testing)),
      sequenced_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})) {}

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

void BrowserSwitcherServiceWin::Init() {
  BrowserSwitcherService::Init();
  UpdateAllCacheFiles();
}

void BrowserSwitcherServiceWin::LoadRulesFromPrefs() {
  BrowserSwitcherService::LoadRulesFromPrefs();
  if (prefs().UseIeSitelist())
    sitelist()->SetIeemSitelist(prefs().GetCachedIeemSitelist());
}

base::FilePath BrowserSwitcherServiceWin::GetCacheDir() {
  if (!cache_dir_for_testing_.empty())
    return cache_dir_for_testing_;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  base::FilePath path;
  if (!base::PathService::Get(base::DIR_LOCAL_APP_DATA, &path))
    return path;
  path = path.AppendASCII("Google");
  path = path.AppendASCII("BrowserSwitcher");
  return path;
#else
  return base::FilePath();
#endif
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

  if (*IeemSitelistUrlForTesting() != std::nullopt) {
    return GURL((*IeemSitelistUrlForTesting()).value());
  }

  base::win::RegKey key;
  if (ERROR_SUCCESS != key.Open(HKEY_LOCAL_MACHINE, kIeSiteListKey, KEY_READ) &&
      ERROR_SUCCESS != key.Open(HKEY_CURRENT_USER, kIeSiteListKey, KEY_READ)) {
    return GURL();
  }
  std::wstring url_string;
  if (ERROR_SUCCESS != key.ReadValue(kIeSiteListValue, &url_string))
    return GURL();
  return GURL(base::WideToUTF8(url_string));
}

void BrowserSwitcherServiceWin::OnIeemSitelistParsed(ParsedXml xml) {
  if (xml.error) {
    SYSLOG(ERROR) << "Unable to parse IEEM SiteList: " << *xml.error;
  } else {
    VLOG(2) << "Done parsing IEEM SiteList. "
            << "Applying rules to future navigations.";

    if (prefs().UseIeSitelist())
      prefs().SetCachedIeemSitelist(xml.rules);

    sitelist()->SetIeemSitelist(std::move(xml.rules));
  }
}

void BrowserSwitcherServiceWin::PrefsFileDeleted(bool /*success*/) {
  CacheFileUpdated();
}

void BrowserSwitcherServiceWin::CacheFileUpdated() {
  if (cache_file_updated_callback_for_testing_)
    std::move(cache_file_updated_callback_for_testing_).Run();
}

void BrowserSwitcherServiceWin::SitelistCacheFileUpdated() {
  if (sitelist_cache_file_updated_callback_for_testing_)
    std::move(sitelist_cache_file_updated_callback_for_testing_).Run();
}

void BrowserSwitcherServiceWin::OnCacheFileUpdatedForTesting(
    base::OnceClosure cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  cache_file_updated_callback_for_testing_ = std::move(cb);
}

void BrowserSwitcherServiceWin::OnSitelistCacheFileUpdatedForTesting(
    base::OnceClosure cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  sitelist_cache_file_updated_callback_for_testing_ = std::move(cb);
}

void BrowserSwitcherServiceWin::DeletePrefsFile() {
  base::FilePath path = GetCacheDir();
  if (path.empty())
    return;
  path = path.AppendASCII("cache.dat");
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::GetDeleteFileCallback(
          std::move(path),
          base::BindOnce(&BrowserSwitcherServiceWin::PrefsFileDeleted,
                         weak_ptr_factory_.GetWeakPtr())));
}

void BrowserSwitcherServiceWin::SavePrefsToFile() {
  DCHECK(prefs().IsEnabled());
  base::FilePath path = GetCacheDir();
  if (path.empty())
    return;
  path = path.AppendASCII("cache.dat");
  sequenced_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&SaveDataToFile, SerializeCacheFile(prefs(), sitelist()),
                     std::move(path)),
      base::BindOnce(&BrowserSwitcherServiceWin::CacheFileUpdated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BrowserSwitcherServiceWin::DeleteSitelistCacheFile() {
  base::FilePath path = GetCacheDir();
  if (path.empty())
    return;
  path = path.AppendASCII("sitelistcache.dat");
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::GetDeleteFileCallback(
          std::move(path),
          base::OnceCallback<void(bool)>(base::DoNothing())
              .Then(base::BindOnce(
                  &BrowserSwitcherServiceWin::SitelistCacheFileUpdated,
                  weak_ptr_factory_.GetWeakPtr()))));
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
