// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/browser_switcher_service.h"

#include "build/build_config.h"
#include "chrome/browser/browser_switcher/alternative_browser_launcher.h"
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#include "chrome/browser/browser_switcher/browser_switcher_sitelist.h"
#include "chrome/browser/browser_switcher/ieem_sitelist_parser.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/simple_url_loader.h"

#if defined(OS_WIN)
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#endif

namespace browser_switcher {

namespace {

#if defined(OS_WIN)
const wchar_t kIeSiteListKey[] =
    L"SOFTWARE\\Policies\\Microsoft\\Internet Explorer\\Main\\EnterpriseMode";
const wchar_t kIeSiteListValue[] = L"SiteList";

// How long to wait after |BrowserSwitcherService| is created before initiating
// the sitelist fetch.
const base::TimeDelta kFetchSitelistDelay = base::TimeDelta::FromSeconds(60);

// How many times to re-try fetching the XML file for the sitelist.
const int kFetchNumRetries = 1;

// TODO(nicolaso): Add chrome_policy for this annotation once the policy is
// implemented.
constexpr net::NetworkTrafficAnnotationTag traffic_annotation =
    net::DefineNetworkTrafficAnnotation("browser_switcher_ieem_sitelist", R"(
        semantics {
          sender: "Browser Switcher"
          description:
            "BrowserSwitcher may download Internet Explorer's Enterprise Mode "
            "SiteList XML, to load the list of URLs to open in an alternative "
            "browser. This is often on the organization's intranet. For more "
            "information on Internet Explorer's Enterprise Mode, see: "
            "https://docs.microsoft.com/internet-explorer/ie11-deploy-guide"
            "/what-is-enterprise-mode"
          trigger:
            "This happens only once per profile, 60s after the first page "
            "starts loading. The request may be retried once if it failed the "
            "first time."
          data:
            "An HTTP or HTTPS GET request to the URL configured in Internet "
            "Explorer's SiteList policy."
          destination: OTHER
          destination_other:
            "URL configured in Internet Explorer's SiteList policy."
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled by settings."
          policy_exception_justification:
            "This feature is still in development, and is disabled by default."
        })");
#endif

}  // namespace

BrowserSwitcherService::BrowserSwitcherService(Profile* profile)
    : launcher_(nullptr), sitelist_(nullptr), prefs_(profile->GetPrefs()) {
  DCHECK(profile);
  DCHECK(prefs_);
#if defined(OS_WIN)
  if (prefs_->GetBoolean(prefs::kUseIeSitelist)) {
    GURL sitelist_url = GetIeemSitelistUrl();
    if (sitelist_url.is_valid()) {
      auto factory =
          content::BrowserContext::GetDefaultStoragePartition(profile)
              ->GetURLLoaderFactoryForBrowserProcess();
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&BrowserSwitcherService::FetchIeemSitelist,
                         base::Unretained(this), std::move(sitelist_url),
                         std::move(factory)),
          fetch_sitelist_delay_);
    }
  } else {
    DoneLoadingIeemSitelist();
  }
#endif
}

BrowserSwitcherService::~BrowserSwitcherService() {}

AlternativeBrowserLauncher* BrowserSwitcherService::launcher() {
  if (!launcher_)
    launcher_ = std::make_unique<AlternativeBrowserLauncherImpl>(prefs_);
  return launcher_.get();
}

BrowserSwitcherSitelist* BrowserSwitcherService::sitelist() {
  if (!sitelist_)
    sitelist_ = std::make_unique<BrowserSwitcherSitelistImpl>(prefs_);
  return sitelist_.get();
}

void BrowserSwitcherService::SetLauncherForTesting(
    std::unique_ptr<AlternativeBrowserLauncher> launcher) {
  launcher_ = std::move(launcher);
}

void BrowserSwitcherService::SetSitelistForTesting(
    std::unique_ptr<BrowserSwitcherSitelist> sitelist) {
  sitelist_ = std::move(sitelist);
}

#if defined(OS_WIN)
base::TimeDelta BrowserSwitcherService::fetch_sitelist_delay_ =
    kFetchSitelistDelay;

// static
void BrowserSwitcherService::SetIeemFetchDelayForTesting(
    base::TimeDelta delay) {
  fetch_sitelist_delay_ = delay;
}

base::OnceCallback<void()>
    BrowserSwitcherService::xml_parsed_callback_for_testing_;

// static
void BrowserSwitcherService::SetXmlParsedCallbackForTesting(
    base::OnceCallback<void()> callback) {
  xml_parsed_callback_for_testing_ = std::move(callback);
}

GURL BrowserSwitcherService::ieem_sitelist_url_for_testing_;

// static
void BrowserSwitcherService::SetIeemSitelistUrlForTesting(const GURL& url) {
  ieem_sitelist_url_for_testing_ = url;
}

GURL BrowserSwitcherService::GetIeemSitelistUrl() {
  if (!ieem_sitelist_url_for_testing_.is_empty())
    return ieem_sitelist_url_for_testing_;

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

void BrowserSwitcherService::FetchIeemSitelist(
    GURL url,
    scoped_refptr<network::SharedURLLoaderFactory> factory) {
  DCHECK(factory);
  DCHECK(!url_loader_);
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url;
  request->load_flags =
      (net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES |
       net::LOAD_DISABLE_CACHE);
  url_loader_ =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  url_loader_->SetRetryOptions(
      kFetchNumRetries,
      network::SimpleURLLoader::RetryMode::RETRY_ON_NETWORK_CHANGE);
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      factory.get(), base::BindOnce(&BrowserSwitcherService::ParseXml,
                                    base::Unretained(this)));
}

void BrowserSwitcherService::ParseXml(std::unique_ptr<std::string> bytes) {
  if (!bytes) {
    DoneLoadingIeemSitelist();
    return;
  }
  ParseIeemXml(*bytes,
               base::BindOnce(&BrowserSwitcherService::OnIeemSitelistXmlParsed,
                              base::Unretained(this)));
}

void BrowserSwitcherService::OnIeemSitelistXmlParsed(ParsedXml xml) {
  if (xml.error) {
    LOG(ERROR) << "Unable to parse IEEM SiteList: " << *xml.error;
  } else {
    VLOG(2) << "Done parsing IEEM SiteList. "
            << "Applying rules to future navigations.";
    sitelist()->SetIeemSitelist(std::move(xml));
  }
  DoneLoadingIeemSitelist();
}

void BrowserSwitcherService::DoneLoadingIeemSitelist() {
  if (xml_parsed_callback_for_testing_)
    std::move(xml_parsed_callback_for_testing_).Run();
}
#endif

}  // namespace browser_switcher
