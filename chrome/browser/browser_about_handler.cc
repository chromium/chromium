// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_about_handler.h"

#include <string>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/policy/chrome_policy_blocklist_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "components/policy/core/browser/url_list/policy_blocklist_service.h"
#include "components/policy/core/browser/url_list/url_blocklist_manager.h"
#include "content/public/common/content_features.h"
#include "extensions/buildflags/buildflags.h"

namespace {

bool IsNonNavigationAboutUrl(const GURL& url) {
  if (!url.is_valid()) {
    return false;
  }

  const std::string spec(url.spec());
  return base::EqualsCaseInsensitiveASCII(spec, chrome::kChromeUIRestartURL) ||
         base::EqualsCaseInsensitiveASCII(spec, chrome::kChromeUIQuitURL);
  ;
}

}  // namespace

bool HandleChromeAboutAndChromeSyncRewrite(
    GURL* url,
    content::BrowserContext* browser_context) {
  // Check that about: URLs are either
  // 1) fixed up to chrome: (by url_formatter::FixupURL applied to
  //    browser-initiated navigations)
  // or
  // 2) blocked (by content::RenderProcessHostImpl::FilterURL applied to
  //    renderer-initiated navigations)
  DCHECK(url->IsAboutBlank() || url->IsAboutSrcdoc() ||
         !url->SchemeIs(url::kAboutScheme));

  // Only handle chrome: URLs.
  if (!url->SchemeIs(content::kChromeUIScheme)) {
    return false;
  }

  std::string host(url->GetHost());
  if (host == chrome::kChromeUIAboutHost) {
    // Replace chrome://about with chrome://chrome-urls.
    host = chrome::kChromeUIChromeURLsHost;
  }

  if (host != url->GetHost()) {
    GURL::Replacements replacements;
    replacements.SetHostStr(host);
    *url = url->ReplaceComponents(replacements);
  }

  // Having re-written the URL, make the chrome: handler process it.
  return false;
}

bool HandleNonNavigationAboutURL(const GURL& url,
                                 content::BrowserContext* context) {
  if (!IsNonNavigationAboutUrl(url)) {
    return false;
  }

  // TODO(crbug.com/418187845): Remove this check once Android is supported.
  if (context) {
    PolicyBlocklistService* service =
        ChromePolicyBlocklistServiceFactory::GetForProfile(
            Profile::FromBrowserContext(context));
    using URLBlocklistState = policy::URLBlocklist::URLBlocklistState;
    if (service->GetURLBlocklistState(url) ==
        URLBlocklistState::URL_IN_BLOCKLIST) {
      return true;
    }
  }

  const std::string spec(url.spec());
  if (base::EqualsCaseInsensitiveASCII(spec, chrome::kChromeUIRestartURL)) {
    // Call AttemptRestart after chrome::Navigate() completes to avoid access of
    // gtk objects after they are destroyed by BrowserWindowGtk::Close().
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&chrome::AttemptRestart));
    return true;
  }
  if (base::EqualsCaseInsensitiveASCII(spec, chrome::kChromeUIQuitURL)) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&chrome::AttemptExit));
    return true;
  }
  NOTREACHED();
}
