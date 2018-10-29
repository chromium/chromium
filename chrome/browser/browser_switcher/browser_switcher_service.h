// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_SERVICE_H_
#define CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_SERVICE_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

#include <memory>

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

class PrefService;
class Profile;

namespace browser_switcher {

class AlternativeBrowserLauncher;
class BrowserSwitcherSitelist;
class ParsedXml;

// Manages per-profile resources for BrowserSwitcher.
class BrowserSwitcherService : public KeyedService {
 public:
  explicit BrowserSwitcherService(Profile* profile);
  ~BrowserSwitcherService() override;

  AlternativeBrowserLauncher* launcher();
  BrowserSwitcherSitelist* sitelist();

  void SetLauncherForTesting(
      std::unique_ptr<AlternativeBrowserLauncher> launcher);
  void SetSitelistForTesting(std::unique_ptr<BrowserSwitcherSitelist> sitelist);

#if defined(OS_WIN)
  static void SetIeemFetchDelayForTesting(base::TimeDelta delay);
  static void SetXmlParsedCallbackForTesting(
      base::OnceCallback<void()> callback);
  static void SetIeemSitelistUrlForTesting(const GURL& url);
#endif

 private:
#if defined(OS_WIN)
  // Returns the URL to fetch to get Internet Explorer's Enterprise Mode
  // sitelist, based on policy. Returns an empty (invalid) URL if IE's SiteList
  // policy is unset.
  GURL GetIeemSitelistUrl();

  // Steps to process the IEEM sitelist rules: fetch, parse, apply.
  void FetchIeemSitelist(
      GURL url,
      scoped_refptr<network::SharedURLLoaderFactory> factory);
  void ParseXml(std::unique_ptr<std::string> bytes);
  void OnIeemSitelistXmlParsed(ParsedXml xml);
  void DoneLoadingIeemSitelist();

  // Delay for the IEEM XML fetch task, launched from the constructor.
  static base::TimeDelta fetch_sitelist_delay_;

  // URL to fetch the IEEM sitelist from. Only used for testing.
  static GURL ieem_sitelist_url_for_testing_;

  // If set, gets called once the IEEM sitelist rules are applied. Also gets
  // called if any step of the process fails.
  static base::OnceCallback<void()> xml_parsed_callback_for_testing_;

  // Used to fetch the IEEM XML.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
#endif

  // Per-profile helpers.
  std::unique_ptr<AlternativeBrowserLauncher> launcher_;
  std::unique_ptr<BrowserSwitcherSitelist> sitelist_;

  PrefService* const prefs_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(BrowserSwitcherService);
};

}  // namespace browser_switcher

#endif  // CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_SERVICE_H_
