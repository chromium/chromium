// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/browser_switcher_service.h"

#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/syslog_logging.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/browser_switcher/alternative_browser_driver.h"
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#include "chrome/browser/browser_switcher/browser_switcher_sitelist.h"
#include "chrome/browser/browser_switcher/ieem_sitelist_parser.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/file_url_loader.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"

namespace browser_switcher {

namespace {

// How long to wait after |BrowserSwitcherService| is created before initiating
// the sitelist fetch. Non-zero values are used for testing.
//
// TODO(nicolaso): get rid of this.
const base::TimeDelta kFetchSitelistDelay = base::TimeDelta();

// How long to wait after a fetch to re-fetch the sitelist to keep it fresh.
const base::TimeDelta kRefreshSitelistDelay = base::Minutes(30);

// How many times to re-try fetching the XML file for the sitelist.
const int kFetchNumRetries = 1;

constexpr net::NetworkTrafficAnnotationTag traffic_annotation =
    net::DefineNetworkTrafficAnnotation("browser_switcher_ieem_sitelist", R"(
        semantics {
          sender: "Legacy Browser Support "
          description:
            "Legacy Browser Support  may download Internet Explorer's "
            "Enterprise Mode SiteList XML, to load the list of URLs to open in "
            "an alternative browser. This is often on the organization's "
            "intranet. For more information on Internet Explorer's Enterprise "
            "Mode, see: "
            "https://docs.microsoft.com/internet-explorer/ie11-deploy-guide"
            "/what-is-enterprise-mode"
          trigger:
            "1 minute after browser startup, and then refreshes every 30 "
            "minutes afterwards. Only happens if Legacy Browser Support is "
            "enabled via enterprise policies."
          data:
            "Up to 3 (plus retries) HTTP or HTTPS GET requests to the URLs "
            "configured in Internet Explorer's SiteList policy, and Chrome's "
            "BrowserSwitcherExternalSitelistUrl and "
            "BrowserSwitcherExternalGreylistUrl policies."
          destination: OTHER
          destination_other:
            "URL configured in Internet Explorer's SiteList policy, and URLs "
            "configured in Chrome's BrowserSwitcherExternalSitelistUrl and "
            "BrowserSwitcherExternalGreylistUrl policies."
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled by settings."
          chrome_policy: {
            BrowserSwitcherEnabled: {
              BrowserSwitcherEnabled: false
            }
          }
        })");

}  // namespace

RulesetSource::RulesetSource(
    std::string pref_name_,
    GURL url_,
    bool contains_inverted_rules_,
    base::OnceCallback<void(ParsedXml xml)> parsed_callback_)
    : pref_name(std::move(pref_name_)),
      url(std::move(url_)),
      contains_inverted_rules(contains_inverted_rules_),
      parsed_callback(std::move(parsed_callback_)) {}

RulesetSource::RulesetSource(RulesetSource&&) = default;

RulesetSource::~RulesetSource() = default;

XmlDownloader::XmlDownloader(Profile* profile,
                             BrowserSwitcherService* service,
                             base::TimeDelta first_fetch_delay,
                             base::RepeatingCallback<void()> all_done_callback)
    : service_(service), all_done_callback_(std::move(all_done_callback)) {
  file_url_factory_.Bind(
      content::CreateFileURLLoaderFactory(base::FilePath(), nullptr));
  other_url_factory_ = profile->GetDefaultStoragePartition()
                           ->GetURLLoaderFactoryForBrowserProcess();

  sources_ = service_->GetRulesetSources();

  for (auto& source : sources_) {
    if (!source.url.is_valid())
      DoneParsing(&source, ParsedXml({}, {}, std::nullopt));
  }

  // Fetch in 1 minute.
  ScheduleRefresh(first_fetch_delay);
}

XmlDownloader::~XmlDownloader() = default;

bool XmlDownloader::HasValidSources() const {
  return base::ranges::any_of(sources_, [](const RulesetSource& source) {
    return source.url.is_valid();
  });
}

base::Time XmlDownloader::last_refresh_time() const {
  return last_refresh_time_;
}

base::Time XmlDownloader::next_refresh_time() const {
  return next_refresh_time_;
}

void XmlDownloader::FetchXml() {
  for (auto& source : sources_) {
    if (!source.url.is_valid()) {
      DoneParsing(&source, ParsedXml({}));
      continue;
    }

    auto request = std::make_unique<network::ResourceRequest>();
    request->url = source.url;
    request->load_flags = net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
    request->credentials_mode = network::mojom::CredentialsMode::kInclude;
    request->priority = net::IDLE;
    source.url_loader = network::SimpleURLLoader::Create(std::move(request),
                                                         traffic_annotation);
    source.url_loader->SetRetryOptions(
        kFetchNumRetries,
        network::SimpleURLLoader::RetryMode::RETRY_ON_NETWORK_CHANGE);
    source.url_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        GetURLLoaderFactoryForURL(source.url),
        base::BindOnce(&XmlDownloader::ParseXml, weak_ptr_factory_.GetWeakPtr(),
                       base::Unretained(&source)));
  }
}

network::mojom::URLLoaderFactory* XmlDownloader::GetURLLoaderFactoryForURL(
    const GURL& url) {
  if (url.SchemeIsFile())
    return file_url_factory_.get();
  return other_url_factory_.get();
}

void XmlDownloader::ParseXml(RulesetSource* source,
                             std::unique_ptr<std::string> bytes) {
  if (!bytes) {
    DoneParsing(source, ParsedXml({}, {}, "could not fetch XML"));
    return;
  }
  ParseIeemXml(
      *bytes, service_->prefs().GetParsingMode(),
      base::BindOnce(&XmlDownloader::DoneParsing,
                     weak_ptr_factory_.GetWeakPtr(), base::Unretained(source)));
}

void XmlDownloader::DoneParsing(RulesetSource* source, ParsedXml xml) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Special processing for "greylist" XML.
  if (source->contains_inverted_rules) {
    // BrowserSwitcherExternalGreylistUrl is special: all the rules are part of
    // the greylist, regardless of what <open-in> says in the XML.
    //
    // Merge all the rules into |greylist|, and clear |sitelist|.
    xml.rules.greylist.insert(xml.rules.greylist.end(),
                              xml.rules.sitelist.begin(),
                              xml.rules.sitelist.end());
    xml.rules.sitelist.clear();

    // Greylists can't contain any negative rules either, so remove the leading
    // "!".
    for (auto& rule : xml.rules.greylist) {
      if (base::StartsWith(rule, "!", base::CompareCase::SENSITIVE))
        rule.erase(0, 1);
    }
  }

  if (xml.error)
    LOG(ERROR) << *xml.error;
  std::move(source->parsed_callback).Run(std::move(xml));

  // Run the "all done" callback if this was the last ruleset.
  counter_++;
  DCHECK(counter_ <= sources_.size());
  if (counter_ == sources_.size()) {
    all_done_callback_.Run();
    if (HasValidSources())
      last_refresh_time_ = base::Time::Now();
    ScheduleRefresh(service_->refresh_delay());
  }
}

void XmlDownloader::ScheduleRefresh(base::TimeDelta delay) {
  // Avoid doing unnecessary work.
  if (!HasValidSources())
    return;

  // Refresh in 30 minutes, so the sitelists are never too stale.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&XmlDownloader::Refresh, weak_ptr_factory_.GetWeakPtr()),
      delay);
  next_refresh_time_ = base::Time::Now() + delay;
}

void XmlDownloader::Refresh() {
  sources_ = service_->GetRulesetSources();
  counter_ = 0;
  FetchXml();
}

BrowserSwitcherService::BrowserSwitcherService(Profile* profile)
    : profile_(profile),
      prefs_(profile),
      driver_(new AlternativeBrowserDriverImpl(&prefs_)),
      sitelist_(new BrowserSwitcherSitelistImpl(&prefs_)) {
  prefs_subscription_ =
      prefs().RegisterPrefsChangedCallback(base::BindRepeating(
          &BrowserSwitcherService::OnBrowserSwitcherPrefsChanged,
          base::Unretained(this)));

  if (prefs_.IsEnabled()) {
    UMA_HISTOGRAM_ENUMERATION("BrowserSwitcher.AlternativeBrowser",
                              driver_->GetBrowserType());
  }
}

BrowserSwitcherService::~BrowserSwitcherService() = default;

void BrowserSwitcherService::Init() {
  LoadRulesFromPrefs();
  StartDownload(fetch_delay());
}

void BrowserSwitcherService::OnAllRulesetsLoadedForTesting(
    base::OnceCallback<void()> cb) {
  all_rulesets_loaded_callback_for_testing_ = std::move(cb);
}

void BrowserSwitcherService::StartDownload(base::TimeDelta delay) {
  // This destroys the previous XmlDownloader, which cancels any scheduled
  // refresh operations.
  sitelist_downloader_ = std::make_unique<XmlDownloader>(
      profile_, this, delay,
      base::BindRepeating(&BrowserSwitcherService::OnAllRulesetsParsed,
                          base::Unretained(this)));
}

void BrowserSwitcherService::Shutdown() {
  prefs_.Shutdown();
}

AlternativeBrowserDriver* BrowserSwitcherService::driver() {
  return driver_.get();
}

BrowserSwitcherSitelist* BrowserSwitcherService::sitelist() {
  return sitelist_.get();
}

BrowserSwitcherPrefs& BrowserSwitcherService::prefs() {
  return prefs_;
}

Profile* BrowserSwitcherService::profile() {
  return profile_;
}

XmlDownloader* BrowserSwitcherService::sitelist_downloader() {
  return sitelist_downloader_.get();
}

base::TimeDelta BrowserSwitcherService::fetch_delay() {
  return fetch_delay_;
}

base::TimeDelta BrowserSwitcherService::refresh_delay() {
  return refresh_delay_;
}

void BrowserSwitcherService::SetDriverForTesting(
    std::unique_ptr<AlternativeBrowserDriver> driver) {
  driver_ = std::move(driver);
}

void BrowserSwitcherService::SetSitelistForTesting(
    std::unique_ptr<BrowserSwitcherSitelist> sitelist) {
  sitelist_ = std::move(sitelist);
}

std::vector<RulesetSource> BrowserSwitcherService::GetRulesetSources() {
  std::vector<RulesetSource> sources;

  GURL sitelist_url = prefs_.GetExternalSitelistUrl();
  sources.emplace_back(
      prefs::kExternalSitelistUrl, sitelist_url, /* invert_rules */ false,
      base::BindOnce(&BrowserSwitcherService::OnExternalSitelistParsed,
                     weak_ptr_factory_.GetWeakPtr()));

  GURL greylist_url = prefs_.GetExternalGreylistUrl();
  sources.emplace_back(
      prefs::kExternalGreylistUrl, greylist_url, /* invert_rules */ true,
      base::BindOnce(&BrowserSwitcherService::OnExternalGreylistParsed,
                     weak_ptr_factory_.GetWeakPtr()));

  return sources;
}

void BrowserSwitcherService::LoadRulesFromPrefs() {
  if (prefs().GetExternalSitelistUrl().is_valid())
    sitelist()->SetExternalSitelist(prefs().GetCachedExternalSitelist());
  if (prefs().GetExternalGreylistUrl().is_valid())
    sitelist()->SetExternalGreylist(prefs().GetCachedExternalGreylist());
}

void BrowserSwitcherService::OnAllRulesetsParsed() {
  callback_list_.Notify(this);
  if (all_rulesets_loaded_callback_for_testing_)
    std::move(all_rulesets_loaded_callback_for_testing_).Run();
}

base::CallbackListSubscription
BrowserSwitcherService::RegisterAllRulesetsParsedCallback(
    AllRulesetsParsedCallback callback) {
  return callback_list_.Add(callback);
}

void BrowserSwitcherService::OnBrowserSwitcherPrefsChanged(
    BrowserSwitcherPrefs* prefs,
    const std::vector<std::string>& changed_prefs) {
  // Record |BrowserSwitcher.AlternativeBrowser| when the
  // |BrowserSwitcherEnabled| or |AlternativeBrowserPath| policies change.
  bool should_record_metrics =
      base::ranges::any_of(changed_prefs, [](const std::string& pref) {
        return pref == prefs::kEnabled ||
               pref == prefs::kAlternativeBrowserPath;
      });
  if (should_record_metrics && prefs_.IsEnabled()) {
    UMA_HISTOGRAM_ENUMERATION("BrowserSwitcher.AlternativeBrowser",
                              driver_->GetBrowserType());
  }

  auto sources = GetRulesetSources();

  // Re-download if one of the URLs or the ParsingMode changed. O(n^2), but n<=3
  // so it's fast.
  auto it = base::ranges::find(changed_prefs, prefs::kParsingMode);
  bool parsing_mode_changed = it != changed_prefs.end();
  bool should_redownload =
      parsing_mode_changed ||
      base::ranges::any_of(
          sources,
          [&changed_prefs](const std::string& pref_name) {
            auto it = base::ranges::find(changed_prefs, pref_name);
            return it != changed_prefs.end();
          },
          &RulesetSource::pref_name);

  if (should_redownload)
    StartDownload(fetch_delay());
}

void BrowserSwitcherService::OnExternalSitelistParsed(ParsedXml xml) {
  if (xml.error) {
    SYSLOG(INFO) << "Unable to parse IEEM SiteList: " << *xml.error;
  } else {
    VLOG(2) << "Done parsing external SiteList for sitelist rules. "
            << "Applying rules to future navigations.";

    if (prefs().GetExternalSitelistUrl().is_valid())
      prefs().SetCachedExternalSitelist(xml.rules);

    sitelist()->SetExternalSitelist(std::move(xml.rules));
  }
}

void BrowserSwitcherService::OnExternalGreylistParsed(ParsedXml xml) {
  if (xml.error) {
    SYSLOG(INFO) << "Unable to parse IEEM SiteList: " << *xml.error;
  } else {
    VLOG(2) << "Done parsing external SiteList for greylist rules. "
            << "Applying rules to future navigations.";
    DCHECK(xml.rules.sitelist.empty());

    if (prefs().GetExternalGreylistUrl().is_valid()) {
      prefs().SetCachedExternalGreylist(xml.rules);
    }

    sitelist()->SetExternalGreylist(std::move(xml.rules));
  }
}

base::TimeDelta BrowserSwitcherService::fetch_delay_ = kFetchSitelistDelay;
base::TimeDelta BrowserSwitcherService::refresh_delay_ = kRefreshSitelistDelay;

// static
void BrowserSwitcherService::SetFetchDelayForTesting(base::TimeDelta delay) {
  fetch_delay_ = delay;
}

// static
void BrowserSwitcherService::SetRefreshDelayForTesting(base::TimeDelta delay) {
  refresh_delay_ = delay;
}

}  // namespace browser_switcher
